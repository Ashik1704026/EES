#include "scheduler.h"

#include <algorithm>
#include <cmath>
#include <deque>
#include <limits>
#include <map>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

struct TraversalEvent {
    int group_id = 0;
    int people = 0;
    int edge_id = 0;
    int from_node = 0;
    int to_node = 0;
    int enter_time = 0;
    int leave_time = 0;
};

struct DynamicEdgeState {
    std::deque<TraversalEvent> active_traversals;
    int entered_this_second_total = 0;
    int current_occupancy_total = 0;
    int max_occupancy = 0;
    int total_entries = 0;
    int saturated_seconds = 0;
};

struct ExitQueueItem {
    int group_id = 0;
};

const Route* FindRoute(const std::vector<Route>& routes, int room_id, int exit_id) {
    for (const Route& route : routes) {
        if (route.room_id == room_id && route.exit_id == exit_id && route.reachable) {
            return &route;
        }
    }
    return nullptr;
}

const Node* FindNodeById(const Scenario& scenario, int node_id) {
    for (const Node& node : scenario.nodes) {
        if (node.id == node_id) {
            return &node;
        }
    }
    return nullptr;
}

const PhysicalEdge* FindEdgeById(const Scenario& scenario, int edge_id) {
    for (const PhysicalEdge& edge : scenario.edges) {
        if (edge.id == edge_id) {
            return &edge;
        }
    }
    return nullptr;
}

std::string NodeName(const Scenario& scenario, int node_id) {
    const Node* node = FindNodeById(scenario, node_id);
    return node != nullptr ? node->name : std::to_string(node_id);
}

std::vector<Group> GenerateGroups(const Scenario& scenario,
                                  const std::vector<Assignment>& assignments,
                                  const std::vector<Route>& routes,
                                  std::vector<GeneratedGroupSummary>& summaries) {
    std::vector<Group> groups;
    int next_group_id = 1;

    for (const Assignment& assignment : assignments) {
        if (assignment.assigned_people <= 0) {
            continue;
        }

        const Route* route = FindRoute(routes, assignment.room_id, assignment.exit_id);
        if (route == nullptr) {
            continue;
        }

        GeneratedGroupSummary summary;
        summary.room_id = assignment.room_id;
        summary.exit_id = assignment.exit_id;
        summary.assigned_people = assignment.assigned_people;

        int remaining = assignment.assigned_people;
        while (remaining > 0) {
            const int chunk = std::min(remaining, scenario.chunk_size);
            Group group;
            group.group_id = next_group_id++;
            group.room_id = assignment.room_id;
            group.exit_id = assignment.exit_id;
            group.size = chunk;
            group.remaining_people = chunk;
            group.route_edge_path = route->edge_path;
            group.route_node_path = route->node_path;
            group.current_node_id = assignment.room_id;
            group.last_queue_enter_time = 0;
            groups.push_back(group);

            summary.group_count += 1;
            summary.chunk_sizes.push_back(chunk);
            remaining -= chunk;
        }

        std::map<int, int, std::greater<int>> chunk_counts;
        for (int chunk_size : summary.chunk_sizes) {
            chunk_counts[chunk_size] += 1;
        }

        std::ostringstream summary_text;
        summary_text << summary.group_count << " groups";
        if (!chunk_counts.empty()) {
            summary_text << " (";
            bool first = true;
            for (const auto& entry : chunk_counts) {
                if (!first) {
                    summary_text << ", ";
                }
                summary_text << entry.second << " group";
                if (entry.second != 1) {
                    summary_text << "s";
                }
                summary_text << " of " << entry.first;
                first = false;
            }
            summary_text << ")";
        }
        summary.display_summary = summary_text.str();

        summaries.push_back(summary);
    }

    return groups;
}

}  // namespace

SimulationReport RunSimulation(const Scenario& scenario,
                               const std::vector<Assignment>& assignments,
                               const std::vector<Route>& routes) {
    SimulationReport report;
    report.total_initial_people = 0;
    for (const Node& node : scenario.nodes) {
        if (node.type == NodeType::ROOM && node.active) {
            report.total_initial_people += node.initial_people;
        }
        if (node.type == NodeType::EXIT && node.active) {
            report.peak_exit_queue_lengths[node.id] = 0;
        }
    }

    report.generated_groups = GenerateGroups(scenario, assignments, routes, report.group_summaries);
    report.total_groups_generated = static_cast<int>(report.generated_groups.size());

    std::unordered_map<int, Group*> group_by_id;
    for (Group& group : report.generated_groups) {
        group_by_id[group.group_id] = &group;
    }

    std::unordered_map<int, std::vector<int>> node_waiting_groups;
    std::unordered_map<int, std::deque<ExitQueueItem>> exit_queues;
    std::unordered_map<int, DynamicEdgeState> edge_states;
    for (const PhysicalEdge& edge : scenario.edges) {
        edge_states[edge.id] = DynamicEdgeState{};
    }

    for (Group& group : report.generated_groups) {
        node_waiting_groups[group.current_node_id].push_back(group.group_id);
    }

    int consecutive_no_progress = 0;
    report.stuck_threshold_seconds = std::max(30, report.total_groups_generated * 2);
    const int hard_limit_seconds = std::max(600, report.total_groups_generated * 20);

    auto push_timeline = [&](const std::string& text) {
        report.timeline_events.push_back(text);
    };

    for (int t = 0; t <= hard_limit_seconds; ++t) {
        bool progress_this_second = false;

        for (auto& edge_entry : edge_states) {
            DynamicEdgeState& state = edge_entry.second;
            while (!state.active_traversals.empty() &&
                   state.active_traversals.front().leave_time == t) {
                const TraversalEvent event = state.active_traversals.front();
                state.active_traversals.pop_front();
                state.current_occupancy_total -= event.people;
                Group* group = group_by_id[event.group_id];
                if (group == nullptr) {
                    continue;
                }
                group->current_node_id = event.to_node;
                group->current_step_index += 1;
                group->ready_time = t;
                group->last_queue_enter_time = t;

                push_timeline("t=" + std::to_string(t) + " | Group G" +
                              std::to_string(group->group_id) + " arrived at " +
                              NodeName(scenario, event.to_node));

                const Node* destination = FindNodeById(scenario, event.to_node);
                if (destination != nullptr && destination->type == NodeType::EXIT) {
                    exit_queues[event.to_node].push_back({group->group_id});
                    const int queue_size = [&]() {
                        int total = 0;
                        for (const ExitQueueItem& item : exit_queues[event.to_node]) {
                            const Group* queued = group_by_id[item.group_id];
                            if (queued != nullptr) {
                                total += queued->remaining_people;
                            }
                        }
                        return total;
                    }();
                    report.peak_exit_queue_lengths[event.to_node] =
                        std::max(report.peak_exit_queue_lengths[event.to_node], queue_size);
                    push_timeline("t=" + std::to_string(t) + " | Group G" +
                                  std::to_string(group->group_id) + " joined exit queue " +
                                  NodeName(scenario, event.to_node));
                } else {
                    node_waiting_groups[event.to_node].push_back(group->group_id);
                }
                progress_this_second = true;
            }
        }

        for (const Node& node : scenario.nodes) {
            if (node.type != NodeType::EXIT || !node.active) {
                continue;
            }
            std::deque<ExitQueueItem>& queue = exit_queues[node.id];
            int served = 0;
            int capacity = node.exit_service_rate;
            while (capacity > 0 && !queue.empty()) {
                Group* group = group_by_id[queue.front().group_id];
                if (group == nullptr) {
                    queue.pop_front();
                    continue;
                }

                const int evacuating = std::min(capacity, group->remaining_people);
                group->remaining_people -= evacuating;
                capacity -= evacuating;
                served += evacuating;
                report.total_evacuated_people += evacuating;
                report.average_completion_time_per_person += static_cast<double>(evacuating) * t;
                report.maximum_completion_time = std::max(report.maximum_completion_time, t);

                if (group->remaining_people == 0) {
                    group->finished = true;
                    group->completion_time = t;
                    queue.pop_front();
                }
                progress_this_second = progress_this_second || (evacuating > 0);
            }

            int queue_remaining = 0;
            for (const ExitQueueItem& item : queue) {
                const Group* queued = group_by_id[item.group_id];
                if (queued != nullptr) {
                    queue_remaining += queued->remaining_people;
                }
            }
            report.peak_exit_queue_lengths[node.id] =
                std::max(report.peak_exit_queue_lengths[node.id], queue_remaining);

            if (served > 0) {
                push_timeline("t=" + std::to_string(t) + " | Exit " + node.name +
                              " evacuated " + std::to_string(served) + " people");
                report.exit_service_log.push_back({t, node.id, served, queue_remaining});
            }
        }

        for (const Node& node : scenario.nodes) {
            if (!node.active || node.type == NodeType::EXIT) {
                continue;
            }
            std::vector<int>& waiting = node_waiting_groups[node.id];
            if (waiting.empty()) {
                continue;
            }

            std::sort(waiting.begin(), waiting.end(), [&](int left_id, int right_id) {
                const Group* left = group_by_id[left_id];
                const Group* right = group_by_id[right_id];
                const int left_priority = node.priority;
                const int right_priority = node.priority;
                if (left_priority != right_priority) {
                    return left_priority > right_priority;
                }
                if (left->ready_time != right->ready_time) {
                    return left->ready_time < right->ready_time;
                }
                const int left_remaining_steps =
                    static_cast<int>(left->route_edge_path.size()) - left->current_step_index;
                const int right_remaining_steps =
                    static_cast<int>(right->route_edge_path.size()) - right->current_step_index;
                if (left_remaining_steps != right_remaining_steps) {
                    return left_remaining_steps < right_remaining_steps;
                }
                return left->group_id < right->group_id;
            });

            std::vector<int> still_waiting;
            for (int group_id : waiting) {
                Group* group = group_by_id[group_id];
                if (group == nullptr || group->finished) {
                    continue;
                }
                if (group->current_step_index >= static_cast<int>(group->route_edge_path.size())) {
                    still_waiting.push_back(group_id);
                    continue;
                }

                const int edge_id = group->route_edge_path[group->current_step_index];
                const PhysicalEdge* edge = FindEdgeById(scenario, edge_id);
                if (edge == nullptr || !edge->active) {
                    still_waiting.push_back(group_id);
                    continue;
                }

                const int next_node = group->route_node_path[group->current_step_index + 1];
                DynamicEdgeState& edge_state = edge_states[edge_id];
                const bool can_enter =
                    edge_state.entered_this_second_total + group->remaining_people <=
                        edge->entry_capacity_per_sec &&
                    edge_state.current_occupancy_total + group->remaining_people <=
                        edge->occupancy_limit;

                if (!can_enter) {
                    still_waiting.push_back(group_id);
                    continue;
                }

                const int waited = t - group->last_queue_enter_time;
                group->total_wait_time += waited;
                group->max_single_wait = std::max(group->max_single_wait, waited);
                report.max_waiting_time_anywhere =
                    std::max(report.max_waiting_time_anywhere, group->max_single_wait);

                TraversalEvent event;
                event.group_id = group->group_id;
                event.people = group->remaining_people;
                event.edge_id = edge_id;
                event.from_node = group->current_node_id;
                event.to_node = next_node;
                event.enter_time = t;
                event.leave_time = t + edge->travel_time_sec;
                edge_state.active_traversals.push_back(event);
                edge_state.entered_this_second_total += group->remaining_people;
                edge_state.current_occupancy_total += group->remaining_people;
                edge_state.max_occupancy =
                    std::max(edge_state.max_occupancy, edge_state.current_occupancy_total);
                edge_state.total_entries += group->remaining_people;
                report.peak_edge_occupancies[edge_id] = edge_state.max_occupancy;

                push_timeline("t=" + std::to_string(t) + " | Group G" +
                              std::to_string(group->group_id) + " size=" +
                              std::to_string(group->remaining_people) + " entered edge " +
                              NodeName(scenario, event.from_node) + "->" +
                              NodeName(scenario, event.to_node));
                progress_this_second = true;
            }
            waiting = still_waiting;
        }

        for (const PhysicalEdge& edge : scenario.edges) {
            DynamicEdgeState& state = edge_states[edge.id];
            if (state.entered_this_second_total >= edge.entry_capacity_per_sec ||
                state.current_occupancy_total >= edge.occupancy_limit) {
                state.saturated_seconds += 1;
            }
            state.entered_this_second_total = 0;
        }

        bool all_finished = true;
        for (const Group& group : report.generated_groups) {
            if (!group.finished) {
                all_finished = false;
                break;
            }
        }
        if (all_finished) {
            report.completed = true;
            report.total_evacuation_time = t;
            break;
        }

        if (progress_this_second) {
            consecutive_no_progress = 0;
        } else {
            consecutive_no_progress += 1;
        }

        if (consecutive_no_progress >= report.stuck_threshold_seconds) {
            report.stuck = true;
            report.total_evacuation_time = t;
            break;
        }
    }

    if (report.total_evacuated_people > 0) {
        report.average_completion_time_per_person /=
            static_cast<double>(report.total_evacuated_people);
    }

    for (const PhysicalEdge& edge : scenario.edges) {
        DynamicEdgeState& state = edge_states[edge.id];
        EdgeUsageSummary summary;
        summary.edge_id = edge.id;
        summary.max_occupancy = state.max_occupancy;
        summary.total_entries = state.total_entries;
        summary.saturated_seconds = state.saturated_seconds;
        if (report.total_evacuation_time > 0 && edge.entry_capacity_per_sec > 0) {
            summary.utilization_percent =
                100.0 * static_cast<double>(state.total_entries) /
                static_cast<double>(edge.entry_capacity_per_sec * report.total_evacuation_time);
        }
        report.edge_usage.push_back(summary);
    }
    std::sort(report.edge_usage.begin(), report.edge_usage.end(),
              [](const EdgeUsageSummary& left, const EdgeUsageSummary& right) {
                  return left.edge_id < right.edge_id;
              });

    for (const Node& room : scenario.nodes) {
        if (room.type != NodeType::ROOM || !room.active) {
            continue;
        }

        RoomWaitSummary summary;
        summary.room_id = room.id;
        summary.initial_people = room.initial_people;

        double weighted_wait_sum = 0.0;
        int weighted_people = 0;
        for (const Assignment& assignment : assignments) {
            if (assignment.room_id == room.id) {
                summary.assigned_by_exit[assignment.exit_id] += assignment.assigned_people;
            }
        }

        for (const Group& group : report.generated_groups) {
            if (group.room_id != room.id) {
                continue;
            }
            summary.max_wait_time = std::max(summary.max_wait_time, group.max_single_wait);
            weighted_wait_sum += static_cast<double>(group.total_wait_time * group.size);
            weighted_people += group.size;
        }

        if (weighted_people > 0) {
            summary.average_wait_time = weighted_wait_sum / weighted_people;
        }
        report.room_waits.push_back(summary);
    }
    std::sort(report.room_waits.begin(), report.room_waits.end(),
              [](const RoomWaitSummary& left, const RoomWaitSummary& right) {
                  return left.room_id < right.room_id;
              });

    if (report.completed && !report.stuck &&
        report.total_evacuated_people == report.total_initial_people) {
        report.status_message = "Phase 4 complete: dynamic simulation finished successfully.";
    } else if (report.stuck) {
        report.status_message = "Phase 4 complete: simulation detected as stuck.";
    } else {
        report.status_message = "Phase 4 complete: simulation ended before full evacuation.";
    }
    return report;
}

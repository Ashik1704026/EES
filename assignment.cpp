#include "assignment.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

struct CandidateAssignment {
    int room_id = 0;
    int exit_id = 0;
    double cost = 0.0;
};

std::vector<const Node*> CollectRooms(const Scenario& scenario) {
    std::vector<const Node*> rooms;
    for (const Node& node : scenario.nodes) {
        if (node.active && node.type == NodeType::ROOM) {
            rooms.push_back(&node);
        }
    }
    return rooms;
}

std::vector<const Node*> CollectExits(const Scenario& scenario) {
    std::vector<const Node*> exits;
    for (const Node& node : scenario.nodes) {
        if (node.active && node.type == NodeType::EXIT) {
            exits.push_back(&node);
        }
    }
    return exits;
}

std::map<int, int> ComputeExitQuotas(const Scenario& scenario, int total_people) {
    std::vector<const Node*> exits = CollectExits(scenario);
    std::map<int, int> quotas;
    if (exits.empty()) {
        return quotas;
    }

    int total_service_rate = 0;
    for (const Node* exit : exits) {
        total_service_rate += exit->exit_service_rate;
    }
    if (total_service_rate <= 0) {
        for (const Node* exit : exits) {
            quotas[exit->id] = 0;
        }
        return quotas;
    }

    int running_total = 0;
    for (std::size_t i = 0; i < exits.size(); ++i) {
        const Node* exit = exits[i];
        int quota = static_cast<int>(std::llround(
            static_cast<double>(total_people) * exit->exit_service_rate / total_service_rate));
        quotas[exit->id] = quota;
        running_total += quota;
    }

    if (!exits.empty()) {
        quotas[exits.back()->id] += (total_people - running_total);
    }
    return quotas;
}

}  // namespace

AssignmentResult ComputeAssignments(const Scenario& scenario, const std::vector<Route>& routes) {
    AssignmentResult result;
    const std::vector<const Node*> rooms = CollectRooms(scenario);
    const std::vector<const Node*> exits = CollectExits(scenario);

    for (const Node* room : rooms) {
        result.total_people += room->initial_people;
    }
    result.exit_quotas = ComputeExitQuotas(scenario, result.total_people);
    for (const Node* exit : exits) {
        result.exit_loads[exit->id] = 0;
    }

    std::vector<CandidateAssignment> candidates;
    for (const Route& route : routes) {
        if (route.reachable) {
            candidates.push_back({route.room_id, route.exit_id, route.static_cost});
        }
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const CandidateAssignment& left, const CandidateAssignment& right) {
                  if (left.cost != right.cost) {
                      return left.cost < right.cost;
                  }
                  if (left.room_id != right.room_id) {
                      return left.room_id < right.room_id;
                  }
                  return left.exit_id < right.exit_id;
              });

    std::unordered_map<int, int> remaining_room_people;
    for (const Node* room : rooms) {
        remaining_room_people[room->id] = room->initial_people;
    }

    std::map<std::pair<int, int>, int> assigned_by_pair;
    for (const CandidateAssignment& candidate : candidates) {
        int& remaining_people = remaining_room_people[candidate.room_id];
        int& exit_load = result.exit_loads[candidate.exit_id];
        const int exit_quota = result.exit_quotas[candidate.exit_id];
        const int exit_remaining = exit_quota - exit_load;
        if (remaining_people <= 0 || exit_remaining <= 0) {
            continue;
        }

        const int assigned = std::min(remaining_people, exit_remaining);
        assigned_by_pair[{candidate.room_id, candidate.exit_id}] += assigned;
        remaining_people -= assigned;
        exit_load += assigned;
        result.total_assigned += assigned;
    }

    for (const Node* room : rooms) {
        if (remaining_room_people[room->id] <= 0) {
            continue;
        }

        for (const CandidateAssignment& candidate : candidates) {
            if (candidate.room_id != room->id) {
                continue;
            }

            const int assigned = remaining_room_people[room->id];
            if (assigned <= 0) {
                break;
            }

            assigned_by_pair[{candidate.room_id, candidate.exit_id}] += assigned;
            remaining_room_people[room->id] = 0;
            result.exit_loads[candidate.exit_id] += assigned;
            result.total_assigned += assigned;
            break;
        }
    }

    for (const auto& entry : assigned_by_pair) {
        if (entry.second <= 0) {
            continue;
        }
        result.assignments.push_back(
            {entry.first.first, entry.first.second, entry.second});
    }

    std::sort(result.assignments.begin(), result.assignments.end(),
              [](const Assignment& left, const Assignment& right) {
                  if (left.room_id != right.room_id) {
                      return left.room_id < right.room_id;
                  }
                  return left.exit_id < right.exit_id;
              });

    result.unassigned_people = result.total_people - result.total_assigned;
    result.success = (result.unassigned_people == 0);
    if (result.success) {
        result.status_message = "Phase 3 complete: greedy assignment computed successfully.";
    } else {
        result.status_message = "Phase 3 complete with unassigned people remaining.";
    }
    return result;
}

#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include "assignment.h"
#include "dijkstra.h"
#include "graph.h"
#include "scheduler.h"

namespace {

std::string FormatCost(double cost) {
    if (cost == std::numeric_limits<double>::infinity()) {
        return "INF";
    }

    std::ostringstream stream;
    if (std::abs(cost - std::round(cost)) < 1e-9) {
        stream << static_cast<long long>(std::llround(cost));
    } else {
        stream << std::fixed << std::setprecision(2) << cost;
    }
    return stream.str();
}

std::string FormatFixed(double value, int precision = 1) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(precision) << value;
    return stream.str();
}

std::string JoinNodePath(const Graph& graph, const std::vector<int>& node_ids) {
    if (node_ids.empty()) {
        return "-";
    }

    std::ostringstream stream;
    for (std::size_t i = 0; i < node_ids.size(); ++i) {
        const Node* node = graph.FindNode(node_ids[i]);
        stream << (node != nullptr ? node->name : std::to_string(node_ids[i]));
        if (i + 1 < node_ids.size()) {
            stream << " -> ";
        }
    }
    return stream.str();
}

std::string JoinEdgePath(const std::vector<int>& edge_ids) {
    if (edge_ids.empty()) {
        return "-";
    }

    std::ostringstream stream;
    for (std::size_t i = 0; i < edge_ids.size(); ++i) {
        stream << edge_ids[i];
        if (i + 1 < edge_ids.size()) {
            stream << ", ";
        }
    }
    return stream.str();
}

bool AllRoomsReachAnExit(const Scenario& scenario, const RoutingResult& routing) {
    for (const Node& node : scenario.nodes) {
        if (!node.active || node.type != NodeType::ROOM) {
            continue;
        }

        bool has_reachable_exit = false;
        for (const Route& route : routing.routes) {
            if (route.room_id == node.id && route.reachable) {
                has_reachable_exit = true;
                break;
            }
        }

        if (!has_reachable_exit) {
            return false;
        }
    }
    return true;
}

int FindAssignedPeople(const AssignmentResult& assignment_result, int room_id, int exit_id) {
    for (const Assignment& assignment : assignment_result.assignments) {
        if (assignment.room_id == room_id && assignment.exit_id == exit_id) {
            return assignment.assigned_people;
        }
    }
    return 0;
}

std::string BuildAssignedSplit(const Graph& graph, const RoomWaitSummary& wait_summary) {
    std::ostringstream out;
    bool first = true;
    for (const auto& entry : wait_summary.assigned_by_exit) {
        const Node* exit_node = graph.FindNode(entry.first);
        if (!first) {
            out << ", ";
        }
        out << (exit_node != nullptr ? exit_node->name : std::to_string(entry.first))
            << "=" << entry.second;
        first = false;
    }
    return first ? "-" : out.str();
}

void WriteFullReport(const Graph& graph,
                     const ValidationResult& validation,
                     const RoutingResult& routing,
                     const AssignmentResult& assignment_result,
                     const SimulationReport& simulation_report,
                     const std::string& output_path) {
    const Scenario& scenario = graph.GetScenario();
    std::ofstream output(output_path);
    if (!output) {
        throw std::runtime_error("Failed to open output file: " + output_path);
    }

    output << "============================================================\n";
    output << "INTELLIGENT EMERGENCY EVACUATION PLANNING SYSTEM\n";
    output << "============================================================\n\n";
    output << "ACTIVE SCENARIO: " << (scenario.name.empty() ? "DEFAULT" : scenario.name) << "\n";
    output << "SIMULATION STATUS: "
           << (simulation_report.completed ? "COMPLETED" :
               (simulation_report.stuck ? "STUCK" : "INCOMPLETE"))
           << "\n\n";

    output << "------------------------------------------------------------\n";
    output << "1. SCENARIO SUMMARY\n";
    output << "------------------------------------------------------------\n";
    output << "Time Step (sec): " << scenario.time_step_sec << "\n";
    output << "Chunk Size: " << scenario.chunk_size << "\n\n";
    output << "Total Nodes: " << scenario.nodes.size() << "\n";
    output << "Total Edges: " << scenario.edges.size() << "\n";
    output << "Total Rooms: " << CountNodesOfType(scenario, NodeType::ROOM) << "\n";
    output << "Total Junctions: " << CountNodesOfType(scenario, NodeType::JUNCTION) << "\n";
    output << "Total Exits: " << CountNodesOfType(scenario, NodeType::EXIT) << "\n";
    output << "Total Initial People: " << TotalInitialPeople(scenario) << "\n\n";

    output << "Exit Information:\n";
    for (const Node& node : scenario.nodes) {
        if (node.type == NodeType::EXIT) {
            output << "  " << node.name << " -> service rate = "
                   << node.exit_service_rate << " people/sec\n";
        }
    }

    output << "\nNode List:\n";
    for (const Node& node : scenario.nodes) {
        output << "  [" << node.id << "] "
               << std::left << std::setw(9) << NodeTypeToString(node.type)
               << " " << std::setw(4) << node.name;
        if (node.type == NodeType::ROOM) {
            output << " people=" << node.initial_people;
        } else if (node.type == NodeType::EXIT) {
            output << " rate=" << node.exit_service_rate;
        }
        output << "\n";
    }

    output << "\nEdge List:\n";
    for (const PhysicalEdge& edge : scenario.edges) {
        const Node* a = graph.FindNode(edge.a);
        const Node* b = graph.FindNode(edge.b);
        output << "  [" << edge.id << "] "
               << (a != nullptr ? a->name : std::to_string(edge.a))
               << " <-> "
               << (b != nullptr ? b->name : std::to_string(edge.b))
               << "   length=" << edge.length_m
               << "  speed=" << edge.speed_mps
               << "  travel_time=" << edge.travel_time_sec
               << "  entry_cap=" << edge.entry_capacity_per_sec
               << "  occ_limit=" << edge.occupancy_limit
               << "  hazard=" << edge.hazard_penalty
               << "  active=" << (edge.active ? 1 : 0) << "\n";
    }

    output << "\n------------------------------------------------------------\n";
    output << "2. VALIDATION RESULTS\n";
    output << "------------------------------------------------------------\n";
    output << "Graph Loaded Successfully: " << (validation.ok ? "YES" : "NO") << "\n";
    output << "All Active Edges Valid: " << (validation.errors.empty() ? "YES" : "NO") << "\n";
    output << "All Exit Rates Valid: " << (validation.errors.empty() ? "YES" : "NO") << "\n";
    output << "All Rooms Have Nonnegative Population: " << (validation.errors.empty() ? "YES" : "NO")
           << "\n";
    output << "All Rooms Reach At Least One Exit: "
           << (AllRoomsReachAnExit(scenario, routing) ? "YES" : "NO") << "\n";
    if (validation.errors.empty() && validation.warnings.empty()) {
        output << "Warnings: NONE\n";
    } else {
        if (!validation.errors.empty()) {
            output << "Errors:\n";
            for (const std::string& error : validation.errors) {
                output << "  - " << error << "\n";
            }
        }
        if (!validation.warnings.empty()) {
            output << "Warnings:\n";
            for (const std::string& warning : validation.warnings) {
                output << "  - " << warning << "\n";
            }
        }
    }

    output << "\n------------------------------------------------------------\n";
    output << "3. DIJKSTRA ROUTE TABLE\n";
    output << "------------------------------------------------------------\n";
    output << "Format:\n";
    output << "Room -> Exit | Reachable | Cost | Node Path | Edge Path\n\n";
    for (const Route& route : routing.routes) {
        const Node* room = graph.FindNode(route.room_id);
        const Node* exit = graph.FindNode(route.exit_id);
        output << (room != nullptr ? room->name : std::to_string(route.room_id))
               << " -> "
               << (exit != nullptr ? exit->name : std::to_string(route.exit_id))
               << " | "
               << (route.reachable ? "YES" : "NO")
               << " | "
               << FormatCost(route.static_cost)
               << " | "
               << JoinNodePath(graph, route.node_path)
               << " | "
               << JoinEdgePath(route.edge_path)
               << "\n";
    }

    output << "\n------------------------------------------------------------\n";
    output << "4. EXIT QUOTAS\n";
    output << "------------------------------------------------------------\n";
    output << "Total People: " << assignment_result.total_people << "\n";
    int total_exit_rate = 0;
    for (const Node& node : scenario.nodes) {
        if (node.type == NodeType::EXIT && node.active) {
            total_exit_rate += node.exit_service_rate;
        }
    }
    output << "Total Exit Service Rate: " << total_exit_rate << "\n\n";
    output << "Quota Formula:\n";
    output << "quota(exit) = round(total_people * exit_rate / total_exit_rate)\n\n";
    output << "Computed Quotas:\n";
    int quota_sum = 0;
    for (const Node& node : scenario.nodes) {
        if (node.type != NodeType::EXIT || !node.active) {
            continue;
        }
        const int quota = assignment_result.exit_quotas.count(node.id) > 0
                              ? assignment_result.exit_quotas.at(node.id)
                              : 0;
        quota_sum += quota;
        output << node.name << " -> " << quota << "\n";
    }
    output << "\nQuota Sum Check:\n";
    bool first_quota = true;
    for (const Node& node : scenario.nodes) {
        if (node.type != NodeType::EXIT || !node.active) {
            continue;
        }
        if (!first_quota) {
            output << " + ";
        }
        output << assignment_result.exit_quotas.at(node.id);
        first_quota = false;
    }
    output << " = " << quota_sum
           << (quota_sum == assignment_result.total_people ? "  [OK]\n" : "  [MISMATCH]\n");

    output << "\n------------------------------------------------------------\n";
    output << "5. ASSIGNMENT TABLE\n";
    output << "------------------------------------------------------------\n";
    output << "Format:\n";
    output << "Room | Pop | To each Exit | Total Assigned | Status\n\n";
    for (const Node& node : scenario.nodes) {
        if (node.type != NodeType::ROOM || !node.active) {
            continue;
        }

        int room_total = 0;
        output << node.name << " | " << node.initial_people << " | ";
        bool first_exit = true;
        for (const Node& exit_node : scenario.nodes) {
            if (exit_node.type != NodeType::EXIT || !exit_node.active) {
                continue;
            }
            if (!first_exit) {
                output << ", ";
            }
            const int assigned = FindAssignedPeople(assignment_result, node.id, exit_node.id);
            output << exit_node.name << "=" << assigned;
            room_total += assigned;
            first_exit = false;
        }
        output << " | " << room_total << " | "
               << (room_total == node.initial_people ? "OK" : "INCOMPLETE") << "\n";
    }

    output << "\nExit Loads:\n";
    for (const Node& node : scenario.nodes) {
        if (node.type != NodeType::EXIT || !node.active) {
            continue;
        }
        const int load = assignment_result.exit_loads.count(node.id) > 0
                             ? assignment_result.exit_loads.at(node.id)
                             : 0;
        const int quota = assignment_result.exit_quotas.count(node.id) > 0
                              ? assignment_result.exit_quotas.at(node.id)
                              : 0;
        output << node.name << " -> " << load << " / " << quota << "\n";
    }

    output << "\nUnassigned People: " << assignment_result.unassigned_people << "\n";
    output << "Assignment Status: " << (assignment_result.success ? "SUCCESS" : "PARTIAL") << "\n";

    output << "\n------------------------------------------------------------\n";
    output << "6. GROUP GENERATION SUMMARY\n";
    output << "------------------------------------------------------------\n";
    output << "Chunk Size: " << scenario.chunk_size << "\n\n";
    output << "Format:\n";
    output << "Room -> Exit | Assigned People | Groups Generated\n\n";
    for (const GeneratedGroupSummary& summary : simulation_report.group_summaries) {
        const Node* room = graph.FindNode(summary.room_id);
        const Node* exit = graph.FindNode(summary.exit_id);
        output << (room != nullptr ? room->name : std::to_string(summary.room_id))
               << " -> "
               << (exit != nullptr ? exit->name : std::to_string(summary.exit_id))
               << " | " << summary.assigned_people << " | " << summary.display_summary << "\n";
    }
    output << "\nTotal Groups Generated: " << simulation_report.total_groups_generated << "\n";

    output << "\n------------------------------------------------------------\n";
    output << "7. TIMELINE LOG\n";
    output << "------------------------------------------------------------\n";
    output << "Format:\n";
    output << "t = second | event\n\n";
    for (const std::string& event : simulation_report.timeline_events) {
        output << event << "\n";
    }

    output << "\n------------------------------------------------------------\n";
    output << "8. EXIT QUEUE LOG\n";
    output << "------------------------------------------------------------\n";
    output << "Format:\n";
    output << "t | Exit | People Served | Queue Remaining\n\n";
    for (const ExitServiceLogEntry& entry : simulation_report.exit_service_log) {
        const Node* exit = graph.FindNode(entry.exit_id);
        output << "t=" << entry.time << " | "
               << (exit != nullptr ? exit->name : std::to_string(entry.exit_id))
               << " | " << entry.served_people
               << " | " << entry.queue_remaining << "\n";
    }

    output << "\n------------------------------------------------------------\n";
    output << "9. EDGE UTILIZATION LOG\n";
    output << "------------------------------------------------------------\n";
    output << "Format:\n";
    output << "Edge | Max Occupancy | Total Entries | Saturated Seconds | Utilization %\n\n";
    for (const EdgeUsageSummary& usage : simulation_report.edge_usage) {
        const PhysicalEdge* edge = graph.FindEdge(usage.edge_id);
        const Node* a = edge != nullptr ? graph.FindNode(edge->a) : nullptr;
        const Node* b = edge != nullptr ? graph.FindNode(edge->b) : nullptr;
        output << (a != nullptr ? a->name : "?") << " <-> "
               << (b != nullptr ? b->name : "?")
               << " | " << usage.max_occupancy
               << " | " << usage.total_entries
               << " | " << usage.saturated_seconds
               << " | " << FormatFixed(usage.utilization_percent) << "%\n";
    }

    output << "\n------------------------------------------------------------\n";
    output << "10. ROOM WAITING STATISTICS\n";
    output << "------------------------------------------------------------\n";
    output << "Format:\n";
    output << "Room | Initial People | Assigned Exit Split | Max Wait Time | Avg Wait Time\n\n";
    for (const RoomWaitSummary& wait_summary : simulation_report.room_waits) {
        const Node* room = graph.FindNode(wait_summary.room_id);
        output << (room != nullptr ? room->name : std::to_string(wait_summary.room_id))
               << " | " << wait_summary.initial_people
               << " | " << BuildAssignedSplit(graph, wait_summary)
               << " | " << wait_summary.max_wait_time << " sec"
               << " | " << FormatFixed(wait_summary.average_wait_time) << " sec\n";
    }

    output << "\n------------------------------------------------------------\n";
    output << "11. FINAL METRICS\n";
    output << "------------------------------------------------------------\n";
    output << "Routing Status: " << routing.status_message << "\n";
    output << "Assignment Status: " << assignment_result.status_message << "\n";
    output << "Simulation Status: " << simulation_report.status_message << "\n";
    output << "Total Initial People: " << simulation_report.total_initial_people << "\n";
    output << "Total Evacuated People: " << simulation_report.total_evacuated_people << "\n";
    output << "Evacuation Completion Rate: "
           << FormatFixed(simulation_report.total_initial_people > 0
                              ? (100.0 * simulation_report.total_evacuated_people /
                                 simulation_report.total_initial_people)
                              : 0.0,
                          2)
           << "%\n\n";
    output << "Total Evacuation Time: " << simulation_report.total_evacuation_time << " sec\n";
    output << "Average Completion Time Per Person: "
           << FormatFixed(simulation_report.average_completion_time_per_person) << " sec\n";
    output << "Maximum Completion Time: " << simulation_report.maximum_completion_time
           << " sec\n";
    output << "Maximum Waiting Time At Any Room/Junction: "
           << simulation_report.max_waiting_time_anywhere << " sec\n\n";
    output << "Peak Exit Queue Length:\n";
    for (const auto& entry : simulation_report.peak_exit_queue_lengths) {
        const Node* exit = graph.FindNode(entry.first);
        output << "  " << (exit != nullptr ? exit->name : std::to_string(entry.first))
               << " -> " << entry.second << " people\n";
    }
    output << "\nPeak Edge Occupancy:\n";
    for (const auto& entry : simulation_report.peak_edge_occupancies) {
        const PhysicalEdge* edge = graph.FindEdge(entry.first);
        const Node* a = edge != nullptr ? graph.FindNode(edge->a) : nullptr;
        const Node* b = edge != nullptr ? graph.FindNode(edge->b) : nullptr;
        output << "  " << (a != nullptr ? a->name : "?")
               << " <-> " << (b != nullptr ? b->name : "?")
               << " -> " << entry.second << "\n";
    }

    double max_utilization = -1.0;
    int most_congested_edge_id = -1;
    for (const EdgeUsageSummary& usage : simulation_report.edge_usage) {
        if (usage.utilization_percent > max_utilization) {
            max_utilization = usage.utilization_percent;
            most_congested_edge_id = usage.edge_id;
        }
    }
    output << "\nMost Congested Edge:\n";
    if (most_congested_edge_id >= 0) {
        const PhysicalEdge* edge = graph.FindEdge(most_congested_edge_id);
        const Node* a = edge != nullptr ? graph.FindNode(edge->a) : nullptr;
        const Node* b = edge != nullptr ? graph.FindNode(edge->b) : nullptr;
        output << "  " << (a != nullptr ? a->name : "?")
               << " <-> " << (b != nullptr ? b->name : "?")
               << " (" << FormatFixed(max_utilization) << "% utilization)\n";
    } else {
        output << "  NONE\n";
    }
    output << "\nSimulation Stuck: " << (simulation_report.stuck ? "YES" : "NO") << "\n";
    output << "All Rooms Served: " << (assignment_result.success ? "YES" : "NO") << "\n";
    output << "All Exits Used Correctly: "
           << ((simulation_report.total_evacuated_people == simulation_report.total_initial_people &&
                !simulation_report.stuck)
                   ? "YES"
                   : "NO")
           << "\n";

    output << "\n------------------------------------------------------------\n";
    output << "12. FINAL STATUS\n";
    output << "------------------------------------------------------------\n";
    if (validation.ok && simulation_report.completed && !simulation_report.stuck &&
        simulation_report.total_evacuated_people == simulation_report.total_initial_people) {
        output << "RESULT: SUCCESS\n";
        output << "All evacuees reached a valid exit and were processed successfully.\n";
    } else if (validation.ok) {
        output << "RESULT: INCOMPLETE\n";
        output << "Simulation ended before a full successful evacuation.\n";
    } else {
        output << "RESULT: INVALID SCENARIO\n";
        output << "Fix validation errors before proceeding.\n";
    }

    output << "\n============================================================\n";
    output << "END OF OUTPUT\n";
    output << "============================================================\n";
}

}  // namespace

int main(int argc, char* argv[]) {
    const std::string preferred_input_path = argc >= 2 ? argv[1] : "input.txt";
    const std::string output_path = argc >= 3 ? argv[2] : "output.txt";

    try {
        Graph graph;
        std::string load_error;
        if (!graph.LoadFromFile(preferred_input_path, load_error)) {
            std::ofstream output(output_path);
            output << "Failed to load scenario input.\n";
            output << "Input file: " << preferred_input_path << "\n";
            output << load_error << "\n";
            std::cerr << load_error << std::endl;
            return 1;
        }

        const ValidationResult validation = graph.Validate();
        const RoutingResult routing = ComputeAllShortestRoutes(graph);
        const AssignmentResult assignment_result =
            ComputeAssignments(graph.GetScenario(), routing.routes);
        const SimulationReport simulation_report =
            RunSimulation(graph.GetScenario(), assignment_result.assignments, routing.routes);
        WriteFullReport(
            graph, validation, routing, assignment_result, simulation_report, output_path);
        return validation.ok ? 0 : 1;
    } catch (const std::exception& ex) {
        std::ofstream output(output_path);
        output << "Unhandled error: " << ex.what() << "\n";
        std::cerr << ex.what() << std::endl;
        return 1;
    }
}

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <map>
#include <string>
#include <vector>

#include "assignment.h"
#include "dijkstra.h"
#include "graph.h"

struct Group {
    int group_id = 0;
    int room_id = 0;
    int exit_id = 0;
    int size = 0;
    int remaining_people = 0;
    std::vector<int> route_edge_path;
    std::vector<int> route_node_path;
    int current_step_index = 0;
    int current_node_id = 0;
    int ready_time = 0;
    bool finished = false;
    int total_wait_time = 0;
    int max_single_wait = 0;
    int last_queue_enter_time = 0;
    int completion_time = -1;
};

struct GeneratedGroupSummary {
    int room_id = 0;
    int exit_id = 0;
    int assigned_people = 0;
    int group_count = 0;
    std::vector<int> chunk_sizes;
    std::string display_summary;
};

struct ExitServiceLogEntry {
    int time = 0;
    int exit_id = 0;
    int served_people = 0;
    int queue_remaining = 0;
};

struct EdgeUsageSummary {
    int edge_id = 0;
    int max_occupancy = 0;
    int total_entries = 0;
    int saturated_seconds = 0;
    double utilization_percent = 0.0;
};

struct RoomWaitSummary {
    int room_id = 0;
    int initial_people = 0;
    std::map<int, int> assigned_by_exit;
    int max_wait_time = 0;
    double average_wait_time = 0.0;
};

struct SimulationReport {
    bool completed = false;
    bool stuck = false;
    int total_groups_generated = 0;
    int total_initial_people = 0;
    int total_evacuated_people = 0;
    int total_evacuation_time = 0;
    double average_completion_time_per_person = 0.0;
    int maximum_completion_time = 0;
    int max_waiting_time_anywhere = 0;
    int stuck_threshold_seconds = 0;
    std::map<int, int> peak_exit_queue_lengths;
    std::map<int, int> peak_edge_occupancies;
    std::vector<Group> generated_groups;
    std::vector<GeneratedGroupSummary> group_summaries;
    std::vector<std::string> timeline_events;
    std::vector<ExitServiceLogEntry> exit_service_log;
    std::vector<EdgeUsageSummary> edge_usage;
    std::vector<RoomWaitSummary> room_waits;
    std::string status_message;
};

SimulationReport RunSimulation(const Scenario& scenario,
                               const std::vector<Assignment>& assignments,
                               const std::vector<Route>& routes);

#endif

#ifndef ASSIGNMENT_H
#define ASSIGNMENT_H

#include <map>
#include <string>
#include <vector>

#include "dijkstra.h"
#include "graph.h"

struct Assignment {
    int room_id = 0;
    int exit_id = 0;
    int assigned_people = 0;
};

struct AssignmentResult {
    std::map<int, int> exit_quotas;
    std::map<int, int> exit_loads;
    std::vector<Assignment> assignments;
    int total_people = 0;
    int total_assigned = 0;
    int unassigned_people = 0;
    bool success = false;
    std::string status_message;
};

AssignmentResult ComputeAssignments(const Scenario& scenario,
                                    const std::vector<Route>& routes);

#endif

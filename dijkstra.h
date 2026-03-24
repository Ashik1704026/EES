#ifndef DIJKSTRA_H
#define DIJKSTRA_H

#include <string>
#include <vector>

#include "graph.h"

struct Route {
    int room_id = 0;
    int exit_id = 0;
    std::vector<int> node_path;
    std::vector<int> edge_path;
    double static_cost = 0.0;
    bool reachable = false;
};

struct RoutingResult {
    std::vector<Route> routes;
    std::string status_message;
};

RoutingResult ComputeAllShortestRoutes(const Graph& graph);

#endif

#include "dijkstra.h"

#include <algorithm>
#include <limits>
#include <queue>
#include <unordered_map>

namespace {

struct QueueState {
    double distance = 0.0;
    int node_id = 0;

    bool operator>(const QueueState& other) const {
        if (distance != other.distance) {
            return distance > other.distance;
        }
        return node_id > other.node_id;
    }
};

std::vector<int> CollectRoomIds(const Scenario& scenario) {
    std::vector<int> ids;
    for (const Node& node : scenario.nodes) {
        if (node.active && node.type == NodeType::ROOM) {
            ids.push_back(node.id);
        }
    }
    return ids;
}

std::vector<int> CollectExitIds(const Scenario& scenario) {
    std::vector<int> ids;
    for (const Node& node : scenario.nodes) {
        if (node.active && node.type == NodeType::EXIT) {
            ids.push_back(node.id);
        }
    }
    return ids;
}

Route BuildRoute(int room_id,
                 int exit_id,
                 const std::unordered_map<int, double>& distances,
                 const std::unordered_map<int, int>& previous_node,
                 const std::unordered_map<int, int>& previous_edge) {
    Route route;
    route.room_id = room_id;
    route.exit_id = exit_id;

    const auto distance_it = distances.find(exit_id);
    if (distance_it == distances.end() ||
        distance_it->second == std::numeric_limits<double>::infinity()) {
        route.reachable = false;
        route.static_cost = std::numeric_limits<double>::infinity();
        return route;
    }

    route.reachable = true;
    route.static_cost = distance_it->second;

    std::vector<int> reverse_nodes;
    std::vector<int> reverse_edges;
    int current = exit_id;
    reverse_nodes.push_back(current);

    while (current != room_id) {
        const auto node_it = previous_node.find(current);
        const auto edge_it = previous_edge.find(current);
        if (node_it == previous_node.end() || edge_it == previous_edge.end()) {
            route.reachable = false;
            route.node_path.clear();
            route.edge_path.clear();
            route.static_cost = std::numeric_limits<double>::infinity();
            return route;
        }
        reverse_edges.push_back(edge_it->second);
        current = node_it->second;
        reverse_nodes.push_back(current);
    }

    route.node_path.assign(reverse_nodes.rbegin(), reverse_nodes.rend());
    route.edge_path.assign(reverse_edges.rbegin(), reverse_edges.rend());
    return route;
}

}  // namespace

RoutingResult ComputeAllShortestRoutes(const Graph& graph) {
    RoutingResult result;
    const Scenario& scenario = graph.GetScenario();
    const std::vector<int> room_ids = CollectRoomIds(scenario);
    const std::vector<int> exit_ids = CollectExitIds(scenario);

    for (int room_id : room_ids) {
        std::unordered_map<int, double> distances;
        std::unordered_map<int, int> previous_node;
        std::unordered_map<int, int> previous_edge;

        for (const Node& node : scenario.nodes) {
            distances[node.id] = std::numeric_limits<double>::infinity();
        }
        distances[room_id] = 0.0;

        std::priority_queue<QueueState, std::vector<QueueState>, std::greater<QueueState>> frontier;
        frontier.push({0.0, room_id});

        while (!frontier.empty()) {
            const QueueState current = frontier.top();
            frontier.pop();

            if (current.distance > distances[current.node_id]) {
                continue;
            }

            for (const AdjacencyEntry& neighbor : graph.GetNeighbors(current.node_id)) {
                const Node* neighbor_node = graph.FindNode(neighbor.neighbor_node_id);
                const PhysicalEdge* edge = graph.FindEdge(neighbor.edge_id);
                if (neighbor_node == nullptr || edge == nullptr) {
                    continue;
                }
                if (!neighbor_node->active || !edge->active) {
                    continue;
                }

                const double candidate_distance = current.distance + neighbor.weight;
                const double known_distance = distances[neighbor.neighbor_node_id];
                if (candidate_distance < known_distance) {
                    distances[neighbor.neighbor_node_id] = candidate_distance;
                    previous_node[neighbor.neighbor_node_id] = current.node_id;
                    previous_edge[neighbor.neighbor_node_id] = neighbor.edge_id;
                    frontier.push({candidate_distance, neighbor.neighbor_node_id});
                }
            }
        }

        for (int exit_id : exit_ids) {
            result.routes.push_back(
                BuildRoute(room_id, exit_id, distances, previous_node, previous_edge));
        }
    }

    result.status_message = "Phase 2 complete: Dijkstra shortest routes computed.";
    return result;
}

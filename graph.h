#ifndef GRAPH_H
#define GRAPH_H

#include <string>
#include <unordered_map>
#include <vector>

enum class NodeType {
    ROOM,
    JUNCTION,
    EXIT
};

struct Node {
    int id = 0;
    std::string name;
    NodeType type = NodeType::JUNCTION;
    int initial_people = 0;
    int exit_service_rate = 0;
    int priority = 0;
    bool active = true;
};

struct PhysicalEdge {
    int id = 0;
    int a = 0;
    int b = 0;
    double length_m = 0.0;
    double speed_mps = 0.0;
    int entry_capacity_per_sec = 0;
    int occupancy_limit = 0;
    double hazard_penalty = 0.0;
    bool active = true;
    int travel_time_sec = 0;
};

struct Scenario {
    std::string name;
    int time_step_sec = 1;
    int chunk_size = 1;
    std::vector<Node> nodes;
    std::vector<PhysicalEdge> edges;
};

struct ValidationResult {
    bool ok = false;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

struct AdjacencyEntry {
    int neighbor_node_id = 0;
    int edge_id = 0;
    double weight = 0.0;
};

class Graph {
public:
    bool LoadFromFile(const std::string& path, std::string& error_message);
    ValidationResult Validate() const;

    const Scenario& GetScenario() const;
    const std::vector<AdjacencyEntry>& GetNeighbors(int node_id) const;
    const Node* FindNode(int node_id) const;
    const PhysicalEdge* FindEdge(int edge_id) const;

private:
    void RebuildIndexes();
    void RebuildAdjacency();

    Scenario scenario_;
    std::unordered_map<int, std::size_t> node_index_by_id_;
    std::unordered_map<int, std::size_t> edge_index_by_id_;
    std::unordered_map<int, std::vector<AdjacencyEntry>> adjacency_;
};

std::string NodeTypeToString(NodeType type);
bool TryParseNodeType(const std::string& text, NodeType& type);
int CountNodesOfType(const Scenario& scenario, NodeType type);
int TotalInitialPeople(const Scenario& scenario);

#endif

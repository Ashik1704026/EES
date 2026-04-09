#include "graph.h"

#include <cmath>
#include <fstream>
#include <sstream>
#include <unordered_set>

#include "utils.h"

namespace {

bool ParseHeaderLine(const std::vector<std::string>& tokens,
                     const std::string& expected_key,
                     int& value,
                     std::string& error_message) {
    if (tokens.size() != 2 || ToUpperCopy(tokens[0]) != expected_key) {
        error_message = "Expected header line: " + expected_key + " <value>";
        return false;
    }
    return TryParseInt(tokens[1], value);
}

bool IsScenarioHeader(const std::vector<std::string>& tokens) {
    return tokens.size() == 2 && ToUpperCopy(tokens[0]) == "SCENARIO";
}

bool ParseNodeLine(const std::vector<std::string>& tokens,
                   Node& node,
                   std::string& error_message) {
    if (tokens.size() == 8 && ToUpperCopy(tokens[0]) == "NODE") {
        if (!TryParseInt(tokens[1], node.id)) {
            error_message = "Invalid node id: " + tokens[1];
            return false;
        }
        node.name = tokens[2];
        if (!TryParseNodeType(tokens[3], node.type)) {
            error_message = "Invalid node type: " + tokens[3];
            return false;
        }
        if (!TryParseInt(tokens[4], node.initial_people) ||
            !TryParseInt(tokens[5], node.exit_service_rate) ||
            !TryParseInt(tokens[6], node.priority)) {
            error_message = "Invalid numeric node field on node " + std::to_string(node.id);
            return false;
        }

        int active_flag = 0;
        if (!TryParseInt(tokens[7], active_flag)) {
            error_message = "Invalid active flag on node " + std::to_string(node.id);
            return false;
        }
        node.active = (active_flag != 0);
        return true;
    }

    if (tokens.size() == 5) {
        if (!TryParseInt(tokens[0], node.id)) {
            error_message = "Invalid node id: " + tokens[0];
            return false;
        }
        if (!TryParseNodeType(tokens[1], node.type)) {
            error_message = "Invalid node type: " + tokens[1];
            return false;
        }
        node.name = tokens[2];
        if (!TryParseInt(tokens[3], node.initial_people) ||
            !TryParseInt(tokens[4], node.exit_service_rate)) {
            error_message = "Invalid numeric node field on node " + std::to_string(node.id);
            return false;
        }
        node.priority = 0;
        node.active = true;
        return true;
    }

    error_message =
        "Expected node line in either format: NODE id name type initial_people "
        "exit_service_rate priority active OR id type name people exit_rate";
    return false;
}

bool ParseEdgeLine(const std::vector<std::string>& tokens,
                   PhysicalEdge& edge,
                   std::string& error_message) {
    if (tokens.size() == 10 && ToUpperCopy(tokens[0]) == "EDGE") {
        int active_flag = 0;
        if (!TryParseInt(tokens[1], edge.id) ||
            !TryParseInt(tokens[2], edge.a) ||
            !TryParseInt(tokens[3], edge.b) ||
            !TryParseDouble(tokens[4], edge.length_m) ||
            !TryParseDouble(tokens[5], edge.speed_mps) ||
            !TryParseInt(tokens[6], edge.entry_capacity_per_sec) ||
            !TryParseInt(tokens[7], edge.occupancy_limit) ||
            !TryParseDouble(tokens[8], edge.hazard_penalty) ||
            !TryParseInt(tokens[9], active_flag)) {
            error_message = "Invalid edge fields on edge line";
            return false;
        }

        edge.active = (active_flag != 0);
        if (edge.speed_mps > 0.0) {
            edge.travel_time_sec = static_cast<int>(std::ceil(edge.length_m / edge.speed_mps));
        }
        return true;
    }

    if (tokens.size() == 9) {
        int active_flag = 0;
        if (!TryParseInt(tokens[0], edge.id) ||
            !TryParseInt(tokens[1], edge.a) ||
            !TryParseInt(tokens[2], edge.b) ||
            !TryParseDouble(tokens[3], edge.length_m) ||
            !TryParseDouble(tokens[4], edge.speed_mps) ||
            !TryParseInt(tokens[5], edge.entry_capacity_per_sec) ||
            !TryParseInt(tokens[6], edge.occupancy_limit) ||
            !TryParseDouble(tokens[7], edge.hazard_penalty) ||
            !TryParseInt(tokens[8], active_flag)) {
            error_message = "Invalid edge fields on edge line";
            return false;
        }

        edge.active = (active_flag != 0);
        if (edge.speed_mps > 0.0) {
            edge.travel_time_sec = static_cast<int>(std::ceil(edge.length_m / edge.speed_mps));
        }
        return true;
    }

    error_message =
        "Expected edge line in either format: EDGE id a b length_m speed_mps "
        "entry_capacity occupancy_limit hazard_penalty active OR "
        "id a b length speed entry_cap occupancy hazard active";
    return false;
}

bool ParseScenarioBody(const std::vector<std::string>& lines,
                       std::size_t& cursor,
                       Scenario& scenario,
                       std::string& error_message) {
    std::vector<std::string> tokens;
    auto next_tokens = [&](std::vector<std::string>& out_tokens) -> bool {
        if (cursor >= lines.size()) {
            error_message = "Unexpected end of file while parsing scenario " + scenario.name;
            return false;
        }
        out_tokens = SplitWhitespace(lines[cursor++]);
        return true;
    };

    if (!next_tokens(tokens) ||
        !ParseHeaderLine(tokens, "TIME_STEP", scenario.time_step_sec, error_message)) {
        return false;
    }
    if (!next_tokens(tokens) ||
        !ParseHeaderLine(tokens, "CHUNK_SIZE", scenario.chunk_size, error_message)) {
        return false;
    }

    int node_count = 0;
    if (!next_tokens(tokens) || !ParseHeaderLine(tokens, "NODES", node_count, error_message)) {
        return false;
    }
    for (int i = 0; i < node_count; ++i) {
        Node node;
        if (!next_tokens(tokens) || !ParseNodeLine(tokens, node, error_message)) {
            return false;
        }
        scenario.nodes.push_back(node);
    }

    int edge_count = 0;
    if (!next_tokens(tokens) || !ParseHeaderLine(tokens, "EDGES", edge_count, error_message)) {
        return false;
    }
    for (int i = 0; i < edge_count; ++i) {
        PhysicalEdge edge;
        if (!next_tokens(tokens) || !ParseEdgeLine(tokens, edge, error_message)) {
            return false;
        }
        scenario.edges.push_back(edge);
    }

    if (cursor < lines.size()) {
        const std::vector<std::string> maybe_end = SplitWhitespace(lines[cursor]);
        if (maybe_end.size() == 1 && ToUpperCopy(maybe_end[0]) == "END_SCENARIO") {
            ++cursor;
        }
    }

    return true;
}

}  // namespace

bool Graph::LoadFromFile(const std::string& path, std::string& error_message) {
    std::ifstream input(path);
    if (!input) {
        error_message = "Unable to open input file: " + path;
        return false;
    }

    Scenario loaded;
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(input, line)) {
        const std::string cleaned = StripComment(TrimCopy(line));
        if (!cleaned.empty()) {
            lines.push_back(cleaned);
        }
    }

    if (lines.empty()) {
        error_message = "Input file is empty: " + path;
        return false;
    }

    std::size_t cursor = 0;
    std::vector<std::string> tokens = SplitWhitespace(lines[cursor]);
    if (tokens.size() == 2 && ToUpperCopy(tokens[0]) == "ACTIVE_SCENARIO") {
        const std::string active_scenario = tokens[1];
        ++cursor;

        bool found_active = false;
        while (cursor < lines.size()) {
            tokens = SplitWhitespace(lines[cursor]);
            if (!IsScenarioHeader(tokens)) {
                error_message = "Expected SCENARIO <name> after ACTIVE_SCENARIO.";
                return false;
            }

            Scenario candidate;
            candidate.name = tokens[1];
            ++cursor;
            if (!ParseScenarioBody(lines, cursor, candidate, error_message)) {
                return false;
            }

            if (!found_active && candidate.name == active_scenario) {
                loaded = candidate;
                found_active = true;
            }
        }

        if (!found_active) {
            error_message = "ACTIVE_SCENARIO " + active_scenario + " not found in file.";
            return false;
        }
    } else {
        loaded.name = "DEFAULT";
        if (!ParseScenarioBody(lines, cursor, loaded, error_message)) {
            return false;
        }
    }

    scenario_ = loaded;
    RebuildIndexes();
    RebuildAdjacency();
    return true;
}

ValidationResult Graph::Validate() const {
    ValidationResult result;

    if (scenario_.time_step_sec != 1) {
        result.warnings.push_back("Spec expects time_step_sec to be 1.");
    }
    if (scenario_.chunk_size <= 0) {
        result.errors.push_back("chunk_size must be positive.");
    }
    if (scenario_.nodes.empty()) {
        result.errors.push_back("Scenario must contain at least one node.");
    }
    if (scenario_.edges.empty()) {
        result.errors.push_back("Scenario must contain at least one edge.");
    }

    std::unordered_set<int> node_ids;
    int room_count = 0;
    int exit_count = 0;
    for (const Node& node : scenario_.nodes) {
        if (!node_ids.insert(node.id).second) {
            result.errors.push_back("Duplicate node id: " + std::to_string(node.id));
        }
        if (node.name.empty()) {
            result.errors.push_back("Node " + std::to_string(node.id) + " has an empty name.");
        }
        if (node.initial_people < 0) {
            result.errors.push_back("Node " + std::to_string(node.id) +
                                    " has negative initial_people.");
        }
        if (node.exit_service_rate < 0) {
            result.errors.push_back("Node " + std::to_string(node.id) +
                                    " has negative exit_service_rate.");
        }
        if (node.type == NodeType::ROOM) {
            ++room_count;
        }
        if (node.type == NodeType::EXIT) {
            ++exit_count;
            if (node.exit_service_rate <= 0) {
                result.errors.push_back("Exit node " + std::to_string(node.id) +
                                        " must have positive exit_service_rate.");
            }
        }
    }

    if (room_count == 0) {
        result.errors.push_back("Scenario must contain at least one ROOM.");
    }
    if (exit_count == 0) {
        result.errors.push_back("Scenario must contain at least one EXIT.");
    }

    std::unordered_set<int> edge_ids;
    for (const PhysicalEdge& edge : scenario_.edges) {
        if (!edge_ids.insert(edge.id).second) {
            result.errors.push_back("Duplicate edge id: " + std::to_string(edge.id));
        }
        if (edge.a == edge.b) {
            result.errors.push_back("Edge " + std::to_string(edge.id) +
                                    " cannot connect a node to itself.");
        }
        if (FindNode(edge.a) == nullptr || FindNode(edge.b) == nullptr) {
            result.errors.push_back("Edge " + std::to_string(edge.id) +
                                    " references an unknown endpoint.");
        }
        if (edge.length_m <= 0.0) {
            result.errors.push_back("Edge " + std::to_string(edge.id) + " must have positive length.");
        }
        if (edge.speed_mps <= 0.0) {
            result.errors.push_back("Edge " + std::to_string(edge.id) + " must have positive speed.");
        }
        if (edge.entry_capacity_per_sec <= 0) {
            result.errors.push_back("Edge " + std::to_string(edge.id) +
                                    " must have positive entry_capacity_per_sec.");
        }
        if (edge.occupancy_limit <= 0) {
            result.errors.push_back("Edge " + std::to_string(edge.id) +
                                    " must have positive occupancy_limit.");
        }
    }

    result.ok = result.errors.empty();
    return result;
}

const Scenario& Graph::GetScenario() const {
    return scenario_;
}

const std::vector<AdjacencyEntry>& Graph::GetNeighbors(int node_id) const {
    static const std::vector<AdjacencyEntry> empty;
    const auto it = adjacency_.find(node_id);
    return it == adjacency_.end() ? empty : it->second;
}

const Node* Graph::FindNode(int node_id) const {
    const auto it = node_index_by_id_.find(node_id);
    if (it == node_index_by_id_.end()) {
        return nullptr;
    }
    return &scenario_.nodes[it->second];
}

const PhysicalEdge* Graph::FindEdge(int edge_id) const {
    const auto it = edge_index_by_id_.find(edge_id);
    if (it == edge_index_by_id_.end()) {
        return nullptr;
    }
    return &scenario_.edges[it->second];
}

void Graph::RebuildIndexes() {
    node_index_by_id_.clear();
    edge_index_by_id_.clear();

    for (std::size_t i = 0; i < scenario_.nodes.size(); ++i) {
        node_index_by_id_[scenario_.nodes[i].id] = i;
    }
    for (std::size_t i = 0; i < scenario_.edges.size(); ++i) {
        edge_index_by_id_[scenario_.edges[i].id] = i;
    }
}

void Graph::RebuildAdjacency() {
    adjacency_.clear();
    for (const PhysicalEdge& edge : scenario_.edges) {
        const double weight = static_cast<double>(edge.travel_time_sec) + edge.hazard_penalty;
        adjacency_[edge.a].push_back({edge.b, edge.id, weight});
        adjacency_[edge.b].push_back({edge.a, edge.id, weight});
    }
}

std::string NodeTypeToString(NodeType type) {
    switch (type) {
        case NodeType::ROOM:
            return "ROOM";
        case NodeType::JUNCTION:
            return "JUNCTION";
        case NodeType::EXIT:
            return "EXIT";
    }
    return "UNKNOWN";
}

bool TryParseNodeType(const std::string& text, NodeType& type) {
    const std::string upper = ToUpperCopy(text);
    if (upper == "ROOM") {
        type = NodeType::ROOM;
        return true;
    }
    if (upper == "JUNCTION") {
        type = NodeType::JUNCTION;
        return true;
    }
    if (upper == "EXIT") {
        type = NodeType::EXIT;
        return true;
    }
    return false;
}

int CountNodesOfType(const Scenario& scenario, NodeType type) {
    int count = 0;
    for (const Node& node : scenario.nodes) {
        if (node.type == type) {
            ++count;
        }
    }
    return count;
}

int TotalInitialPeople(const Scenario& scenario) {
    int total = 0;
    for (const Node& node : scenario.nodes) {
        if (node.type == NodeType::ROOM) {
            total += node.initial_people;
        }
    }
    return total;
}

Build a C++17 project called "Intelligent Emergency Evacuation Planning System".

Goal:
Create a graph-based evacuation simulator for a building or small campus. The system must:
1. model the environment as a graph of rooms, junctions, corridors, and exits,
2. compute safe shortest paths from each room to each exit,
3. assign evacuees from rooms to exits while respecting exit capacity limits,
4. dynamically simulate evacuation over time through shared corridor segments,
5. decide movement direction during simulation, since physical corridors are bidirectional,
6. enforce travel time, occupancy, and throughput constraints on corridor segments,
7. output route tables, assignment tables, timeline logs, and final performance metrics.

Important modeling rules:
- The physical graph is bidirectional for corridor segments, not pre-directed in input.
- Direction is chosen dynamically by the algorithm when a group moves from one node to another.
- A corridor segment is a shared physical resource; people moving in either direction share its capacity and occupancy.
- The input should describe physical structure only, not fixed evacuation direction.
- A room may split its occupants across multiple exits.
- Exit 1 and Exit 2 may be used simultaneously if flows use different corridor segments.
- Edge travel is not instantaneous; a group remains on an edge for several seconds depending on edge length and speed.
- Do not model the whole corridor as one single global bottleneck unless explicitly given as one edge.
- Corridors are treated as shared resources, and time-based scheduling must resolve conflicts and congestion.
- Use only standard C++17 and STL, consistent with the proposal’s intended implementation environment.

Data model:

NodeType:
- ROOM
- JUNCTION
- EXIT

Node:
- id: int
- name: string
- type: NodeType
- initial_people: int (for ROOM)
- exit_service_rate: int (for EXIT, people per second)
- priority: int default 0
- active: bool default true

PhysicalEdge:
- id: int
- a: int   // endpoint node id
- b: int   // endpoint node id
- length_m: double
- speed_mps: double
- entry_capacity_per_sec: int   // total number of people allowed to enter this physical edge per second, regardless of direction
- occupancy_limit: int          // total people simultaneously allowed on this physical edge, regardless of direction
- hazard_penalty: double default 0
- active: bool default true
- travel_time_sec: int = ceil(length_m / speed_mps)

Route:
- room_id: int
- exit_id: int
- node_path: vector<int>
- edge_path: vector<int>
- static_cost: double

Assignment:
- room_id: int
- exit_id: int
- assigned_people: int

Group:
- group_id: int
- room_id: int
- exit_id: int
- size: int
- route_edge_path: vector<int>
- current_step_index: int
- current_node_id: int
- ready_time: int
- finished: bool

TraversalEvent:
- group_id: int
- people: int
- edge_id: int
- from_node: int
- to_node: int
- enter_time: int
- leave_time: int

DynamicEdgeState:
- active_traversals: deque<TraversalEvent>
- entered_this_second_total: int
- current_occupancy_total: int

ExitQueueState:
- queue of arriving groups or partial groups waiting to leave through the exit
- service rate = exit_service_rate people per second

Input format:
Read scenario from JSON or a simple structured text file.
The input must include:
- time_step_sec (use 1)
- chunk_size
- nodes[]
- edges[]

Important:
- Corridor edges are undirected physical edges in input.
- During routing, each physical edge can be traversed both ways.
- During simulation, a traversal on an edge has a chosen direction (a->b or b->a), but both directions share the same edge capacity and occupancy.

Routing phase:
1. Build an adjacency representation from the physical graph.
2. For each room, run Dijkstra to each exit.
3. Treat every active physical edge as traversable in both directions for pathfinding.
4. Use edge weight:
   weight = travel_time_sec + hazard_penalty
5. Save the shortest route from every room to every reachable exit:
   - total cost
   - node path
   - edge path

Assignment phase:
1. Build a room-to-exit min-cost flow model:
   - SuperSource -> Room_i with capacity = room initial_people, cost = 0
   - Room_i -> Exit_j with capacity = room initial_people, cost = shortest path cost from Dijkstra, only if exit_j is reachable
   - Exit_j -> SuperSink with capacity based on exit quota
2. Compute exit quota proportional to exit service rates:
   quota_j = round(total_people * exit_service_rate_j / sum(exit_service_rates))
3. If rounding causes mismatch, fix the final quota so total assigned people equals total population.
4. Solve min-cost flow and produce assignments:
   - room -> exit -> assigned_people
5. If min-cost flow is too hard, implement a correct fallback:
   - sort candidate room-exit pairs by cost
   - assign greedily while respecting remaining room population and exit quota
   - but structure the code so real min-cost flow can replace it later

Group generation phase:
1. Convert each assignment into small groups using chunk_size.
2. Example:
   assigned_people = 15, chunk_size = 2
   => groups of 2,2,2,2,2,2,2,1
3. Every group starts at its room node with ready_time = 0.

Dynamic simulation phase:
Run the evacuation in discrete time steps of 1 second.

At each time t:

Step A: Clear completed edge traversals
- For each physical edge, remove all traversal events with leave_time == t.
- Move those groups to the destination node of that traversal.
- If destination is an EXIT, place the group into that exit queue.
- Otherwise place the group into the waiting queue of that node.

Step B: Serve exit queues
- For each exit, evacuate up to exit_service_rate people from its queue during this second.
- Support partial service of a group if needed.
- Mark groups finished when all people in that group have exited.

Step C: Try to dispatch waiting groups from nodes into their next route edge
- For each node queue, examine waiting groups in priority order:
  1. higher node/group priority
  2. earlier ready_time
  3. shorter remaining route
  4. smaller group_id as tie-break
- For each group, determine its next edge and intended direction.
- A group may enter the physical edge only if:
  1. the edge is active,
  2. entered_this_second_total + group_size <= entry_capacity_per_sec,
  3. current_occupancy_total + group_size <= occupancy_limit
- If allowed:
  - create a TraversalEvent with chosen direction
  - enter_time = t
  - leave_time = t + travel_time_sec
  - add to the edge state
  - update entered_this_second_total
  - update current_occupancy_total
- If not allowed, the group remains waiting at its node.

Step D: Reset per-second counters
- After processing all dispatches for this second, reset entered_this_second_total on each edge before the next time step.

Termination:
- Continue until all people are evacuated, or no progress is possible for a long threshold and the simulation is declared stuck.

Important simulation behavior:
- People do not teleport between nodes.
- A long corridor segment remains occupied while traversals are in progress.
- Rooms do not wait for full evacuation of other rooms globally.
- Different parts of the graph can operate simultaneously.
- Example:
  left-side rooms may move toward Exit 1 while right-side rooms move toward Exit 2 at the same time, as long as they use different edges or capacity on shared edges is still available.
- If two groups use the same physical edge in opposite directions during the same second, they still share the same total entry capacity and same total occupancy.
- This models a physically bidirectional corridor whose direction is chosen dynamically by group movement.

Output requirements:

1. Scenario summary
- total nodes
- total edges
- total rooms
- total exits
- total people
- chunk size
- time step

2. Route table
For every room and exit:
- cost
- reachable/unreachable
- node path

3. Assignment table
For every room:
- number assigned to each exit
- total assigned
- final exit quotas and actual loads

4. Timeline log
For each second, print key events such as:
- group entered edge in direction X->Y
- group arrived at node
- group joined exit queue
- people evacuated through exit

5. Final metrics
- total evacuation time
- total evacuated people
- average completion time per person
- max waiting time at any room/junction
- max queue length per exit
- max occupancy per edge
- edge utilization percentage
- whether all rooms reached at least one exit
- whether simulation got stuck

Recommended project files:
- main.cpp
- model.h
- parser.h / parser.cpp
- graph.h / graph.cpp
- routing.h / routing.cpp
- assignment.h / assignment.cpp
- simulation.h / simulation.cpp
- reporting.h / reporting.cpp

Implementation notes:
- Use STL only: vector, queue, deque, priority_queue, unordered_map, map, algorithm, cmath, limits.
- Keep the code modular.
- Use clear comments.
- Add validation checks for malformed scenarios.
- Make it easy to swap the greedy assignment with true min-cost flow later.
- Design for synthetic datasets and multiple test scenarios.

Test scenario:
Include a built-in sample scenario based on:
- 6 rooms
- 1 corridor chain with 3 junctions
- 2 exits
- room populations [50, 40, 60, 25, 30, 70]
- exits with service rates 3 and 4 people/sec
- corridor segment lengths and capacities configurable
- some middle rooms may split toward either exit

The final result should compile and run in C++17, producing a full evacuation simulation from input to final report.
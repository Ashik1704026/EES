#!/usr/bin/env python3
"""
README
------
Run `python generate_animation.py` (optionally with `--input path/to/input.txt` and `--output
path/to/output.txt`) to parse the simulator files, render the evacuation graph, and emit
`evacuation_animation.gif` (and `evacuation_animation.mp4` if ffmpeg is installed).
"""

import argparse
import os
import re
from collections import defaultdict

import matplotlib.animation as animation
import matplotlib.pyplot as plt
import networkx as nx
from matplotlib.patches import Patch


def parse_input_file(filepath):
    if not os.path.exists(filepath):
        raise FileNotFoundError(f"Input file {filepath} not found")

    nodes = []
    edges = []
    time_step = 1.0
    active_scenario = None
    scenario_mode = False
    scenario_lines = []

    with open(filepath, "r", encoding="utf-8") as fh:
        for raw in fh:
            line = raw.strip()
            if not line:
                continue
            if line.startswith("ACTIVE_SCENARIO"):
                parts = line.split()
                if len(parts) >= 2:
                    active_scenario = parts[1]
                continue
            if line.startswith("SCENARIO"):
                parts = line.split()
                scenario_name = parts[1] if len(parts) > 1 else ""
                scenario_mode = scenario_name == active_scenario
                continue
            if line == "END_SCENARIO":
                if scenario_mode:
                    break
                scenario_mode = False
                continue
            if not scenario_mode:
                continue
            scenario_lines.append(line)

    if not scenario_lines:
        raise ValueError("No active scenario found in the input file")

    idx = 0
    while idx < len(scenario_lines):
        line = scenario_lines[idx]
        if line.startswith("TIME_STEP"):
            parts = line.split()
            if len(parts) > 1:
                try:
                    time_step = float(parts[1])
                except ValueError:
                    pass
        elif line.startswith("NODES"):
            parts = line.split()
            count = int(parts[1]) if len(parts) > 1 else 0
            for node_line in scenario_lines[idx + 1 : idx + 1 + count]:
                fields = node_line.split()
                if len(fields) < 5:
                    continue
                try:
                    nodes.append(
                        {
                            "id": int(fields[0]),
                            "type": fields[1],
                            "name": fields[2],
                            "initial_people": int(fields[3]),
                            "exit_rate": float(fields[4]),
                        }
                    )
                except ValueError:
                    continue
            idx += min(count, len(scenario_lines) - idx - 1)
        elif line.startswith("EDGES"):
            parts = line.split()
            count = int(parts[1]) if len(parts) > 1 else 0
            for edge_line in scenario_lines[idx + 1 : idx + 1 + count]:
                fields = edge_line.split()
                if len(fields) < 4:
                    continue
                try:
                    edge_id = fields[0]
                    source = int(fields[1])
                    target = int(fields[2])
                    length = float(fields[3])
                except ValueError:
                    continue
                edges.append(
                    {"id": edge_id, "source": source, "target": target, "length": length}
                )
            idx += min(count, len(scenario_lines) - idx - 1)
        idx += 1

    return nodes, edges, time_step


def parse_output_timeline(filepath):
    if not os.path.exists(filepath):
        raise FileNotFoundError(f"Output file {filepath} not found")

    with open(filepath, "r", encoding="utf-8") as fh:
        lines = [line.rstrip("\n") for line in fh]

    timeline_started = False
    timeline_lines = []
    for raw in lines:
        line = raw.strip()
        if not timeline_started:
            if "TIMELINE LOG" in line:
                timeline_started = True
            continue
        if line.startswith("------------------------------------------------"):
            if timeline_lines:
                break
            continue
        if not line or line.startswith("Format:"):
            continue
        timeline_lines.append(line)

    if not timeline_lines:
        raise ValueError("TIMELINE LOG section not found in the output file")

    events = []
    for line in timeline_lines:
        match = re.match(r"t=(\d+)\s*\|\s*(.+)", line)
        if not match:
            continue
        time = int(match.group(1))
        rest = match.group(2).strip()
        enter = re.match(
            r"Group (\S+) size=(\d+) entered edge (\S+)->(\S+)", rest
        )
        arrive = re.match(r"Group (\S+) arrived at (\S+)", rest)
        join_queue = re.match(r"Group (\S+) joined exit queue (\S+)", rest)
        evacuated = re.match(
            r"Exit (\S+) evacuated (\d+) people", rest
        )

        if enter:
            events.append(
                {
                    "time": time,
                    "event_type": "enter_edge",
                    "group_id": enter.group(1),
                    "size": int(enter.group(2)),
                    "source": enter.group(3),
                    "destination": enter.group(4),
                }
            )
        elif arrive:
            events.append(
                {
                    "time": time,
                    "event_type": "arrive_node",
                    "group_id": arrive.group(1),
                    "location": arrive.group(2),
                }
            )
        elif join_queue:
            events.append(
                {
                    "time": time,
                    "event_type": "join_exit_queue",
                    "group_id": join_queue.group(1),
                    "exit": join_queue.group(2),
                }
            )
        elif evacuated:
            events.append(
                {
                    "time": time,
                    "event_type": "exit_evacuated",
                    "exit": evacuated.group(1),
                    "count": int(evacuated.group(2)),
                }
            )

    return events


def build_graph_layout(nodes):
    rooms = [n for n in nodes if n["type"] == "ROOM"]
    junctions = [n for n in nodes if n["type"] == "JUNCTION"]
    exits = [n for n in nodes if n["type"] == "EXIT"]

    spacing = 1.5
    pos = {}

    def assign(column_nodes, x_coord):
        count = len(column_nodes)
        if not count:
            return
        total_height = (count - 1) * spacing
        for idx, node in enumerate(sorted(column_nodes, key=lambda n: n["name"])):
            y = total_height / 2 - idx * spacing
            pos[node["id"]] = (x_coord, y)

    assign(rooms, -2.5)
    assign(junctions, 0.0)
    assign(exits, 2.5)

    return pos


def preprocess_movements(events, node_name_to_id):
    group_events = defaultdict(list)
    for event in events:
        group_id = event.get("group_id")
        if group_id:
            group_events[group_id].append(event)

    movement_map = {}
    for group_id, group_evts in group_events.items():
        for idx, event in enumerate(group_evts):
            if event["event_type"] != "enter_edge":
                continue
            movement = {
                "group_id": group_id,
                "enter_time": event["time"],
                "source_name": event["source"],
                "destination_name": event["destination"],
                "size": event["size"],
                "arrive_time": None,
                "edge": (
                    node_name_to_id.get(event["source"]),
                    node_name_to_id.get(event["destination"]),
                ),
            }
            for later in group_evts[idx + 1 :]:
                if later["event_type"] == "arrive_node":
                    movement["arrive_time"] = later["time"]
                    movement["arrive_location"] = later["location"]
                    break
            if movement["arrive_time"] is None:
                movement["arrive_time"] = movement["enter_time"] + 1
                movement["arrive_location"] = event["destination"]
            movement_map[(group_id, event["time"])] = movement
    return movement_map


def save_animation(anim, gif_path, mp4_path=None, fps=1):
    anim.save(gif_path, writer="pillow", fps=fps)
    print(f" - GIF saved as {gif_path}")
    if mp4_path and animation.writers.is_available("ffmpeg"):
        anim.save(mp4_path, writer="ffmpeg", fps=fps)
        print(f" - MP4 saved as {mp4_path}")
    elif mp4_path:
        print(" - ffmpeg writer not available; skipping MP4 export")


def create_animation(
    nodes,
    edges,
    events,
    movement_map,
    node_name_to_id,
    node_lookup,
    time_step,
):
    G = nx.Graph()
    for node in nodes:
        G.add_node(node["id"], label=node["name"])
    for edge in edges:
        G.add_edge(edge["source"], edge["target"], id=edge["id"])

    pos = build_graph_layout(nodes)
    fig, ax = plt.subplots(figsize=(12, 7))
    fig.subplots_adjust(top=0.85)
    ax.set_title("Evacuation Simulation", fontsize=18, pad=20)
    ax.set_axis_off()
    nx.draw_networkx_nodes(
        G,
        pos,
        nodelist=[node["id"] for node in nodes if node["type"] == "ROOM"],
        node_color="lightblue",
        node_size=600,
        edgecolors="k",
        linewidths=0.5,
        label="Room",
    )
    nx.draw_networkx_nodes(
        G,
        pos,
        nodelist=[node["id"] for node in nodes if node["type"] == "JUNCTION"],
        node_color="lightgray",
        node_size=600,
        edgecolors="k",
        linewidths=0.5,
        label="Junction",
    )
    nx.draw_networkx_nodes(
        G,
        pos,
        nodelist=[node["id"] for node in nodes if node["type"] == "EXIT"],
        node_color="lightgreen",
        node_size=600,
        edgecolors="k",
        linewidths=0.5,
        label="Exit",
    )
    nx.draw_networkx_labels(
        G,
        pos,
        labels={node["id"]: node["name"] for node in nodes},
        font_size=9,
    )
    nx.draw_networkx_edges(G, pos, width=2)
    edge_labels = {
        (edge["source"], edge["target"]): edge["id"] for edge in edges
    }
    nx.draw_networkx_edge_labels(
        G, pos, edge_labels=edge_labels, font_size=8, label_pos=0.5
    )

    legend_elements = [
        Patch(facecolor="lightblue", edgecolor="k", label="Room"),
        Patch(facecolor="lightgray", edgecolor="k", label="Junction"),
        Patch(facecolor="lightgreen", edgecolor="k", label="Exit"),
        plt.Line2D([], [], color="red", marker="o", linestyle="None", label="Moving evacuees"),
    ]
    ax.legend(handles=legend_elements, loc="upper right")

    room_remaining = {
        node["id"]: node["initial_people"]
        for node in nodes
        if node["type"] == "ROOM"
    }
    initial_total_people = sum(room_remaining.values())
    total_evacuated = 0

    events_by_time = defaultdict(list)
    max_time = 0
    for event in events:
        events_by_time[event["time"]].append(event)
        max_time = max(max_time, event["time"])

    group_states = {}

    status_text = ax.text(
        0.02,
        0.95,
        "",
        transform=ax.transAxes,
        fontsize=10,
        ha="left",
        va="top",
        bbox=dict(facecolor="white", alpha=0.6, edgecolor="none"),
    )
    time_text = ax.text(
        0.5,
        0.88,
        "",
        transform=ax.transAxes,
        ha="center",
        va="bottom",
        fontsize=12,
    )

    room_texts = {}
    for node_id, count in room_remaining.items():
        x, y = pos[node_id]
        room_texts[node_id] = ax.text(
            x,
            y - 0.4,
            f"{count} waiting",
            ha="center",
            fontsize=8,
            color="navy",
        )

    group_scatter = ax.scatter([], [], c="red", s=60, zorder=4)

    def process_event(event):
        nonlocal total_evacuated
        if event["event_type"] == "enter_edge":
            group_id = event["group_id"]
            state = group_states.setdefault(group_id, {"size": event["size"]})
            movement = movement_map.get((group_id, event["time"]))
            state.update(
                {
                    "status": "edge",
                    "movement": movement,
                    "size": event["size"],
                }
            )
            source_id = node_name_to_id.get(event["source"])
            if source_id in room_remaining:
                room_remaining[source_id] = max(
                    0, room_remaining[source_id] - event["size"]
                )
        elif event["event_type"] == "arrive_node":
            group_id = event["group_id"]
            state = group_states.setdefault(group_id, {})
            node_id = node_name_to_id.get(event["location"])
            state.update(
                {
                    "status": "node",
                    "last_node": node_id,
                    "movement": None,
                    "arrival_time": event["time"],
                }
            )
            if (
                node_id
                and node_lookup[node_id]["type"] == "EXIT"
            ):
                state["display_until"] = event["time"]
            else:
                state.pop("display_until", None)
        elif event["event_type"] == "exit_evacuated":
            total_evacuated += event["count"]

    max_frame = max_time + 1

    def update_frame(frame):
        current_time = min(frame, max_time)
        for event in events_by_time.get(current_time, []):
            process_event(event)

        positions = []
        sizes = []
        for state in group_states.values():
            if state.get("status") == "edge":
                movement = state.get("movement")
                if not movement:
                    continue
                start = movement.get("edge")[0]
                end = movement.get("edge")[1]
                if start is None or end is None:
                    continue
                enter = movement["enter_time"]
                arrive = movement["arrive_time"]
                progress = (
                    (current_time - enter)
                    / max(1, arrive - enter)
                    if arrive > enter
                    else 1
                )
                progress = max(0.0, min(1.0, progress))
                src = pos[start]
                dst = pos[end]
                x = src[0] + (dst[0] - src[0]) * progress
                y = src[1] + (dst[1] - src[1]) * progress
                positions.append((x, y))
                sizes.append(state.get("size", 1) * 10)
            elif state.get("status") == "node":
                arrival_time = state.get("arrival_time")
                display_until = state.get("display_until")
                node_id = state.get("last_node")
                if (
                    node_id
                    and display_until is not None
                    and current_time == display_until
                ):
                    positions.append(pos[node_id])
                    sizes.append(state.get("size", 1) * 10)

        if positions:
            group_scatter.set_offsets(positions)
            group_scatter.set_sizes(sizes)
        else:
            group_scatter.set_offsets([])
            group_scatter.set_sizes([])

        remaining_to_evacuate = max(0, initial_total_people - total_evacuated)
        time_text.set_text(f"Time: {current_time}s")
        status_text.set_text(
            f"Evacuated: {total_evacuated} / {initial_total_people}   Remaining to evacuate: {remaining_to_evacuate}"
        )
        for node_id, text_obj in room_texts.items():
            text_obj.set_text(f"{room_remaining[node_id]} waiting")

        return [group_scatter, time_text, status_text, *room_texts.values()]

    frame_interval = max(800, int(time_step * 1500))
    anim = animation.FuncAnimation(
        fig,
        update_frame,
        frames=range(max_frame),
        interval=frame_interval,
        blit=False,
        repeat=False,
    )

    fig.savefig("initial_layout.png", bbox_inches="tight")
    print("Saved initial layout snapshot: initial_layout.png")

    return anim, max_frame, time_step


def main():
    parser = argparse.ArgumentParser(
        description="Animate evacuation results using NetworkX and Matplotlib"
    )
    parser.add_argument("--input", default="input.txt", help="Path to scenario input file")
    parser.add_argument("--output", default="output.txt", help="Path to simulator output file")
    parser.add_argument(
        "--gif", default="evacuation_animation.gif", help="Filename for GIF output"
    )
    parser.add_argument(
        "--mp4", default="evacuation_animation.mp4", help="Filename for MP4 output (requires ffmpeg)"
    )
    args = parser.parse_args()

    nodes, edges, time_step = parse_input_file(args.input)
    events = parse_output_timeline(args.output)
    node_name_to_id = {node["name"]: node["id"] for node in nodes}
    node_lookup = {node["id"]: node for node in nodes}
    movement_map = preprocess_movements(events, node_name_to_id)
    anim, total_frames, step = create_animation(
        nodes, edges, events, movement_map, node_name_to_id, node_lookup, time_step
    )

    duration = total_frames * step
    display_fps = 0.5
    save_animation(anim, args.gif, mp4_path=args.mp4, fps=display_fps)
    print(
        f"Animation complete: {total_frames} frames over {duration:.1f}s -> {args.gif}"
    )


if __name__ == "__main__":
    main()

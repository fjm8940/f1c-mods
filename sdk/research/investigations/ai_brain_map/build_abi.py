#!/usr/bin/env python
"""B8: build the AI Brain ABI v1 field registry.

Sources (read-only): offsets_ALL.csv (every hex/dec/type/writer/reader is taken
from the generated CSVs, never hand-typed), the per-function maps, FIELD_GLOSSARY.md,
docs/54-ai-driving-brain-map.md, REVIEW_*.md and findings.jsonl. Output:
ai_brain_abi_v1.json in this directory. No GUI and no DLL code.
"""

from __future__ import annotations

import csv
import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
ALL_CSV = HERE / "offsets_ALL.csv"
FINDINGS = REPO / "findings.jsonl"
OUT = HERE / "ai_brain_abi_v1.json"
ABI11 = HERE / "ai_brain_abi_v1_1.json"
NAMES11 = HERE / "phase2" / "abi_v1_1_names.json"
DOC54 = REPO / "docs" / "54-ai-driving-brain-map.md"

# C6: extra source texts for the v1.1 fields (phase-2 maps/CSVs). v1 stays
# byte-identical: CORE_SOURCES and the v1 code path are not touched.
V11_SOURCES = [
    "offsets_AIMainUpdate.csv",
    "offsets_sub_426430.csv",
    "offsets_AIRacingLine.csv",
    "offsets_AISteering.csv",
    "offsets_AIAbility.csv",
    "offsets_AISteeringRacingLineCorrection.csv",
    "phase2/maps/map_PitStopDecisionTireCompound.md",
    "phase2/maps/map_AIPushLap.md",
    "phase2/maps/map_sub_4C7BD0.md",
    "phase2/maps/offsets_ServerSessionGateCheck.csv",
]

GROUPS = [
    "state", "perception", "plan_speed", "plan_line", "decision",
    "watchdog", "derived_views",
]
BASES = {"veh", "wp", "opp_slot"}
ALIASES = {
    ("AIComputePassScore", "a1"),
    ("OpponentProximityScanTeamRecognition", "v37"),
}
HEX_RE = re.compile(r"0x[0-9A-Fa-f]+")

CORE_SOURCES = [
    "offsets_ALL.csv",
    "FIELD_GLOSSARY.md",
    "map_AIRacingLine.md",
    "map_AIRacingLinePathFollowing.md",
    "map_AIAbility.md",
    "map_AIComputePassScore.md",
    "map_AISteeringRacingLineCorrection.md",
    "map_AISteering_part1.md",
    "map_AISteering_part5.md",
    "map_OpponentProximityScanTeamRecognition.md",
    "map_PhysicsGearShiftLogicWithThresholds.md",
    "REVIEW_1_B1B2.md",
    "REVIEW_2_GROUP1.md",
    "REVIEW_3_GROUP2.md",
    "REVIEW_4_FINAL.md",
    "docs/54-ai-driving-brain-map.md",
    "findings.jsonl",
]

FIELDS = [
    {
        "dec": 17472, "id": "speed", "group": "state", "base": "veh",
        "units": "m/s", "tier": "OBSERVED",
        "meaning": "Car speed computed each tick from the velocity vector; doc 54's live check saw 69-324 km/h and every speed gate uses this value.",
        "source": "docs/54-ai-driving-brain-map.md:15,56",
    },
    {
        "dec": 13552, "id": "yaw", "group": "state", "base": "veh",
        "units": "rad", "tier": "DERIVED",
        "meaning": "Vehicle yaw; the tick derives the heading vector (-sin, -cos) from it.",
        "source": "docs/54-ai-driving-brain-map.md:14",
    },
    {
        "dec": 17680, "id": "driver_status", "group": "state", "base": "veh",
        "units": "enum", "tier": "OBSERVED",
        "meaning": "AI driver/session mode: 0 race, 1 recovery stop, 2 stopped/handed over, 3 pit approach, 4/5 pit-garage wait, 6 reverse, 7 stop. Modes 4/5 park the car (throttle 0, brake 1, steer 0).",
        "source": "docs/54-ai-driving-brain-map.md:88; finding:ai-mode-4-native-park-2026-10-02",
    },
    {
        "dec": 15724, "id": "car_conditions", "group": "state", "base": "veh",
        "units": "enum", "tier": "OBSERVED",
        "meaning": "Vehicle condition status; value 2 = retired/pit mode. While 2 the game keeps re-retiring the car (limbo), so it must be cleared to recover the car.",
        "source": "docs/54-ai-driving-brain-map.md:109; finding:limbo-unlocked-carconditions",
    },
    {
        "dec": 14844, "id": "gear", "group": "state", "base": "veh",
        "units": "gear", "tier": "DERIVED",
        "meaning": "Current gear from the auto-shift logic; 0 while shifting, -1 is the neutral/reverse sentinel.",
        "source": "map_PhysicsGearShiftLogicWithThresholds.md:22; finding:organic-pit-trace-full-field-map",
    },
    {
        "dec": 14852, "id": "gear_max", "group": "state", "base": "veh",
        "units": "gear", "tier": "DERIVED",
        "meaning": "Highest available gear; an upshift is ordered only while the current gear is below this.",
        "source": "map_PhysicsGearShiftLogicWithThresholds.md:29",
    },
    {
        "dec": 14864, "id": "gear_shift_lockout", "group": "state", "base": "veh",
        "units": "s", "tier": "DERIVED",
        "meaning": "Shift lockout timer; while greater than zero the auto-shift logic returns immediately.",
        "source": "map_PhysicsGearShiftLogicWithThresholds.md:21",
    },
    {
        "dec": 13536, "id": "position_x", "group": "state", "base": "veh",
        "units": "m", "tier": "DERIVED",
        "meaning": "Live car world position X used by the locator, steering law and watchdog; the path walk refreshes it and brainlog.py reads it.",
        "source": "FIELD_GLOSSARY.md:18; map_AIRacingLinePathFollowing.md:38",
    },
    {
        "dec": 13540, "id": "position_y", "group": "state", "base": "veh",
        "units": "m", "tier": "DERIVED",
        "meaning": "Live car world position Y used by the locator, steering law and watchdog.",
        "source": "FIELD_GLOSSARY.md:19; map_AIRacingLinePathFollowing.md:39",
    },
    {
        "dec": 13544, "id": "position_z", "group": "state", "base": "veh",
        "units": "m", "tier": "DERIVED",
        "meaning": "Live car world position Z used by the locator, steering law and watchdog.",
        "source": "FIELD_GLOSSARY.md:20; map_AIRacingLinePathFollowing.md:40",
    },
    {
        "dec": 13524, "id": "placement_pose_x", "group": "state", "base": "veh",
        "units": "m", "tier": "GUESS",
        "meaning": "Placement pose X written from the racing-line path node by the path walk. Object kind is OPEN; this is a placement/visual walk, not per-tick driving (REVIEW_4).",
        "source": "map_AIRacingLinePathFollowing.md:35",
    },
    {
        "dec": 13528, "id": "placement_pose_y", "group": "state", "base": "veh",
        "units": "m", "tier": "GUESS",
        "meaning": "Placement pose Y written from the racing-line path node by the path walk (not per-tick driving; REVIEW_4).",
        "source": "map_AIRacingLinePathFollowing.md:36",
    },
    {
        "dec": 13532, "id": "placement_pose_z", "group": "state", "base": "veh",
        "units": "m", "tier": "GUESS",
        "meaning": "Placement pose Z written from the racing-line path node by the path walk (not per-tick driving; REVIEW_4).",
        "source": "map_AIRacingLinePathFollowing.md:37",
    },
    {
        "dec": 13560, "id": "velocity_x", "group": "state", "base": "veh",
        "units": "m/s", "tier": "DERIVED", "type": "float",
        "meaning": "Velocity X; the tick computes the speed field as the length of the velocity vector.",
        "source": "map_AIRacingLine.md:75",
    },
    {
        "dec": 13564, "id": "velocity_y", "group": "state", "base": "veh",
        "units": "m/s", "tier": "DERIVED", "type": "float",
        "meaning": "Velocity Y (see velocity_x).",
        "source": "map_AIRacingLine.md:75",
    },
    {
        "dec": 13568, "id": "velocity_z", "group": "state", "base": "veh",
        "units": "m/s", "tier": "DERIVED", "type": "float",
        "meaning": "Velocity Z (see velocity_x).",
        "source": "map_AIRacingLine.md:75",
    },
    {
        "dec": 17236, "id": "current_waypoint_ptr", "group": "perception", "base": "veh",
        "units": "pointer", "tier": "DERIVED", "type": "_DWORD",
        "meaning": "Pointer to the current racing-line waypoint; the locator re-finds it when the car is far away, and the whole AI tick is skipped when it is null.",
        "source": "docs/54-ai-driving-brain-map.md:15",
    },
    {
        "dec": 17240, "id": "lookahead_waypoints", "group": "perception", "base": "veh",
        "units": "pointer", "tier": "DERIVED", "type": "_DWORD", "count": 19, "stride": 4,
        "meaning": "19 look-ahead waypoint pointers; entries follow the next link, or the alternate branch link when the branch id matches the branch-override field.",
        "source": "docs/54-ai-driving-brain-map.md:16",
    },
    {
        "dec": 17448, "id": "lateral_position", "group": "perception", "base": "veh",
        "units": "m", "tier": "OBSERVED",
        "meaning": "Measured lateral position in the current waypoint frame; doc 54's live check saw -3.78..+2.70 m.",
        "source": "docs/54-ai-driving-brain-map.md:17,63",
    },
    {
        "dec": 17500, "id": "on_track", "group": "perception", "base": "veh",
        "units": "bool", "tier": "DERIVED",
        "meaning": "1 when the car is inside the current waypoint width plus a 1.5 m margin.",
        "source": "docs/54-ai-driving-brain-map.md:85",
    },
    {
        "dec": 16756, "id": "opp_slot_vehicle_ptr", "group": "perception", "base": "opp_slot",
        "units": "pointer", "tier": "GUESS", "count": 5, "stride": 68,
        "meaning": "Per-slot opponent vehicle pointer (first field of the 68-byte slot record, count 5). Cleared each scan, then the five nearest vehicles are inserted.",
        "source": "map_OpponentProximityScanTeamRecognition.md:38",
    },
    {
        "dec": 16808, "id": "opp_slot_distance_sq", "group": "perception", "base": "opp_slot",
        "units": "m^2", "tier": "GUESS", "type": "float", "count": 5, "stride": 68,
        "meaning": "Per-slot squared distance to the opponent (distance field in each slot record); the 999999 sentinel means empty.",
        "source": "map_OpponentProximityScanTeamRecognition.md:39",
    },
    {
        "dec": 17100, "id": "opp_team_match_flag", "group": "perception", "base": "veh",
        "units": "bool", "tier": "GUESS", "type": "_DWORD", "count": 5, "stride": 28,
        "meaning": "Per-team-record match flag (record stride 28); 1 when the record matched this car/team.",
        "source": "map_OpponentProximityScanTeamRecognition.md:48",
    },
    {
        "dec": 17104, "id": "opp_team_id", "group": "perception", "base": "veh",
        "units": "team id", "tier": "GUESS", "count": 5, "stride": 28,
        "meaning": "Team id stored in the team record (+4 in the 28-byte record).",
        "source": "map_OpponentProximityScanTeamRecognition.md:50",
    },
    {
        "dec": 17116, "id": "opp_team_threshold", "group": "perception", "base": "veh",
        "units": "m^2", "tier": "GUESS", "count": 5, "stride": 28, "type": "float",
        "meaning": "Per-team-record threshold float (+12 in the record), reset to 500 by the team-scan reset flag.",
        "source": "map_OpponentProximityScanTeamRecognition.md:52",
    },
    {
        "dec": 17428, "id": "corridor_bound_1", "group": "perception", "base": "veh",
        "units": "m", "tier": "OBSERVED", "type": "float",
        "meaning": "First lateral corridor bound; AISteering clamps the published lateral target between the two bounds. Widening them live let a dead AI car steer off the racing line.",
        "source": "finding:ai-pulloff-lateral-chain-v4-2026-10-02; map_AISteering_part1.md:26",
    },
    {
        "dec": 17432, "id": "corridor_bound_2", "group": "perception", "base": "veh",
        "units": "m", "tier": "OBSERVED", "type": "float",
        "meaning": "Second lateral corridor bound (see corridor_bound_1).",
        "source": "finding:ai-pulloff-lateral-chain-v4-2026-10-02; map_AISteering_part1.md:27",
    },
    {
        "dec": 17668, "id": "corner_speed_force_coeff", "group": "plan_speed", "base": "veh",
        "units": "speed per force", "tier": "DERIVED",
        "meaning": "Multiplier of the tire-force term in the per-waypoint corner-speed limit.",
        "source": "finding:ai-brain-corner-speed-braking-2026-10-02; docs/54-ai-driving-brain-map.md:80",
    },
    {
        "dec": 17664, "id": "corner_speed_force_offset", "group": "plan_speed", "base": "veh",
        "units": "m/s", "tier": "DERIVED",
        "meaning": "Offset added to the tire-force term in the per-waypoint corner-speed limit.",
        "source": "finding:ai-brain-corner-speed-braking-2026-10-02; docs/54-ai-driving-brain-map.md:80",
    },
    {
        "dec": 17792, "id": "corner_speed_scale", "group": "plan_speed", "base": "veh",
        "units": "scale", "tier": "DERIVED",
        "meaning": "Overall scale of the per-waypoint corner-speed limit.",
        "source": "finding:ai-brain-corner-speed-braking-2026-10-02; docs/54-ai-driving-brain-map.md:80",
    },
    {
        "dec": 17724, "id": "ai_pace_multiplier", "group": "plan_speed", "base": "veh",
        "units": "scale", "tier": "DERIVED",
        "meaning": "AIAbility dynamic pace multiplier walked by +-0.01 per tick and reset by car-condition/pit paths; external writes do not persist.",
        "source": "map_AIAbility.md:15; finding:0x453C-recalc",
    },
    {
        "dec": 17316, "id": "speed_profile", "group": "plan_speed", "base": "veh",
        "units": "m/s", "tier": "DERIVED", "type": "float", "count": 20, "stride": 4,
        "meaning": "20-slot projected-speed profile from the braking look-ahead; each slot is the projected speed at a waypoint ahead.",
        "source": "docs/54-ai-driving-brain-map.md:85; map_AIRacingLine.md:78",
    },
    {
        "dec": 17400, "id": "limiting_waypoint_ptr", "group": "plan_speed", "base": "veh",
        "units": "pointer", "tier": "DERIVED",
        "meaning": "Pointer to the waypoint that forces braking now, or null when the projection reaches zero first.",
        "source": "docs/54-ai-driving-brain-map.md:85; map_AIRacingLine.md:79",
    },
    {
        "dec": 17404, "id": "braking_target_speed", "group": "plan_speed", "base": "veh",
        "units": "m/s", "tier": "DERIVED", "type": "float",
        "meaning": "Target speed of the limiting waypoint; the 9999.0 sentinel means none.",
        "source": "docs/54-ai-driving-brain-map.md:85; map_AIRacingLine.md:80",
    },
    {
        "dec": 17612, "id": "brake_decel_factor", "group": "plan_speed", "base": "veh",
        "units": "1/m", "tier": "DERIVED",
        "meaning": "Per-segment deceleration factor used when projecting speed loss from braking.",
        "source": "docs/54-ai-driving-brain-map.md:81; map_AIRacingLine.md:65",
    },
    {
        "dec": 17440, "id": "lateral_request", "group": "plan_line", "base": "veh",
        "units": "m", "tier": "OBSERVED",
        "meaning": "Lateral offset request; zeroed every tick unless the yield target is set or the car is in condition 2. With override-enable it replaces the published target.",
        "source": "finding:ai-lateral-request-4420-zeroed-each-tick-2026-10-02",
    },
    {
        "dec": 17420, "id": "working_lateral_target", "group": "plan_line", "base": "veh",
        "units": "m", "tier": "OBSERVED",
        "meaning": "Working lateral target before the published-target overrides; seeded from the request and clamped to the corridor.",
        "source": "finding:ai-pulloff-lateral-chain-v4-2026-10-02; map_AISteering_part1.md:41",
    },
    {
        "dec": 17736, "id": "override_enable", "group": "plan_line", "base": "veh",
        "units": "bool", "tier": "DERIVED",
        "meaning": "Non-zero means publish the lateral request directly as the steering target.",
        "source": "map_AISteering_part5.md:207; docs/54-ai-driving-brain-map.md:123",
    },
    {
        "dec": 17444, "id": "hard_lateral_override", "group": "plan_line", "base": "veh",
        "units": "m", "tier": "DERIVED",
        "meaning": "Forced lateral offset that wins over both the working target and the request when non-zero.",
        "source": "docs/54-ai-driving-brain-map.md:125; map_AISteering_part5.md:209",
    },
    {
        "dec": 17416, "id": "published_lateral_target", "group": "plan_line", "base": "veh",
        "units": "m", "tier": "OBSERVED", "type": "float",
        "meaning": "Final lateral offset the steering law follows (live-confirmed); set from the working target, then overridden by the request or the forced value.",
        "source": "docs/54-ai-driving-brain-map.md:63,123",
    },
    {
        "dec": 17512, "id": "steering_lookahead_index", "group": "plan_line", "base": "veh",
        "units": "index", "tier": "OBSERVED",
        "meaning": "Look-ahead waypoint index 0..3 chosen from speed; the steering law aims at the waypoint pointer this many entries ahead. Conflict noted in ABI_README (organic pit trace names it a pit-phase enum).",
        "source": "docs/54-ai-driving-brain-map.md:61",
    },
    {
        "dec": 17560, "id": "steering_anchor_x", "group": "plan_line", "base": "veh",
        "units": "m", "tier": "DERIVED", "type": "float",
        "meaning": "Anchor point X recorded at the last waypoint change; the steering law adds the distance driven since the change along the waypoint direction.",
        "source": "docs/54-ai-driving-brain-map.md:18",
    },
    {
        "dec": 17568, "id": "steering_anchor_z", "group": "plan_line", "base": "veh",
        "units": "m", "tier": "DERIVED",
        "meaning": "Anchor point Z recorded at the last waypoint change (see steering_anchor_x).",
        "source": "docs/54-ai-driving-brain-map.md:18",
    },
    {
        "dec": 17588, "id": "steering_lock_divisor", "group": "plan_line", "base": "veh",
        "units": "rad", "tier": "OBSERVED",
        "meaning": "Steering-lock divisor; steer = heading error / this. Live constant 0.4014 rad (about 23 degrees). Writers still OPEN.",
        "source": "docs/54-ai-driving-brain-map.md:60",
    },
    {
        "dec": 17108, "id": "opp_mode", "group": "plan_line", "base": "veh",
        "units": "enum", "tier": "DERIVED", "type": "_DWORD", "count": 5, "stride": 28,
        "meaning": "Per-opponent overtaking mode in the 5-slot table (1/2/3/4/7/8/16; 7/8/16 are attempt states).",
        "source": "map_AISteering_part5.md:25",
    },
    {
        "dec": 17112, "id": "opp_state", "group": "plan_line", "base": "veh",
        "units": "mixed", "tier": "GUESS", "count": 5, "stride": 28, "type": "float",
        "meaning": "Per-opponent working state compared with the per-opponent reset value. Same address is the team record's -9999 sentinel field written by the traffic scan.",
        "source": "FIELD_GLOSSARY.md:48; map_OpponentProximityScanTeamRecognition.md:51",
    },
    {
        "dec": 17460, "id": "cmd_throttle", "group": "decision", "base": "veh",
        "units": "0..1", "tier": "DERIVED", "type": "float",
        "meaning": "Throttle candidate before post-processing (steer lift, traction control, speed limit).",
        "source": "docs/54-ai-driving-brain-map.md:21",
    },
    {
        "dec": 17464, "id": "cmd_brake", "group": "decision", "base": "veh",
        "units": "0..1", "tier": "DERIVED", "type": "float",
        "meaning": "Brake candidate before post-processing.",
        "source": "docs/54-ai-driving-brain-map.md:21",
    },
    {
        "dec": 17452, "id": "cmd_steer", "group": "decision", "base": "veh",
        "units": "-1..1", "tier": "OBSERVED",
        "meaning": "Normalized steering command written by the steering law; live recomputation from the law correlated 0.989.",
        "source": "docs/54-ai-driving-brain-map.md:65",
    },
    {
        "dec": 13692, "id": "act_throttle", "group": "decision", "base": "veh",
        "units": "0..1", "tier": "OBSERVED",
        "meaning": "Throttle actuator copy written at tick end from the throttle candidate (0 in park mode).",
        "source": "docs/54-ai-driving-brain-map.md:28; finding:ai-mode-4-native-park-2026-10-02",
    },
    {
        "dec": 13696, "id": "act_steer", "group": "decision", "base": "veh",
        "units": "-1..1", "tier": "OBSERVED",
        "meaning": "Steering actuator copy written at tick end from the steer command; live-equal to the command in every sample (0 in park mode).",
        "source": "docs/54-ai-driving-brain-map.md:28,65",
    },
    {
        "dec": 13700, "id": "act_brake", "group": "decision", "base": "veh",
        "units": "0..1", "tier": "OBSERVED",
        "meaning": "Brake actuator copy written at tick end from the brake candidate (1 in park mode).",
        "source": "docs/54-ai-driving-brain-map.md:28; finding:ai-mode-4-native-park-2026-10-02",
    },
    {
        "dec": 17540, "id": "speed_limit", "group": "decision", "base": "veh",
        "units": "m/s", "tier": "DERIVED", "type": "float",
        "meaning": "Speed governor input (pit limiter etc.); reset to 1000000.0 every tick before the AI functions, so a write must land between the reset and the pedal post-processor.",
        "source": "docs/54-ai-driving-brain-map.md:72,120",
    },
    {
        "dec": 17592, "id": "throttle_cap_race_start", "group": "decision", "base": "veh",
        "units": "scale", "tier": "DERIVED",
        "meaning": "Throttle smoothing constant used at race start when the race-start byte is set.",
        "source": "docs/54-ai-driving-brain-map.md:74",
    },
    {
        "dec": 17788, "id": "throttle_cap_difficulty", "group": "decision", "base": "veh",
        "units": "0..1", "tier": "DERIVED",
        "meaning": "Difficulty throttle cap = AIDifficultyScalar * 0.75, applied when the interaction target is set.",
        "source": "docs/54-ai-driving-brain-map.md:74; map_AIAbility.md:54",
    },
    {
        "dec": 17516, "id": "watchdog_counter", "group": "watchdog", "base": "veh",
        "units": "count", "tier": "DERIVED",
        "meaning": "Wrong-way/stuck counter; +5 per tick while travel direction opposes yaw, -1 otherwise. Above the limit the mode switches to recovery.",
        "source": "docs/54-ai-driving-brain-map.md:101",
    },
    {
        "dec": 17752, "id": "watchdog_limit", "group": "watchdog", "base": "veh",
        "units": "count", "tier": "DERIVED",
        "meaning": "Counter threshold that triggers the mode-1 recovery stop.",
        "source": "docs/54-ai-driving-brain-map.md:101",
    },
    {
        "dec": 17520, "id": "watchdog_saved_x", "group": "watchdog", "base": "veh",
        "units": "m", "tier": "DERIVED", "type": "float",
        "meaning": "Saved position X used by the 4-second progress check.",
        "source": "docs/54-ai-driving-brain-map.md:102",
    },
    {
        "dec": 17524, "id": "watchdog_saved_y", "group": "watchdog", "base": "veh",
        "units": "m", "tier": "DERIVED", "type": "float",
        "meaning": "Saved position Y used by the 4-second progress check.",
        "source": "FIELD_GLOSSARY.md:81",
    },
    {
        "dec": 17528, "id": "watchdog_saved_z", "group": "watchdog", "base": "veh",
        "units": "m", "tier": "DERIVED", "type": "float",
        "meaning": "Saved position Z used by the 4-second progress check.",
        "source": "docs/54-ai-driving-brain-map.md:102",
    },
    {
        "dec": 17532, "id": "watchdog_timer", "group": "watchdog", "base": "veh",
        "units": "s", "tier": "DERIVED",
        "meaning": "Progress-check timer; when stuck it toggles the mode between normal and reverse attempt.",
        "source": "docs/54-ai-driving-brain-map.md:102",
    },
]

DERIVED_VIEWS = [
    {
        "id": "steering_target_point", "group": "derived_views", "tier": "DERIVED",
        "formula": "waypoint = look-ahead entry current_waypoint_ptr + steering_lookahead_index; target = waypoint.position + published_lateral_target * waypoint.right + distance_since_waypoint_change * waypoint.direction",
        "inputs": ["current_waypoint_ptr", "steering_lookahead_index",
                   "published_lateral_target", "steering_anchor_x", "steering_anchor_z"],
        "source": "docs/54-ai-driving-brain-map.md:34-43",
    },
    {
        "id": "corner_speed_limit", "group": "derived_views", "tier": "DERIVED",
        "formula": "corner_speed = (tire_force_solver(waypoint) * corner_speed_force_coeff + corner_speed_force_offset) * corner_speed_scale * ai_pace_multiplier * waypoint.grip_factor",
        "inputs": ["corner_speed_force_coeff", "corner_speed_force_offset",
                   "corner_speed_scale", "ai_pace_multiplier"],
        "source": "finding:ai-brain-corner-speed-braking-2026-10-02",
    },
    {
        "id": "braking_now", "group": "derived_views", "tier": "DERIVED",
        "formula": "braking while the look-ahead's limiting_waypoint_ptr target (braking_target_speed) sits below the projected speed; then cmd_brake ramps +0.2/tick and cmd_throttle = 0",
        "inputs": ["speed", "speed_profile", "limiting_waypoint_ptr",
                   "braking_target_speed", "cmd_brake", "cmd_throttle"],
        "source": "map_AIRacingLine.md:24-35",
    },
    {
        "id": "intended_vs_true_lateral", "group": "derived_views", "tier": "DERIVED",
        "formula": "true_lateral = dot(car_position - waypoint_position, waypoint.perpendicular); published_lateral_target is the AI's intended offset, not its true lateral position",
        "inputs": ["position_x", "position_z", "published_lateral_target",
                   "current_waypoint_ptr"],
        "source": "finding:ai-free-space-scanner-prototype-2026-10-02",
    },
    {
        "id": "on_track_margin", "group": "derived_views", "tier": "DERIVED",
        "formula": "on_track = (waypoint.half_width_a + 1.5 > lateral_position) and (waypoint.half_width_b + 1.5 > -lateral_position)",
        "inputs": ["on_track", "lateral_position", "current_waypoint_ptr"],
        "source": "docs/54-ai-driving-brain-map.md:85; map_AIRacingLine.md:250",
    },
]


def load_rows() -> dict[int, list[dict[str, str]]]:
    with ALL_CSV.open(encoding="utf-8", newline="") as fh:
        rows = list(csv.DictReader(fh))
    by_dec: dict[int, list[dict[str, str]]] = {}
    for row in rows:
        if row["dec"]:
            by_dec.setdefault(int(row["dec"]), []).append(row)
    return by_dec


def load_finding_ids() -> set[str]:
    ids: set[str] = set()
    with FINDINGS.open(encoding="utf-8") as fh:
        for line in fh:
            line = line.strip()
            if not line:
                continue
            try:
                entry = json.loads(line)
            except json.JSONDecodeError:
                continue
            if "id" in entry:
                ids.add(entry["id"])
    return ids


def load_source_texts() -> dict[str, str]:
    texts: dict[str, str] = {}
    for name in CORE_SOURCES:
        path = REPO / name if name.startswith("docs/") else HERE / name
        if path.exists():
            texts[name] = path.read_text(encoding="utf-8")
    return texts


def load_source_texts_v11(base: dict[str, str]) -> dict[str, str]:
    texts = dict(base)
    for name in V11_SOURCES:
        path = REPO / name if name.startswith("docs/") else HERE / name
        if path.exists():
            texts[name] = path.read_text(encoding="utf-8")
        else:
            raise SystemExit("v1.1 source missing: %s" % name)
    return texts


def load_rows_v11() -> dict[int, list[dict[str, str]]]:
    by_dec = load_rows()
    for path in sorted((HERE / "phase2" / "maps").glob("offsets_*.csv")):
        with path.open(encoding="utf-8", newline="") as fh:
            for row in csv.DictReader(fh):
                if row["dec"]:
                    by_dec.setdefault(int(row["dec"]), []).append(row)
    return by_dec


def check_spec(spec, origin):
    for key in ("id", "group", "units", "tier", "meaning", "source"):
        if key not in spec:
            raise SystemExit("%s[%s] missing %s" % (origin, spec.get("id"), key))
    spec.setdefault("base", "veh")
    if spec["group"] not in GROUPS or spec["group"] == "derived_views":
        raise SystemExit("bad group for %s" % spec["id"])
    if spec["base"] not in BASES:
        raise SystemExit("bad base for %s" % spec["id"])
    if spec["tier"] not in {"OBSERVED", "DERIVED", "GUESS"}:
        raise SystemExit("bad tier for %s" % spec["id"])
    for key, value in spec.items():
        if key != "source" and isinstance(value, str) and HEX_RE.search(value):
            raise SystemExit(
                "%s[%s].%s must not contain hand-typed hex"
                % (origin, spec["id"], key))


def field_rows(by_dec, dec):
    keep = [
        r for r in by_dec.get(dec, [])
        if r["base"] == "this" or (r["func"], r["base"]) in ALIASES
    ]
    if not keep:
        raise SystemExit("no CSV row for dec %d (0x%X)" % (dec, dec))
    return keep


def field_type(rows, override):
    if override:
        return override
    for row in rows:
        if row["access"] == "W" and row["width"]:
            return row["width"]
    for row in rows:
        if row["width"]:
            return row["width"]
    return ""


def funcs(rows, access):
    return sorted({row["func"] for row in rows if row["access"] == access})


def check_source(source, hex_, texts, finding_ids):
    for part in (p.strip() for p in source.split(";")):
        if not part:
            continue
        if part.startswith("finding:"):
            fid = part.split(":", 1)[1]
            if fid not in finding_ids:
                raise SystemExit("unknown finding id %r" % fid)
            continue
        path_name = re.sub(r":\d.*$", "", part)
        if path_name not in texts:
            raise SystemExit("source file not loaded: %s" % path_name)
        if hex_.lower() not in texts[path_name].lower():
            raise SystemExit("hex %s not found in source %s" % (hex_, path_name))


def build_field(by_dec, spec, texts, finding_ids):
    rows = field_rows(by_dec, spec["dec"])
    hex_ = rows[0]["hex"].replace("0X", "0x")
    check_source(spec["source"], hex_, texts, finding_ids)
    writers = funcs(rows, "W")
    readers = funcs(rows, "R")
    out = {
        "id": spec["id"],
        "group": spec["group"],
        "base": spec["base"],
        "hex": hex_,
        "dec": spec["dec"],
        "type": field_type(rows, spec.get("type")),
        "count": spec.get("count", 1),
        "units": spec["units"],
        "meaning": spec["meaning"],
        "tier": spec["tier"],
        "source": spec["source"],
        "writer": writers[0] if writers else None,
        "writers": writers,
        "readers": readers,
    }
    if "stride" in spec:
        out["stride"] = spec["stride"]
    return out


def main() -> int:
    for spec in FIELDS:
        if spec["group"] not in GROUPS or spec["group"] == "derived_views":
            raise SystemExit("bad group for %s" % spec["id"])
        if spec["base"] not in BASES:
            raise SystemExit("bad base for %s" % spec["id"])
        if spec["tier"] not in {"OBSERVED", "DERIVED", "GUESS"}:
            raise SystemExit("bad tier for %s" % spec["id"])
    for spec in FIELDS:
        for key, value in spec.items():
            if key != "source" and isinstance(value, str) and HEX_RE.search(value):
                raise SystemExit(
                    "FIELDS[%s].%s must not contain hand-typed hex"
                    % (spec["id"], key))

    by_dec = load_rows()
    texts = load_source_texts()
    finding_ids = load_finding_ids()
    fields = [build_field(by_dec, spec, texts, finding_ids) for spec in FIELDS]

    ids = [f["id"] for f in fields] + [v["id"] for v in DERIVED_VIEWS]
    if len(ids) != len(set(ids)):
        raise SystemExit("duplicate ids")

    doc = {
        "schema": "f1c.ai_brain_abi.v1",
        "generated_by": "build_abi.py",
        "generated_from": CORE_SOURCES,
        "vehicle": {
            "base_hex": "0x5EC6D8",
            "stride_hex": "0x48C0",
            "driver_name_note": "NUL-terminated driver name string follows the vehicle base; see ABI_README.md.",
        },
        "groups": GROUPS,
        "fields": fields,
        "derived_views": DERIVED_VIEWS,
    }
    OUT.write_text(json.dumps(doc, indent=2) + "\n", encoding="utf-8")
    print("wrote %s: %d fields, %d derived views" % (OUT.name, len(fields), len(DERIVED_VIEWS)))

    if NAMES11.exists():
        names = json.loads(NAMES11.read_text(encoding="utf-8"))["fields"]
        for spec in names:
            check_spec(spec, "NAMES11")
        seen = set()
        for spec in names:
            if spec["dec"] in seen:
                raise SystemExit("duplicate v1.1 dec %d" % spec["dec"])
            seen.add(spec["dec"])
        by_dec11 = load_rows_v11()
        texts11 = load_source_texts_v11(texts)
        fields11 = [build_field(by_dec11, spec, texts11, finding_ids) for spec in names]
        ids11 = [f["id"] for f in fields + fields11] + [v["id"] for v in DERIVED_VIEWS]
        if len(ids11) != len(set(ids11)):
            raise SystemExit("duplicate ids in v1.1")
        doc11 = dict(doc)
        doc11["version"] = "1.1"
        doc11["generated_from"] = CORE_SOURCES + V11_SOURCES
        doc11["fields"] = fields + fields11
        ABI11.write_text(json.dumps(doc11, indent=2) + "\n", encoding="utf-8")
        print("wrote %s: %d fields (%d new), %d derived views" % (
            ABI11.name, len(fields) + len(fields11), len(fields11), len(DERIVED_VIEWS)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

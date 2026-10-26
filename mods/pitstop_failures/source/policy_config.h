static const char policy_config[] = R"ini(; pit_failure_policy_v2.ini — pitstop-modding policy v2 preset.
;
; Development build. Offline policy configuration; not connected to the
; legacy runtime (src/pit_commentary.c) or any game installation.
;
; Units: all *_bp keys are BASIS POINTS (10000 bp = 100%, 1 bp = 0.01%).
; All duration keys are seconds. Do not copy per-mille values from
; config/pit_failures.ini into this file; see docs/POLICY_V2.md.
;
; schema_version must be present and equal to 2. Unknown keys, duplicate
; sections or keys, malformed numbers and out-of-range values are fatal:
; the simulator exits nonzero and never silently falls back to defaults.
;
; Terminology: one "draw" selects at most one outcome per committed stop.
; The first cumulative bucket strictly greater than the draw wins; values
; past the last bucket are a clean stop. The terminal bucket never gains
; mass from context pressure.

[general]
schema_version = 2
; 80..120. Neutral is 100. Per-team values are NOT implemented in v2
; (adapter review required); this is a global preset value.
crew_risk_percent = 100

[probability]
; Recoverable base weights, in bucket order.
slow_corner_bp = 280
wheelgun_bp = 100
fuel_rig_bp = 90
stall_delay_bp = 20
cross_thread_bp = 10
; Entire terminal budget. Not scaled by context. 0..10.
terminal_wheel_bp = 1
; Recoverable sum may never exceed this after contextual scaling.
recoverable_cap_bp = 1200

[duration]
; Triangular(min, mode, max) extra JOB time in seconds.
slow_corner_min = 1.0
slow_corner_mode = 2.5
slow_corner_max = 5.0
wheelgun_min = 3.0
wheelgun_mode = 5.0
wheelgun_max = 10.0
fuel_slow_flow_min = 3.0
fuel_slow_flow_mode = 6.0
fuel_slow_flow_max = 12.0
fuel_temporary_no_flow_min = 4.0
fuel_temporary_no_flow_mode = 8.0
fuel_temporary_no_flow_max = 16.0
fuel_interrupted_flow_min = 3.0
fuel_interrupted_flow_mode = 6.0
fuel_interrupted_flow_max = 12.0
stall_delay_min = 2.0
stall_delay_mode = 4.0
stall_delay_max = 8.0
cross_thread_min = 10.0
cross_thread_mode = 18.0
cross_thread_max = 35.0
terminal_diagnosis_min = 8.0
terminal_diagnosis_mode = 12.0
terminal_diagnosis_max = 20.0

[corner]
; Relative weights (not probabilities). Zero means never selected.
fl_weight = 25
fr_weight = 25
rl_weight = 25
rr_weight = 25

[fuel_subtypes]
; Relative conditional weights within the fuel_rig bucket.
; v2 has no spill or fire subtype.
slow_flow_weight = 70
temporary_no_flow_weight = 20
interrupted_flow_weight = 10

[context]
; Additive percentage points of the risk multiplier (not of absolute risk).
; Applied only when the corresponding flag is explicitly true; false and
; unknown add zero.
late_request_bonus = 15
late_change_bonus = 15
wet_bonus = 10
positioning_bonus = 25
)ini";
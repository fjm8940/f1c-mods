@echo off
REM INTERNAL IMPLEMENTATION. Public callers use: python tools\f1c_mod.py sdk-test
setlocal
cd /d "%~dp0\..\..\.."
if not defined F1C_SDK_TEST_NO_REGEN (
python tools\gen_sdk_from_index.py || exit /b 1
python tools\gen_vehicle_struct_header.py || exit /b 1
python tools\gen_trace_schema.py || exit /b 1
python tools\gen_reviewed_cpp_api.py || exit /b 1
python tools\build_recovery_index.py || exit /b 1
)
python tools\sdk\validate_sdk.py || exit /b 1
python tools\sdk\tests\test_canonical_front_door.py || exit /b 1
python tools\sdk\tests\test_tool_catalog.py || exit /b 1
python tools\sdk\tests\test_gmouse_pid.py || exit /b 1
python tools\sdk\tests\test_safety_car_interfaces.py || exit /b 1
python tools\sdk\tests\test_session_guard.py || exit /b 1
python tools\sdk\tests\test_safety_car_guardrails.py || exit /b 1
python tools\sdk\tests\test_vehicle_lifecycle.py || exit /b 1
python tools\sdk\tests\test_vehicle_native_clear.py || exit /b 1
python tools\sdk\tests\test_safety_car_guardrails_syntax.py || exit /b 1
python tools\sdk\tests\test_restart_guard_chain.py || exit /b 1
python tools\sdk\tests\test_preload.py || exit /b 1
python tools\sdk\tests\test_local_yellow.py || exit /b 1
python tools\sdk\tests\test_placement.py || exit /b 1
python tools\sdk\tests\test_garage_results.py || exit /b 1
call tools\sdk\tests\build_smoke.bat || exit /b 1
call mods\sdk_sandbox\game_update_broker\build_compatibility_matrix.bat || exit /b 1
call mods\sdk_sandbox\game_update_broker\build_broker_regressions.bat || exit /b 1
python tools\sdk\tests\test_recovery_index.py || exit /b 1
python tools\sdk\tests\test_source_explorer.py || exit /b 1
python tools\sdk\tests\test_status_consistency.py || exit /b 1
python tools\sdk\tests\test_ask_recorded_findings.py || exit /b 1
python tools\sdk\tests\test_ida_bridge.py || exit /b 1
python tools\sdk\tests\test_ida_external_targets.py || exit /b 1
python tools\sdk\tests\test_address_harvest.py || exit /b 1
python tools\sdk\tests\test_address_tiering.py || exit /b 1
python tools\sdk\tests\test_new_mod_workflow.py || exit /b 1
python tools\sdk\tests\test_mod_orchestrator.py || exit /b 1
python tools\sdk\tests\test_variant_resolver.py || exit /b 1
python tools\sdk\tests\test_runtime_validator.py || exit /b 1
python tools\sdk\tests\test_qualifying_order.py || exit /b 1
python tools\sdk\tests\test_shadow_source.py || exit /b 1
python tools\sdk\tests\test_shadow_architecture.py || exit /b 1
python tools\sdk\tests\test_shadow_reconcile.py || exit /b 1

python tools\sdk\tests\test_ai_interaction.py || exit /b 1

python tools\sdk\tests\test_car_restart.py || exit /b 1

python tools\sdk\tests\test_callback_recovery.py || exit /b 1

python tools\sdk\tests\test_pit_marker.py || exit /b 1

python tools\sdk\tests\test_pit_marker_read.py || exit /b 1

python tools\sdk\tests\test_surface_contact.py || exit /b 1

python tools\sdk\tests\test_weather.py || exit /b 1


python tools\sdk\tests\test_ai_restart_pending.py || exit /b 1
python tools\sdk\tests\test_safety_car_status.py || exit /b 1
python tools\sdk\tests\test_start_stall.py || exit /b 1
python tools\sdk\tests\test_engine_motion.py || exit /b 1

python tools\sdk\tests\test_race_control.py || exit /b 1
python tools\sdk\tests\test_race_control_abi.py || exit /b 1
python tools\sdk\tests\test_race_control_mixed_abi.py || exit /b 1
python tools\sdk\tests\test_restart_penalties.py || exit /b 1
python tools\sdk\tests\test_restart_fuel.py || exit /b 1
python tools\sdk\tests\test_broker_penalty_hold.py || exit /b 1
python tools\sdk\tests\test_sc_fault.py || exit /b 1
python tools\sdk\tests\test_native_removal_repair.py || exit /b 1
python tools\sdk\tests\test_ai_engine_stop.py || exit /b 1
python tools\sdk\tests\test_inverted_removal.py || exit /b 1
python tools\sdk\tests\test_sc_pit_authority.py || exit /b 1
python tools\sdk\tests\test_memory_mcp.py || exit /b 1

python tools\sdk\tests\test_race_data_observation.py || exit /b 1
python tools\sdk\tests\test_sc_replay_ownership.py || exit /b 1

python tools\sdk\tests\test_replay_live_instances.py || exit /b 1
echo All canonical SDK tests passed.

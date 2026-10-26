# Live-debugging MCP servers — reference (added 2026-07-24)

Three MCP servers give an LLM session direct live control of the running game
(`F1 Challenge Online.exe`, non-ASLR base 0x400000). They are DEFERRED — load a
tool's schema with ToolSearch (`select:<tool_name>`) before first call, or search
by keyword. All three attach to the SAME process; avoid running x64dbg + Frida
heavy work simultaneously (both are debuggers; the ptrace-level x64dbg wins).

Get the PID first: `tasklist //FI "IMAGENAME eq F1 Challenge Online.exe" //FO CSV //NH`.
x64dbg wants the PID in HEX (e.g. 24480 -> `attach 5FA0`).

---
## 1. `mcp__frida__*` — Frida (scripting / hooks / reads)  ← best for reads & calling funcs
Session model: `get_process_by_name` -> `create_interactive_session(pid)` ->
`execute_in_session(session_id, js)`.

Key tools: get_process_by_name, attach_to_process, create_interactive_session,
execute_in_session, list_processes, resume_process, kill_process.

Good for:
- Reading/writing memory cleanly: `ptr(a).readS32()/.readU32()/.readFloat()/.readCString()`,
  `.writeS32()/.writeU8()/.writeFloat()`. (Modern API — NOT `Memory.readU32`.)
- Calling game functions: `new NativeFunction(ptr(0x42AF40),'int',['pointer'],'thiscall')`
  then `fn(ptr(veh))`. thiscall = `this` in ecx. Runs on a Frida thread (usually OK for
  one-shot; deep game funcs may need the game thread — piggyback via Interceptor).
- Hooks: `Interceptor.attach(ptr(addr),{onEnter(a){ this.context.ecx ... }})`.

**HARD GOTCHAS (cost hours this session):**
- **globalThis / any variable does NOT persist between execute_in_session calls.**
  Each snippet is a throwaway script; its Interceptor is torn down when the call
  returns. => a hook that must observe a later in-game event MUST live inside ONE
  call: attach, `Thread.sleep(<seconds>)` to stay alive collecting, detach, return.
- `Thread.sleep` > ~15s exceeds the MCP call timeout ("timeout was reached"). Keep
  blocking captures short; repeat if needed.
- Session dies if the game restarts (PID changes) — recreate it.
- Returned objects stringify to "[object Object]" — wrap results in `JSON.stringify(...)`.
- readByteArray -> convert to hex manually for display.

---
## 2. `mcp__x64dbg__*` — x64dbg / x32dbg (breakpoints, stepping, disasm)  ← best for "who wrote this"
This is `x32dbg` (32-bit). Repo: github.com/john-mayhem/x32dbgMCP. Already wired as an MCP.
Attach: `ExecCommand("attach <PID_HEX>")`. Detach: `ExecCommand("detach")`.
Check: `IsDebugging` (session attached), `IsDebugActive` (true=running / false=paused).

Key tools: ExecCommand, IsDebugging, IsDebugActive, GetRegisterDump, GetCallStack,
GetBreakpointList, GetModuleList, MemoryRead, MemoryWrite, SetHardwareBreakpoint,
DebugRun, plus Register/Flag/Stack/Disasm/Patch/Label/Xref helpers.

**Breakpoints PERSIST across the process (unlike Frida hooks)** — this is why x64dbg is
the right tool for timing-tight captures (catch an event whenever it eventually fires).

Recipes proven this session:
- HW write BP that halts only on a specific value:
  `bphws <addr>,w` then `SetHardwareBreakpointCondition <addr>, "[<addr>]==2"`.
  (Caught the retire writer: BP on FinishState 0x5F98BC, condition `==2`.)
- Software BP with a rich condition on a hot dispatcher, to catch one slot's commands:
  `bp 4B3400` then
  `SetBreakpointCondition 4B3400, "byte:[ecx]==7 && (dword:[ecx]>>29)==4 && ((dword:[ecx]>>18)&0x3f)!=8"`.
  Condition syntax supports `byte:[reg]`, `dword:[reg]`, shifts, masks, `&&`.
- On hit: `GetRegisterDump` (cip = writer/callsite), `MemoryRead` the operand,
  `GetCallStack` (first ~4 frames real, rest is stack-scan noise).
- Resume: `ExecCommand("run")` (more reliable than the DebugRun script API, which
  went flaky when a plain BP re-hit repeatedly).
- Remove BP: `bphwc <addr>` (hardware) / `bc <addr>` (software).

**GOTCHAS:**
- Map a caught `cip` to a corpus function: greatest `functions/*_<addr>.c` whose start
  <= cip. (Caught 0x4bbfea -> PitCrewStateMachine_004bbfc0.)
- Plain (unconditioned) write BPs re-hit on every write and, combined with flaky
  script-API resume, left the game frozen — always condition the BP to the exact
  value/state you want, so resets/noise auto-skip.
- Attaching freezes the game; detaching resumes it (use detach to unstick a wedged game).
- Frida agent leaves TLS-callback BPs visible in GetBreakpointList — ignore those.

---
## 3. `mcp__cheatengine__*` — Cheat Engine (scanning / structures / assembly)  ← not needed this session
Large surface: aob_scan*, scan_all/next_scan, create/dissect_structure,
read/write_integer/memory/string, set_hardware/data_breakpoint, assemble_instruction,
auto_assemble, allocate_memory, inject_dll, find_references, generate_code_injection_script,
disassemble, get_address_list, pointer_rescan, etc.
Use for: AOB pattern scans, CE-style struct dissection across instances, code caves /
assembly injection. Not exercised this session but available and full-featured.

---
## Which to reach for
- Read/write a value, call a function, quick hook → **Frida** (one blocking call).
- "What function writes/reads X", catch an event by breakpoint, step/trace → **x64dbg**.
- AOB scan for a pattern, CE-table-style struct work, asm injection → **Cheat Engine**.
- Bulk struct dissection into the corpus → the local `tools/runtime_extract.py`
  (built on raw ReadProcessMemory, no MCP needed).

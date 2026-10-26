#!/usr/bin/env python3
"""Attach Frida to the running game, load a watch agent, stream its CSV rows.

Usage:
    python watch_run.py investigations/ai_retire/ai_retire_watch.js
    python watch_run.py <agent.js> --seconds 60 --out watch.csv
    python watch_run.py <agent.js> --interactive   # type commands while streaming

Interactive commands (agent must export matching rpc):
    poke <offset> <value>       e.g.  poke 16488 5    (write +0x4068=5 to all AI)
    dump <idx> <start> <end> <step>   e.g.  dump 1 16400 16600 4
    q                           quit

Prints every CSV_HEADER / CSV row the agent emits via send(). Ctrl-C to stop.
"""
import argparse, sys, time
import frida

PROCESS = "F1 Challenge Online.exe"

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("agent")
    ap.add_argument("--seconds", type=float, default=0, help="0 = run until Ctrl-C")
    ap.add_argument("--out", default=None)
    ap.add_argument("--interactive", action="store_true", help="read poke/dump commands from stdin")
    args = ap.parse_args()

    src = open(args.agent, "r", encoding="utf-8").read()
    if not args.out and args.interactive:
        args.out = "watch_capture.csv"
    out = open(args.out, "w", encoding="utf-8") if args.out else None
    if out:
        print(f"[watch_run] full stream -> {args.out}", file=sys.stderr)

    try:
        session = frida.attach(PROCESS)
    except frida.ProcessNotFoundError:
        print(f"ERROR: '{PROCESS}' not running. Start a race first.", file=sys.stderr)
        sys.exit(1)

    state = {"last_key": None, "header": None}

    def on_message(msg, data):
        if msg.get("type") == "send":
            p = msg.get("payload", "")
            if isinstance(p, str) and p.startswith("LOG:"):
                # command results / pokes — always show
                print("  >> " + p[4:], flush=True)
                if out: out.write(p[4:] + "\n"); out.flush()
                return
            line = None
            if isinstance(p, str) and p.startswith("CSV_HEADER:"):
                line = p[len("CSV_HEADER:"):]
                state["header"] = line
                print(line, flush=True)
                if out: out.write(line + "\n"); out.flush()
                return
            elif isinstance(p, str) and p.startswith("CSV:"):
                line = p[len("CSV:"):]
            else:
                print(str(p), flush=True); return

            state["last_line"] = line
            if out:
                out.write(line + "\n"); out.flush()
            # interactive mode: DO NOT print the stream (it would bury typed input).
            # Stream lives in the --out file; console is the command prompt only.
            if not args.interactive:
                print(line, flush=True)
        elif msg.get("type") == "error":
            print("[agent error]", msg.get("stack") or msg.get("description"), file=sys.stderr)

    script = session.create_script(src)
    script.on("message", on_message)
    script.load()
    print(f"[watch_run] attached to {PROCESS}, streaming CSV. Ctrl-C to stop.", file=sys.stderr)

    HELP = ("cmds: row | header | poke <off> <val> | dump <idx> <start> <end> <step> | q\n"
            "  row    print latest streamed values\n"
            "  poke   write S32 <val> at <off> for every AI car (skips player)\n"
            "  dump   read a field window of one car block\n")

    def repl():
        print(HELP, flush=True)
        for line in sys.stdin:
            parts = line.split()
            if not parts:
                continue
            cmd = parts[0].lower()
            try:
                if cmd in ("q", "quit", "exit"):
                    break
                elif cmd in ("help", "?"):
                    print(HELP, flush=True)
                elif cmd == "header":
                    print(state.get("header"), flush=True)
                elif cmd == "row":
                    print(state.get("header"), flush=True)
                    print(state.get("last_line"), flush=True)
                elif cmd == "poke" and len(parts) == 3:
                    n = script.exports_sync.poke(int(parts[1]), int(parts[2]))
                    print(f"[poke] wrote to {n} AI cars", flush=True)
                elif cmd == "dump" and len(parts) == 5:
                    r = script.exports_sync.dump(int(parts[1]), int(parts[2]), int(parts[3]), int(parts[4]))
                    print(f"[dump] {r}", flush=True)
                else:
                    print(HELP, flush=True)
            except Exception as e:
                print("[cmd error]", e, flush=True)

    try:
        if args.interactive:
            repl()
        elif args.seconds > 0:
            time.sleep(args.seconds)
        else:
            while True:
                time.sleep(0.5)
    except KeyboardInterrupt:
        pass
    finally:
        try: script.unload()
        except Exception: pass
        session.detach()
        if out: out.close()

if __name__ == "__main__":
    main()

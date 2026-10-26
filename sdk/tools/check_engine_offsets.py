"""Find LATCHED offsets in engine power region of vehicle struct."""
import json

with open("persistence_map.jsonl") as f:
    pm = [json.loads(l) for l in f]

print("LATCHED offsets in engine region (0x3950-0x3AB0):")
for entry in pm:
    off = int(entry["off"], 16)
    if 0x3950 <= off <= 0x3AB0 and entry.get("verdict") == "LATCHED":
        su = entry.get("sample_u32", "?")
        print(f"  0x{off:04X}  sample_u32=0x{su:>10}")

print("\nAll verdicts in same region:")
for entry in pm:
    off = int(entry["off"], 16)
    if 0x3950 <= off <= 0x3AB0:
        v = entry.get("verdict", "?")
        su = entry.get("sample_u32", "?")
        print(f"  0x{off:04X}  {v:<15} sample_u32=0x{su:>10}")

#!/usr/bin/env python3
"""Read-only, page-deduplicated process snapshots and state diffs."""
from pathlib import Path
import ctypes
import ctypes.wintypes as wt
import hashlib
import json
import os
import struct
import time
import zlib


PAGE_SIZE = 0x1000
MEM_COMMIT = 0x1000
PAGE_GUARD = 0x100
PAGE_NOACCESS = 0x01
PROCESS_ACCESS = 0x0400 | 0x0010


class SnapshotError(RuntimeError):
    pass


class PROCESSENTRY32W(ctypes.Structure):
    _fields_ = [("dwSize", wt.DWORD), ("cntUsage", wt.DWORD),
                ("th32ProcessID", wt.DWORD), ("th32DefaultHeapID", ctypes.c_size_t),
                ("th32ModuleID", wt.DWORD), ("cntThreads", wt.DWORD),
                ("th32ParentProcessID", wt.DWORD), ("pcPriClassBase", wt.LONG),
                ("dwFlags", wt.DWORD), ("szExeFile", wt.WCHAR * 260)]


class MODULEENTRY32W(ctypes.Structure):
    _fields_ = [("dwSize", wt.DWORD), ("th32ModuleID", wt.DWORD),
                ("th32ProcessID", wt.DWORD), ("GlblcntUsage", wt.DWORD),
                ("ProccntUsage", wt.DWORD), ("modBaseAddr", ctypes.POINTER(wt.BYTE)),
                ("modBaseSize", wt.DWORD), ("hModule", wt.HMODULE),
                ("szModule", wt.WCHAR * 256), ("szExePath", wt.WCHAR * 260)]


class MEMORY_BASIC_INFORMATION(ctypes.Structure):
    _fields_ = [("BaseAddress", ctypes.c_void_p), ("AllocationBase", ctypes.c_void_p),
                ("AllocationProtect", wt.DWORD), ("PartitionId", wt.WORD),
                ("RegionSize", ctypes.c_size_t), ("State", wt.DWORD),
                ("Protect", wt.DWORD), ("Type", wt.DWORD)]


def _kernel32():
    if os.name != "nt":
        raise SnapshotError("live process snapshots require Windows")
    return ctypes.windll.kernel32


def find_process(name):
    kernel = _kernel32()
    snapshot = kernel.CreateToolhelp32Snapshot(0x2, 0)
    entry = PROCESSENTRY32W(); entry.dwSize = ctypes.sizeof(entry)
    try:
        if kernel.Process32FirstW(snapshot, ctypes.byref(entry)):
            while True:
                if entry.szExeFile.casefold() == name.casefold():
                    return int(entry.th32ProcessID)
                if not kernel.Process32NextW(snapshot, ctypes.byref(entry)):
                    break
    finally:
        kernel.CloseHandle(snapshot)
    raise SnapshotError(f"process is not running: {name}")


def main_module(pid):
    kernel = _kernel32()
    snapshot = kernel.CreateToolhelp32Snapshot(0x8 | 0x10, pid)
    if snapshot == -1:
        error = kernel.GetLastError()
        if error == 5:
            raise SnapshotError("access denied while enumerating modules; run Codex/terminal at the game's integrity level")
        raise SnapshotError(f"module snapshot failed with Windows error {error}")
    entry = MODULEENTRY32W(); entry.dwSize = ctypes.sizeof(entry)
    try:
        if not kernel.Module32FirstW(snapshot, ctypes.byref(entry)):
            raise SnapshotError("could not enumerate the main module")
        return {"name": entry.szModule, "path": entry.szExePath,
                "base": ctypes.cast(entry.modBaseAddr, ctypes.c_void_p).value,
                "size": int(entry.modBaseSize)}
    finally:
        kernel.CloseHandle(snapshot)


def memory_regions(handle):
    kernel = _kernel32()
    address = 0
    maximum = 0x80000000
    while address < maximum:
        info = MEMORY_BASIC_INFORMATION()
        result = kernel.VirtualQueryEx(handle, ctypes.c_void_p(address), ctypes.byref(info),
                                       ctypes.sizeof(info))
        if not result:
            break
        base = info.BaseAddress or 0
        size = int(info.RegionSize)
        if not size:
            break
        yield {"base": base, "size": size, "allocation_base": info.AllocationBase or 0,
               "state": int(info.State), "protect": int(info.Protect), "type": int(info.Type)}
        address = base + size


def read_memory(handle, address, size):
    kernel = _kernel32()
    buffer = ctypes.create_string_buffer(size)
    received = ctypes.c_size_t()
    if not kernel.ReadProcessMemory(handle, ctypes.c_void_p(address), buffer, size,
                                    ctypes.byref(received)):
        return None
    return buffer.raw[:received.value] if received.value == size else None


def _store_page(store, content):
    digest = hashlib.sha256(content).hexdigest().upper()
    if any(content):
        target = store / f"{digest}.zlib"
        if not target.exists():
            target.write_bytes(zlib.compress(content, level=9))
    return digest, not any(content)


def capture_snapshot(root, tag, *, process_name="F1 Challenge Online.exe", scope="module"):
    destination = root / "runtime_memory" / tag
    if destination.exists():
        raise SnapshotError(f"refusing to overwrite snapshot: {destination}")
    pid = find_process(process_name)
    kernel = _kernel32()
    handle = kernel.OpenProcess(PROCESS_ACCESS, False, pid)
    if not handle:
        error = kernel.GetLastError()
        if error == 5:
            raise SnapshotError("OpenProcess access denied; run Codex/terminal at the game's integrity level")
        raise SnapshotError(f"OpenProcess failed with Windows error {error}")
    try:
        module = main_module(pid)
    except Exception:
        kernel.CloseHandle(handle)
        raise
    store = root / "runtime_memory" / "_page_store"
    destination.mkdir(parents=True)
    store.mkdir(parents=True, exist_ok=True)
    pages = []
    regions = []
    unreadable = 0
    try:
        for region in memory_regions(handle):
            readable = (region["state"] == MEM_COMMIT and
                        not region["protect"] & (PAGE_GUARD | PAGE_NOACCESS))
            in_module = (module["base"] <= region["base"] < module["base"] + module["size"])
            selected = readable and (scope == "process" or in_module)
            regions.append({**region, "selected": selected, "in_main_module": in_module})
            if not selected:
                continue
            end = region["base"] + region["size"]
            for address in range(region["base"], end, PAGE_SIZE):
                size = min(PAGE_SIZE, end - address)
                content = read_memory(handle, address, size)
                if content is None:
                    unreadable += 1
                    continue
                digest, zero = _store_page(store, content)
                page = {"address": f"0x{address:08X}", "size": size,
                        "sha256": digest, "zero": zero}
                if module["base"] <= address < module["base"] + module["size"]:
                    page["module"] = module["name"]
                    page["rva"] = f"0x{address - module['base']:08X}"
                pages.append(page)
    finally:
        kernel.CloseHandle(handle)
    module_path = Path(module["path"])
    module["base"] = f"0x{module['base']:08X}"
    if module_path.is_file():
        module["sha256"] = hashlib.sha256(module_path.read_bytes()).hexdigest().upper()
    manifest = {
        "schema_version": 1, "tag": tag, "captured": time.strftime("%Y-%m-%dT%H:%M:%S"),
        "trust": "OBSERVED", "process": process_name, "pid": pid, "scope": scope,
        "page_size": PAGE_SIZE, "store": str(store.resolve()), "main_module": module,
        "regions": regions, "pages": pages,
        "summary": {"pages": len(pages), "unique_hashes": len({page['sha256'] for page in pages}),
                    "zero_pages": sum(page["zero"] for page in pages),
                    "unreadable_pages": unreadable,
                    "logical_bytes": sum(page["size"] for page in pages)},
        "policy": "Absolute addresses are snapshot-local; prefer module RVA or object offset-path identities.",
    }
    (destination / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    return destination / "manifest.json", manifest


def load_manifest(root, value):
    candidate = Path(value)
    if not candidate.is_absolute() and len(candidate.parts) == 1:
        candidate = root / "runtime_memory" / value / "manifest.json"
    elif candidate.is_dir():
        candidate = candidate / "manifest.json"
    if not candidate.is_file():
        raise SnapshotError(f"snapshot manifest not found: {candidate}")
    return candidate.resolve(), json.loads(candidate.read_text(encoding="utf-8"))


def _page_key(page):
    if page.get("module") is not None and page.get("rva") is not None:
        return f"module:{page['module'].casefold()}:{page['rva'].lower()}"
    return f"address:{page['address'].lower()}"


def _page_content(manifest, page):
    if page.get("zero"):
        return bytes(page["size"])
    store = Path(manifest["store"])
    path = store / f"{page['sha256']}.zlib"
    if not path.is_file():
        raise SnapshotError(f"page blob missing: {path}")
    return zlib.decompress(path.read_bytes())


def diff_snapshots(root, before_value, after_value, *, transition=None, width=1,
                   limit=1000):
    before_path, before = load_manifest(root, before_value)
    after_path, after = load_manifest(root, after_value)
    left = {_page_key(page): page for page in before["pages"]}
    right = {_page_key(page): page for page in after["pages"]}
    changed_keys = sorted(key for key in left.keys() & right.keys()
                          if left[key]["sha256"] != right[key]["sha256"])
    transitions = []
    byte_runs = []
    for key in changed_keys:
        old = _page_content(before, left[key]); new = _page_content(after, right[key])
        different = [index for index in range(min(len(old), len(new))) if old[index] != new[index]]
        if different:
            starts = [different[0]]
            for previous, current in zip(different, different[1:]):
                if current != previous + 1:
                    starts.append(current)
            byte_runs.append({"key": key, "changed_bytes": len(different),
                              "first_offsets": [f"0x{value:X}" for value in starts[:20]]})
        if transition is not None:
            old_value, new_value = transition
            fmt = {1: "<B", 2: "<H", 4: "<I"}[width]
            for offset in range(0, min(len(old), len(new)) - width + 1, width):
                if struct.unpack_from(fmt, old, offset)[0] == old_value and \
                   struct.unpack_from(fmt, new, offset)[0] == new_value:
                    address = int(left[key]["address"], 16) + offset
                    item = {"key": key, "page_offset": f"0x{offset:X}",
                            "snapshot_address": f"0x{address:08X}",
                            "transition": f"{old_value}->{new_value}", "width": width}
                    if left[key].get("rva"):
                        item["module_rva"] = f"0x{int(left[key]['rva'], 16) + offset:08X}"
                    transitions.append(item)
                    if len(transitions) >= limit:
                        break
    return {
        "schema_version": 1, "before": str(before_path), "after": str(after_path),
        "same_process": before.get("pid") == after.get("pid"),
        "same_module_sha256": before.get("main_module", {}).get("sha256") == after.get("main_module", {}).get("sha256"),
        "summary": {"changed_pages": len(changed_keys),
                    "added_pages": len(right.keys() - left.keys()),
                    "removed_pages": len(left.keys() - right.keys()),
                    "transition_candidates": len(transitions)},
        "changed_pages": byte_runs[:limit], "transition_candidates": transitions,
        "policy": "Candidates require repeated state cycles and writer evidence before semantic naming.",
    }

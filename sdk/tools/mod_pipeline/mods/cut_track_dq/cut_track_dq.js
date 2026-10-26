// Mod: Cutting the track = instant Disqualification (live, reversible).
// Applies the same 3 byte edits as the static patch, into the running process.
// Redirects both cut-track penalty calls in FUN_004bccc0 -> DQ handler FUN_004bc0e0.
//
// Run: frida -p <F1C pid> -l cut_track_dq.js
// Reversible: just don't load it (or run restore() to undo in-session).
'use strict';

const EDITS = [
  { va: '0x004bd0aa', orig: '6a0a',       patch: '9090'       }, // NOP push 0xA
  { va: '0x004bd0b3', orig: 'e8a8ecffff', patch: 'e828f0ffff' }, // stopgo -> DQ
  { va: '0x004bd0c1', orig: 'e89aebffff', patch: 'e81af0ffff' }, // warning -> DQ
];

function hexToBytes(h) {
  const a = [];
  for (let i = 0; i < h.length; i += 2) a.push(parseInt(h.substr(i, 2), 16));
  return a;
}

function apply() {
  for (const e of EDITS) {
    const p = ptr(e.va);
    const cur = p.readByteArray(e.orig.length / 2);
    const curHex = Array.from(new Uint8Array(cur))
      .map(b => b.toString(16).padStart(2, '0')).join('');
    if (curHex !== e.orig) {
      console.log('[cut_track_dq] ABORT: ' + e.va + ' expected ' + e.orig +
                  ' found ' + curHex + ' (wrong version or already patched)');
      return;
    }
  }
  for (const e of EDITS) {
    const p = ptr(e.va);
    Memory.protect(p, e.patch.length / 2, 'rwx');
    p.writeByteArray(hexToBytes(e.patch));
    console.log('[cut_track_dq] patched ' + e.va + ': ' + e.orig + ' -> ' + e.patch);
  }
  console.log('[cut_track_dq] active — any track cut now disqualifies.');
}

globalThis.restore = function () {
  for (const e of EDITS) {
    const p = ptr(e.va);
    Memory.protect(p, e.orig.length / 2, 'rwx');
    p.writeByteArray(hexToBytes(e.orig));
  }
  console.log('[cut_track_dq] restored original bytes.');
};

apply();

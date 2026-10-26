# `AISteering` to `Overtaking` call-boundary evidence

Target SHA-256:
`1A70E5ED0C4A059F7F66741B1682D0AEDB14921AA6140CDB70B09549FDF3A003`.

The canonical bounded executable-byte reader found exactly two direct calls
from `AISteering_004270A0` to `Overtaking_0042AF70`, at `0x004290CC` and
`0x0042923C`.

Both call sites have the same setup:

```text
mov ecx, [edi+0x4174]   ; candidate vehicle from the current nearby entry
push ebp                ; candidate lane/mode
push esi                ; current/evaluating AI vehicle
call 0x0042AF70
```

The callee returns with `ret 8`, proving two stack arguments. Its prologue
preserves registers and then reads `[esp+0x10]`, which resolves to the first
stack argument (the current/evaluating vehicle) after the three register
pushes. The ABI is therefore:

```text
int __thiscall Overtaking(
    F1CVehicle *candidate_as_this,  // ECX
    F1CVehicle *evaluating_car,     // first stack argument
    int candidate_lane_mode);       // second stack argument
```

This resolves the apparent Hex-Rays mismatch: its method-style call hides the
candidate object carried in `ECX`. It also reverses the former intuitive field
direction. A successful call writes `candidate+0x45FC = evaluating_car` and
the lateral directive on the candidate object. The holder of `+0x45FC` is the
car being challenged; the pointer identifies the challenger. It is not the
challenger's own target field.

Confidence: machine-code-confirmed for this exact executable. The complete
behavioral contract and a safe hook boundary remain unreviewed, so this does
not authorize calling or detouring the function.

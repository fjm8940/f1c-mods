# Player damage commentary protected-v5 review

Status: local prototype only; do not publish.

## Review of the existing source

The existing `source.c` is suitable for a transparent development test but
not for a paid release:

- one plainly named function performs the complete machine check;
- one `if` statement gates the entire feature;
- all six patch addresses and values are stored in plaintext;
- `.palloset`, `PLWM`, `LICENSE`, `HWID`, and the sidecar filename make the
  mechanism immediately identifiable;
- the sidecar is unsigned and can be edited;
- compiled-code changes are not tied to the data needed by the feature.

## Protected-v5 design

`source_protected_v5.c` removes the separate activation gate. A personalized
80-byte record is used together with the current machine code and a hash of
the DLL's executable-code section to reconstruct the six patch records.

Consequences:

- wrong machine: the patch records do not authenticate and zero writes occur;
- changed protected record: zero writes occur;
- changed compiled code on disk: the reconstruction key changes and zero
  writes occur;
- correct personalized copy: the same six verified writes occur;
- all failures affect only this optional mod; the game and its files remain
  untouched.

There is no custom PE section, readable watermark marker, personal name,
e-mail address, or runtime network request. The 16-byte delivery ID is opaque
and must be mapped to a customer only in the private issuing database.

## Honest security boundary

This raises the cost of casual copying and simple branch patching. It does not
make a native client binary impossible to reverse engineer. A determined
attacker can execute a valid copy under a debugger, recover the six writes,
and rebuild the feature. Authenticode signing must be performed after
personalization, and the loader should verify the expected final DLL hash.

## Safe release order

1. Compile and strip the template DLL.
2. Obtain the customer's eight-character machine code.
3. Run `Issue-ProtectedCopy.ps1` to create a new personalized output.
4. Store the returned opaque delivery ID against the customer server-side.
5. Authenticode-sign that final personalized DLL.
6. Record its SHA-256 in the customer's loader manifest.
7. Test correct-machine, wrong-machine, altered-data, and altered-code cases.

Never overwrite or publish the prototype or any test-personalized copy.

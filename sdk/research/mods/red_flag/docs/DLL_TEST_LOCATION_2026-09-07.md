# DLL testing-location check — 2026-09-07

Follow-up to LATEST_REVIEW_2026-09-07.md. v68 designated test artifact exists at trials/2026-09-01/f1-live-hud-compat-v68/redflag.v68.livehud-compat.dll (previous review verified SHA-256 8D57C1185F1895CB3E7F8CF356FC023AEE5531EAA8270EE003A95251A42BB32E).

Read-only recursive redflag*.dll search in Documents/F1 Challenge V10 and Documents/F1 Challenge V10_clean found only Documents/F1 Challenge V10/plugins/lightloader/redflag.dll, SHA-256 BB9586BB1A4D901300E0B06DBE4BBE48548BD5439C94A43E133A800884CC15D5. This differs from v68; its version was not established. No matching DLL found in those two installations. No installed files changed. v68 gameplay validation remains pending.

Wasted-effort check: no repeated large reads or broad builds; one PowerShell formatting/parser error corrected; no polling/waiting. This dated record is discoverable beside the latest review in docs. Existing workspace validator failures remain as recorded by that review.

# Compatibility SDK export

The large files in this directory are the historical 2026-07-17 SDK export.
They remain here so existing mods keep building, but their function entries are
raw addresses with mixed-trust names and no reviewed prototypes.

New work must use only `python tools/f1c_mod.py`; see the
[`tools/sdk` contract](../../tools/sdk/LLM_OPERATING_CONTRACT.md). The canonical
generator imports this export as explicitly tagged `LEGACY` leads, then overlays
reviewed fields and ABIs. Do not add new offsets here by hand.

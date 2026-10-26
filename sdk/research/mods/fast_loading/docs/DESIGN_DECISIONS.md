# fast_loading design decisions (owner Matias)

| # | Date | Decision |
|---|---|---|
| FL-1 | 2026-10-20 | Goal: use the maximum performance of modern systems (multicore) for loading. |
| FL-2 | 2026-10-20 | Ship the mod with the proven speed-ups; texture caches are an optional setting in the menu. |
| FL-3 | 2026-10-20 | Texture caches (TexCache, MipCache, FileCache) OFF by default, 1 GB cap; when full the cache is cleared. |
| FL-4 | 2026-10-20 | Lightloader update shows a "what's new" notice after an update. |
| FL-5 | 2026-10-20 | (superseded same day) Career hidden behind a switch. |
| FL-7 | 2026-10-20 | (withdrawn same day, misread) Career ships as it is. |
| FL-8 | 2026-10-20 | FINAL: Career is disabled in the shipped build and is not a usable tab (owner, caps). Owner-only test switch LIGHTLOADER_CAREER=1. |
| FL-6 | 2026-10-20 | The packaging/Lightloader work is done by Haiku agents; Opus reviews. |

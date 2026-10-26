/* 1.0.3 settings layout (owner 2026-10-17: plain-language settings page).
 *
 * Lightloader keeps a player's existing multiplayer_overhaul.ini on update, so the
 * DLL upgrades an older file itself: the packaged template (coop_mode.ini, embedded
 * as RCDATA COOP_INI_TEMPLATE) is written to "<ini>.new", the player's values are
 * copied in, the old file is kept as "<ini>.before-1.0.3", then "<ini>.new" replaces
 * the INI. Nothing is deleted. Any failure leaves the old file in place; the config
 * reader falls back to the old key names, so behaviour does not change.
 *
 * Key map (renamed keys are read under both names):
 *   [Prototype] MinHumans           -> PlayersBeforeBots
 *   [Prototype] ExpectedHumans      -> ExpectedPlayers
 *   [CoOp]      HostTimeWarpScale   -> HostTimeWarpSpeed
 *   [CoOp]      QualiAllTimed       -> EveryBotSetsQualiTime
 *   [CoOp]      SlowLaps            -> BotSlowOutAndInLaps
 *   [CoOp]      SlowLapsThrottle    -> SlowLapThrottle
 *   [CoOp]      RefillBotMidSession -> BotTakesOverAtOnce
 *   [CoOp]      PitSync             -> SharePitStopFailures
 *   [CoOp]      MfSync              -> ShareMechanicalFailures
 *   [Log]       Debug               -> DetailedLog
 *   [Log]       MaxMB               -> LogSizeLimitMB
 * Hidden keys (not in the template; still read; copied only when the player's value
 * differs from the default): AIToAdd, BotSource, RaceGridRewrite, PitExitFix,
 * RemoteCarTracking, FastestLapFix, BlueFlags, BotStream*, NativeBotHz.
 * Dropped keys (no effect since 1.0.1/1.0.2): [Net] Mode, SendHz, PlainMP, RequireMod,
 * RequireModCoop, KickMessage. Sections outside the template ([Bots], [HumanCars], dev
 * sections) are copied whole. */
#pragma once

#include <windows.h>
#include <string.h>
#include <stdio.h>

#define COOP_INI_LAYOUT_MARKER "Layout 2"
#define COOP_INI_BACKUP_SUFFIX ".before-1.0.3"

struct CoopIniKey { const char *section, *key, *old_key; };
struct CoopIniHidden { const char *section, *key, *def; };

static const CoopIniKey kCoopIniKeys[] = {
    {"Mod", "Enabled", NULL},
    {"Prototype", "PlayersBeforeBots", "MinHumans"},
    {"Prototype", "ExpectedPlayers", "ExpectedHumans"},
    {"CoOp", "HostAddress", NULL},
    {"CoOp", "ManifestPort", NULL},
    {"CoOp", "HostTimeWarp", NULL},
    {"CoOp", "HostTimeWarpSpeed", "HostTimeWarpScale"},
    {"CoOp", "EveryBotSetsQualiTime", "QualiAllTimed"},
    {"CoOp", "BotSlowOutAndInLaps", "SlowLaps"},
    {"CoOp", "SlowLapThrottle", "SlowLapsThrottle"},
    {"CoOp", "BotTakesOverAtOnce", "RefillBotMidSession"},
    {"CoOp", "SharePitStopFailures", "PitSync"},
    {"CoOp", "ShareMechanicalFailures", "MfSync"},
    {"Log", "DetailedLog", "Debug"},
    {"Log", "LogSizeLimitMB", "MaxMB"},
    {"Menu", "LastSelected", NULL},
};

/* Defaults equal the DLL defaults, so a dropped hidden key changes nothing. */
static const CoopIniHidden kCoopIniHidden[] = {
    {"Prototype", "AIToAdd", "22"},
    {"CoOp", "BotSource", "season"},
    {"CoOp", "RaceGridRewrite", "0"},
    {"CoOp", "PitExitFix", "1"},
    {"CoOp", "RemoteCarTracking", "1"},
    {"CoOp", "FastestLapFix", "1"},
    {"CoOp", "BlueFlags", "0"},
    {"Net", "BotStream", "0"},
    {"Net", "BotStreamPort", "34501"},
    {"Net", "BotStreamHz", "30"},
    {"Net", "NativeBotHz", "30"},
    {"Net", "BotStreamHoldMs", "500"},
    {"Net", "BotStreamBlendMs", "400"},
};

/* Sections the template owns; every other section of the old file is copied whole. */
static const char *const kCoopIniOwnSections[] = {"Mod", "Prototype", "CoOp", "Net", "Log", "Menu"};

enum CoopIniUpgrade {
    COOP_INI_NO_FILE = 0,   /* no INI: defaults, nothing written */
    COOP_INI_CURRENT,       /* already layout 2 */
    COOP_INI_UPGRADED,
    COOP_INI_FAILED         /* old file left in place; *error = GetLastError or step code */
};

/* Marker sentinel: a value no INI holds. */
#define COOP_INI_ABSENT "\x01" "absent"

static int coop_ini_read(const char *path, const char *section, const char *key, char *out, DWORD size)
{
    GetPrivateProfileStringA(section, key, COOP_INI_ABSENT, out, size, path);
    return strcmp(out, COOP_INI_ABSENT) != 0;
}

/* Value of a renamed key: the new name first, then the old one. 0 = absent. */
static int coop_ini_read_either(const char *path, const char *section, const char *key,
                                const char *old_key, char *out, DWORD size)
{
    if (coop_ini_read(path, section, key, out, size)) return 1;
    return old_key && coop_ini_read(path, section, old_key, out, size);
}

static int coop_ini_has_marker(const char *path)
{
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return 0;
    char head[512];
    DWORD got = 0;
    BOOL ok = ReadFile(file, head, sizeof(head) - 1, &got, NULL);
    CloseHandle(file);
    if (!ok) return 0;
    head[got] = 0;
    char *line_end = strpbrk(head, "\r\n");   /* marker only counts on the first line */
    if (line_end) *line_end = 0;
    return strstr(head, COOP_INI_LAYOUT_MARKER) != NULL;
}

static int coop_ini_own_section(const char *name)
{
    for (const char *own : kCoopIniOwnSections)
        if (lstrcmpiA(name, own) == 0) return 1;
    return 0;
}

static int coop_ini_write_file(const char *path, const void *data, DWORD size)
{
    HANDLE file = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return 0;
    DWORD wrote = 0;
    BOOL ok = WriteFile(file, data, size, &wrote, NULL) && wrote == size;
    CloseHandle(file);
    return ok;
}

/* Upgrade `path` to the template layout. `tmpl` is the template file content. */
static CoopIniUpgrade coop_ini_upgrade(const char *path, const void *tmpl, DWORD tmpl_size,
                                       unsigned long *error)
{
    char temp[MAX_PATH], backup[MAX_PATH], value[1024];
    *error = 0;
    if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) return COOP_INI_NO_FILE;
    if (coop_ini_has_marker(path)) return COOP_INI_CURRENT;
    if (!tmpl || !tmpl_size) { *error = 1; return COOP_INI_FAILED; }
    _snprintf(temp, sizeof(temp), "%s.new", path);
    temp[sizeof(temp) - 1] = 0;
    _snprintf(backup, sizeof(backup), "%s" COOP_INI_BACKUP_SUFFIX, path);
    backup[sizeof(backup) - 1] = 0;
    if (!coop_ini_write_file(temp, tmpl, tmpl_size)) { *error = GetLastError(); return COOP_INI_FAILED; }
    int ok = 1;
    for (const CoopIniKey &k : kCoopIniKeys)
        if (ok && coop_ini_read_either(path, k.section, k.key, k.old_key, value, sizeof(value)))
            ok = WritePrivateProfileStringA(k.section, k.key, value, temp) != 0;
    for (const CoopIniHidden &h : kCoopIniHidden)
        if (ok && coop_ini_read(path, h.section, h.key, value, sizeof(value)) && lstrcmpiA(value, h.def) != 0)
            ok = WritePrivateProfileStringA(h.section, h.key, value, temp) != 0;
    static char names[8192], body[32768];
    DWORD names_len = GetPrivateProfileSectionNamesA(names, sizeof(names), path);
    for (const char *name = names; ok && name < names + names_len && *name; name += strlen(name) + 1) {
        if (coop_ini_own_section(name)) continue;
        DWORD body_len = GetPrivateProfileSectionA(name, body, sizeof(body), path);
        if (body_len >= sizeof(body) - 2) { SetLastError(ERROR_MORE_DATA); ok = 0; break; }
        ok = WritePrivateProfileSectionA(name, body, temp) != 0;
    }
    if (!ok) { *error = GetLastError(); DeleteFileA(temp); return COOP_INI_FAILED; }
    WritePrivateProfileStringA(NULL, NULL, NULL, temp);   /* flush the profile cache */
    if (!CopyFileA(path, backup, TRUE) && GetLastError() != ERROR_FILE_EXISTS) {
        *error = GetLastError(); DeleteFileA(temp); return COOP_INI_FAILED;
    }
    if (!MoveFileExA(temp, path, MOVEFILE_REPLACE_EXISTING)) {
        *error = GetLastError(); DeleteFileA(temp); return COOP_INI_FAILED;
    }
    return COOP_INI_UPGRADED;
}

/* Config read with fallback to the pre-1.0.3 key name (used if the upgrade failed). */
static int coop_ini_int(const char *path, const char *section, const char *key, const char *old_key, int def)
{
    char value[64];
    if (coop_ini_read(path, section, key, value, sizeof(value)))
        return (int)GetPrivateProfileIntA(section, key, def, path);
    return old_key ? (int)GetPrivateProfileIntA(section, old_key, def, path) : def;
}

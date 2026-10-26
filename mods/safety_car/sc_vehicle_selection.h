#ifndef SC_VEHICLE_SELECTION_H
#define SC_VEHICLE_SELECTION_H
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* The existing AIDriverCountSetup argument is the native opponent filter.
   A year must be a complete positive token, not a substring of a car name. */
static int sc_filter_year(const char *filter)
{
    char token[512];
    size_t i = 0;
    int year = 0, negative = 0;
    if (!filter) return 0;
    while (i < 512 && filter[i]) {
        size_t n = 0;
        int candidate;
        while (i < 512 && filter[i] &&
               (isspace((unsigned char)filter[i]) || strchr(",:\"()", filter[i]))) ++i;
        if (i == 512) return 0;
        while (i < 512 && filter[i] &&
               !isspace((unsigned char)filter[i]) && !strchr(",:\"()", filter[i]))
            token[n++] = filter[i++];
        if (i == 512) return 0;
        token[n] = 0;
        if (!n) break;
        if (!_stricmp(token, "NOT")) { negative = 1; continue; }
        if (!_stricmp(token, "AND") || !_stricmp(token, "OR")) { negative = 0; continue; }
        if (negative || n != 4 || !isdigit((unsigned char)token[0]) ||
            !isdigit((unsigned char)token[1]) || !isdigit((unsigned char)token[2]) ||
            !isdigit((unsigned char)token[3])) continue;
        candidate = (token[0]-'0')*1000 + (token[1]-'0')*100 +
                    (token[2]-'0')*10 + token[3]-'0';
        if (candidate < 1000) continue;
        if (year && year != candidate) return 0;
        year = candidate;
    }
    return year;
}

static int sc_classes_match(char *value, int year)
{
    char year_text[8], *p = value;
    int safety = 0, notai = 0, era = 0;
    snprintf(year_text, sizeof(year_text), "%d", year);
    while (isspace((unsigned char)*p)) ++p;
    if (*p == '"') {
        char *end = strchr(++p, '"');
        if (!end) return 0;
        *end = 0;
    } else {
        char *comment = strstr(p, "//");
        if (comment) *comment = 0;
    }
    while (*p) {
        char *end = strchr(p, ','), *trim;
        if (end) *end = 0;
        while (isspace((unsigned char)*p)) ++p;
        trim = p + strlen(p);
        while (trim > p && isspace((unsigned char)trim[-1])) --trim;
        *trim = 0;
        if (!_stricmp(p, "Safety")) safety = 1;
        if (!_stricmp(p, "NotAI")) notai = 1;
        if (!strcmp(p, year_text)) era = 1;
        if (!end) break;
        p = end + 1;
    }
    return year > 0 && safety && notai && era;
}

/* -1 means the scan cannot establish uniqueness; 0 is a noncandidate. */
static int sc_vehicle_file_matches(const char *path, int year)
{
    FILE *f = fopen(path, "r");
    char line[4096];
    int result = 0;
    if (!f) return -1;
    while (fgets(line, sizeof(line), f)) {
        char *p = line, *end;
        if (!strchr(line, '\n') && !feof(f)) { result = -1; break; }
        while (isspace((unsigned char)*p)) ++p;
        if (_strnicmp(p, "Classes", 7)) continue;
        p += 7;
        while (isspace((unsigned char)*p)) ++p;
        if (*p != '=') continue;
        end = p + 1;
        result = sc_classes_match(end, year);
        break;
    }
    if (ferror(f)) result = -1;
    fclose(f);
    return result;
}

static int sc_scan_vehicles(const char *dir, int year, char *out, size_t cap,
                            unsigned *matches, unsigned depth)
{
    WIN32_FIND_DATAA fd;
    HANDLE h;
    char path[MAX_PATH];
    int n, ok = 1;
    DWORD error;
    if (depth > 32) return 0;
    n = snprintf(path, sizeof(path), "%s\\*", dir);
    if (n < 0 || (size_t)n >= sizeof(path)) return 0;
    h = FindFirstFileA(path, &fd);
    if (h == INVALID_HANDLE_VALUE) return GetLastError() == ERROR_FILE_NOT_FOUND;
    do {
        size_t len;
        int match;
        if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, "..")) continue;
        n = snprintf(path, sizeof(path), "%s\\%s", dir, fd.cFileName);
        if (n < 0 || (size_t)n >= sizeof(path)) { ok = 0; break; }
        /* Do not follow junctions out of the vehicle tree or create cycles. */
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (!sc_scan_vehicles(path, year, out, cap, matches, depth+1)) { ok = 0; break; }
            continue;
        }
        len = strlen(fd.cFileName);
        if (len < 4 || _stricmp(fd.cFileName+len-4, ".veh")) continue;
        match = sc_vehicle_file_matches(path, year);
        if (match < 0) { logf_("[sc-select] cannot read vehicle completely: %s", path); ok = 0; break; }
        if (!match) continue;
        ++*matches;
        logf_("[sc-select] year %d candidate: %s", year, path);
        if (*matches == 1) {
            if (strlen(path) >= cap) { ok = 0; break; }
            memcpy(out, path, strlen(path)+1);
        }
    } while (FindNextFileA(h, &fd));
    error = GetLastError();
    FindClose(h);
    return ok && error == ERROR_NO_MORE_FILES;
}

static int sc_select_vehicle(const char *filter, const char *root,
                              char *out, size_t cap, int *year_out)
{
    unsigned matches = 0;
    int year = sc_filter_year(filter);
    if (!out || !cap || !year_out) return 0;
    out[0] = 0; *year_out = year;
    if (!year) { logf_("[sc-select] no unique positive year in active filter; add disabled"); return 0; }
    if (!sc_scan_vehicles(root, year, out, cap, &matches, 0)) {
        out[0] = 0;
        logf_("[sc-select] incomplete vehicle scan for year %d; add disabled", year);
        return 0;
    }
    if (matches != 1) {
        out[0] = 0;
        logf_("[sc-select] year %d has %u matching vehicles; exactly one required; add disabled", year, matches);
        return 0;
    }
    logf_("[sc-select] selected year %d vehicle %s", year, out);
    return 1;
}
#endif

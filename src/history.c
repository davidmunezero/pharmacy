/*
 * history.c - transaction history recording, filtering and display.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "history.h"
#include "medicine.h"   /* now_string */

HistoryEntry history_entries[MAX_HISTORY];
int           history_count = 0;

const char *current_user(void)
{
    const char *u = getenv("USERNAME");
    if (!u || !*u) u = getenv("USER");
    return (u && *u) ? u : "user";
}

void log_history(const char *action, const char *detail)
{
    char ts[32];
    FILE *f;

    if (history_count < MAX_HISTORY) {
        HistoryEntry *h = &history_entries[history_count++];
        now_string(h->timestamp, sizeof h->timestamp);
        snprintf(h->user, sizeof h->user, "%s", current_user());
        snprintf(h->action, sizeof h->action, "%s", action);
        snprintf(h->detail, sizeof h->detail, "%s", detail);
    }

    now_string(ts, sizeof ts);
    f = fopen("data/history.dat", "a");
    if (f) {
        fprintf(f, "%s|%s|%s|%s\n", ts, current_user(), action, detail);
        fclose(f);
    }
}

static int entry_matches(const HistoryEntry *h,
                         const char *med_id_filter,
                         const char *date_filter)
{
    if (med_id_filter && *med_id_filter &&
        !strstr(h->detail, med_id_filter) &&
        !strstr(h->action, med_id_filter))
        return 0;
    if (date_filter && *date_filter &&
        strncmp(h->timestamp, date_filter, strlen(date_filter)) != 0)
        return 0;
    return 1;
}

int history_filter(const char *med_id_filter, const char *date_filter,
                   const HistoryEntry **out, int max)
{
    int i, n = 0;
    for (i = 0; i < history_count && n < max; i++) {
        if (entry_matches(&history_entries[i], med_id_filter, date_filter))
            out[n++] = &history_entries[i];
    }
    return n;
}

char *history_filtered_text(const char *med_id_filter,
                            const char *date_filter)
{
    int i, len = 0, cap = 8192;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    buf[0] = 0;

    for (i = 0; i < history_count; i++) {
        HistoryEntry *h = &history_entries[i];
        if (!entry_matches(h, med_id_filter, date_filter))
            continue;
        len += snprintf(buf + len, cap - len, "%s  %-12s %-14s %s\n",
                        h->timestamp, h->user, h->action, h->detail);
        if (len > cap - 600) break;
    }
    if (len == 0)
        snprintf(buf, cap, "(no matching records)");
    return buf;
}

#ifndef HISTORY_H
#define HISTORY_H

#define MAX_HISTORY 5000

typedef struct {
    char timestamp[32];
    char user[64];
    char action[24];
    char detail[512];
} HistoryEntry;

/* transaction history (defined in src/history.c) */
extern HistoryEntry history_entries[MAX_HISTORY];
extern int           history_count;

const char *current_user(void);
void log_history(const char *action, const char *detail);

/* returns a malloc'd string with the filtered history
 * (filter by medicine id substring and/or date prefix).
 * caller must free() the result. */
char *history_filtered_text(const char *med_id_filter,
                            const char *date_filter);

/* fills 'out' with pointers to matching entries (newest last,
 * same order as history_entries). returns the match count. */
int history_filter(const char *med_id_filter, const char *date_filter,
                   const HistoryEntry **out, int max);

#endif /* HISTORY_H */

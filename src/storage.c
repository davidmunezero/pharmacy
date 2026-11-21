/*
 * storage.c - persistent storage for medicines, suppliers, sales and
 * history. All files live in the data/ directory as *.dat text files,
 * one record per line, fields separated by '|'.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define MKDIR(p) mkdir(p, 0755)
#endif

#include "medicine.h"
#include "supplier.h"
#include "sales.h"
#include "history.h"
#include "storage.h"

/* ------------------------------------------------------------------ */
/* helpers                                                             */
/* ------------------------------------------------------------------ */

static void sanitize(char *s)
{
    for (; *s; s++)
        if (*s == '|' || *s == '\n' || *s == '\r') *s = ' ';
}

static void ensure_data_dir(void)
{
    MKDIR("data");   /* ignore errors: directory may already exist */
}

/* ------------------------------------------------------------------ */
/* medicines.dat                                                       */
/* ------------------------------------------------------------------ */

static void save_medicines(void)
{
    int i;
    FILE *f = fopen("data/medicines.dat", "w");
    if (!f) return;
    for (i = 0; i < medicine_count; i++) {
        Medicine *m = &medicines[i];
        char mfg[16], exp[16];
        fmt_date(m->mfg, mfg, sizeof mfg);
        fmt_date(m->expiry, exp, sizeof exp);
        fprintf(f, "%d|%s|%s|%.2f|%d|%s|%d|%s|%s|%d\n",
                m->id, m->name, m->category, m->price, m->qty,
                m->batch, m->prescriptionOnly, mfg, exp, m->supplier_id);
    }
    fclose(f);
}

static void load_medicines(void)
{
    char line[1024];
    FILE *f = fopen("data/medicines.dat", "r");
    if (!f) return;
    while (fgets(line, sizeof line, f) && medicine_count < MAX_MEDICINES) {
        Medicine *m = &medicines[medicine_count];
        char idbuf[32] = "", supbuf[32] = "", mfg[16] = "", exp[16] = "", price[32];
        int fields = sscanf(line, "%31[^|]|%127[^|]|%15[^|]|%31[^|]|%d|%31[^|]|%d|%15[^|]|%15[^|]|%31[^\n]",
                            idbuf, m->name, m->category, price, &m->qty,
                            m->batch, &m->prescriptionOnly, mfg, exp, supbuf);
        if (fields < 9) continue;
        m->id = atoi(idbuf);
        m->supplier_id = atoi(supbuf);
        m->price = atof(price);
        parse_date(mfg, &m->mfg);
        parse_date(exp, &m->expiry);
        sanitize(m->name); sanitize(m->category);
        sanitize(m->batch);
        medicine_count++;
    }
    fclose(f);
}

/* ------------------------------------------------------------------ */
/* suppliers.dat                                                       */
/* ------------------------------------------------------------------ */

static void save_suppliers(void)
{
    int i;
    FILE *f = fopen("data/suppliers.dat", "w");
    if (!f) return;
    for (i = 0; i < supplier_count; i++)
        fprintf(f, "%d|%s|%s|%s\n", suppliers[i].id, suppliers[i].name,
                suppliers[i].phone, suppliers[i].supplies);
    fclose(f);
}

static void load_suppliers(void)
{
    char line[1024];
    FILE *f = fopen("data/suppliers.dat", "r");
    if (!f) return;
    while (fgets(line, sizeof line, f) && supplier_count < MAX_SUPPLIERS) {
        Supplier *s = &suppliers[supplier_count];
        char idbuf[32] = "";
        if (sscanf(line, "%31[^|]|%127[^|]|%31[^|]|%255[^\n]",
                   idbuf, s->name, s->phone, s->supplies) < 3) continue;
        s->id = atoi(idbuf);
        sanitize(s->name); sanitize(s->phone);
        sanitize(s->supplies);
        supplier_count++;
    }
    fclose(f);
}

/* ------------------------------------------------------------------ */
/* sales.dat                                                           */
/* ------------------------------------------------------------------ */

static void save_sales(void)
{
    int i;
    FILE *f = fopen("data/sales.dat", "w");
    if (!f) return;
    for (i = 0; i < sale_count; i++) {
        Sale *s = &sales[i];
        fprintf(f, "%d|%d|%.2f|%.2f|%s|%d|%s|%.1f|%s\n",
                s->med_id, s->qty, s->unit_price, s->total,
                s->customer, s->prescriptionNum, s->timestamp,
                s->discount_pct, s->discount_reason);
    }
    fclose(f);
}

static void load_sales(void)
{
    char line[1024];
    FILE *f = fopen("data/sales.dat", "r");
    if (!f) return;
    while (fgets(line, sizeof line, f) && sale_count < MAX_SALES) {
        Sale *s = &sales[sale_count];
        char idbuf[32] = "", rxbuf[32] = "";
        if (sscanf(line, "%31[^|]|%d|%lf|%lf|%63[^|]|%31[^|]|%31[^|]|%lf|%63[^\n]",
                   idbuf, &s->qty, &s->unit_price, &s->total,
                   s->customer, rxbuf, s->timestamp,
                   &s->discount_pct, s->discount_reason) < 8) continue;
        s->med_id = atoi(idbuf);
        s->prescriptionNum = atoi(rxbuf);
        sanitize(s->customer); sanitize(s->discount_reason);
        sanitize(s->discount_reason);
        sale_count++;
    }
    fclose(f);
}

/* ------------------------------------------------------------------ */
/* history.dat                                                         */
/* ------------------------------------------------------------------ */

static void load_history(void)
{
    char line[2048];
    FILE *f = fopen("data/history.dat", "r");
    if (!f) return;
    while (fgets(line, sizeof line, f) && history_count < MAX_HISTORY) {
        HistoryEntry *h = &history_entries[history_count];
        if (sscanf(line, "%31[^|]|%63[^|]|%23[^|]|%511[^\n]",
                   h->timestamp, h->user, h->action, h->detail) < 3) continue;
        history_count++;
    }
    fclose(f);
}

/* ------------------------------------------------------------------ */
/* public API                                                          */
/* ------------------------------------------------------------------ */

void storage_load_all(void)
{
    ensure_data_dir();
    load_medicines();
    load_suppliers();
    load_sales();
    load_history();
}

void storage_save_all(void)
{
    ensure_data_dir();
    save_medicines();
    save_suppliers();
    save_sales();
    /* history is appended incrementally by log_history() */
}

void storage_reload_all(void)
{
    /* loaders append to the arrays, so reset the counts first */
    medicine_count  = 0;
    supplier_count  = 0;
    sale_count      = 0;
    history_count   = 0;
    storage_load_all();
}

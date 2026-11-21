/*
 * gui.c - IUP interface.
 *
 * Main window = IupTabs with 5 pages:
 *   Home       - summary report + IupPlot bar chart (stock expiring
 *                per month, next 12 months) + export
 *   Medicines  - data-entry forms on the left, medicine table on right
 *   Suppliers  - supplier form on the left, supplier table on right
 *   Sales      - sell/discount forms on the left, sales table on right
 *   History    - filters + scrollable transaction log
 *
 * No modal dialogs are used for data entry; every form lives inside
 * its tab as a 2-column IupGridBox.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <iup.h>
#include <iupcontrols.h>
#include <iup_plot.h>

#include "medicine.h"
#include "supplier.h"
#include "sales.h"
#include "history.h"
#include "storage.h"
#include "gui.h"

/* Exported by libiupcontrols but not declared in the public header. */
extern void iupMatrixSetValue(Ihandle *ih, int lin, int col,
                              const char *value, int user_edited);

/* ------------------------------------------------------------------ */
/* UI state                                                            */
/* ------------------------------------------------------------------ */

typedef struct {
    Ihandle *main_dlg;
    Ihandle *tabs;

    /* Medicines tab */
    Ihandle *matrix;
    Ihandle *status;
    /* add-medicine form */
    Ihandle *add_id, *add_name, *add_cat, *add_price, *add_qty;
    Ihandle *add_batch, *add_mfg, *add_exp, *add_prescriptionOnly, *add_msg;
    /* update-stock form */
    Ihandle *upd_id, *upd_qty, *upd_batch, *upd_exp, *upd_msg;
    /* check form */
    Ihandle *chk_id;
    /* stock value: result shown in a popup dialog */

    /* Suppliers tab */
    Ihandle *sup_matrix;
    Ihandle *sup_id, *sup_name, *sup_phone, *sup_meds, *sup_msg;

    /* Sales tab */
    Ihandle *sales_matrix;
    Ihandle *sell_id, *sell_qty, *sell_cust, *sell_prescriptionNum;
    Ihandle *disc_sales, *disc_pct, *disc_reason;

    /* History tab */
    Ihandle *hist_fid, *hist_fdate, *hist_fdate_on, *hist_matrix;

    /* Home tab */
    Ihandle *rep_text;
    Ihandle *plot;
} UI;

static UI ui;
static int sel_med = -1;
static int sel_sup = -1;

static Ihandle *page_home, *page_meds, *page_sup, *page_sales, *page_hist;

/* ------------------------------------------------------------------ */
/* Small GUI helpers                                                   */
/* ------------------------------------------------------------------ */

static void trim(char *s)
{
    char *p = s;
    size_t n;
    while (*p == ' ' || *p == '\t') p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\n'))
        s[--n] = 0;
}

static void sanitize(char *s)
{
    for (; *s; s++)
        if (*s == '|' || *s == '\n' || *s == '\r') *s = ' ';
}

static const char *ent_val(Ihandle *e)
{
    const char *v = IupGetAttribute(e, "VALUE");
    return v ? v : "";
}

/* IupList VALUE returns the selected index; VALUESTRING returns the text */
static const char *list_sel_text(Ihandle *l)
{
    const char *v = IupGetAttribute(l, "VALUESTRING");
    return v ? v : "";
}

static void ent_clear(Ihandle *e) { IupSetAttribute(e, "VALUE", ""); }

static int parse_int_field(const char *s, int *out)
{
    char *end;
    long v = strtol(s, &end, 10);
    if (end == s) return 0;
    *out = (int)v;
    return 1;
}

static int parse_double_field(const char *s, double *out)
{
    char *end;
    double v = strtod(s, &end);
    if (end == s) return 0;
    *out = v;
    return 1;
}

/* IupDatePick VALUE is always "year/month/day" (display order is
   controlled separately by ORDER/SEPARATOR). */
static int datepick_ymd(Ihandle *dp, int *y, int *m, int *d)
{
    return sscanf(ent_val(dp), "%d/%d/%d", y, m, d) == 3;
}

static Ihandle *date_pick(void)
{
    Ihandle *dp = IupDatePick();
    IupSetAttribute(dp, "ORDER", "YMD");
    IupSetAttribute(dp, "SEPARATOR", "-");
    IupSetAttribute(dp, "EXPAND", "HORIZONTAL");
    return dp;
}

static void list_clear(Ihandle *l) { IupSetAttribute(l, "REMOVEITEM", "ALL"); }
static void list_add(Ihandle *l, const char *text) { IupSetAttribute(l, "APPENDITEM", text); }

static void matrix_cell(Ihandle *m, int lin, int col, const char *text)
{
    iupMatrixSetValue(m, lin, col, text, 0);
}

static void matrix_color(Ihandle *m, int lin, int col, const char *bg, const char *fg)
{
    IupSetAttributeId2(m, "BGCOLOR", lin, col, bg);
    IupSetAttributeId2(m, "FGCOLOR", lin, col, fg);
}

static void show_error(Ihandle *parent, const char *msg)
{
    IupMessageAlarm(parent, "Error", msg, "OK");
}

static void show_info(const char *title, const char *msg)
{
    IupMessage(title, msg);
}

static Ihandle *entry(void)
{
    Ihandle *t = IupText(NULL);
    IupSetAttribute(t, "EXPAND", "HORIZONTAL");
    return t;
}


static Ihandle *msg_label(void)
{
    Ihandle *l = IupLabel("");
    IupSetAttribute(l, "EXPAND", "HORIZONTAL");
    return l;
}

/* ------------------------------------------------------------------ */
/* Refresh functions (call only after widgets are mapped)             */
/* ------------------------------------------------------------------ */

#define MCOLS 10   /* marker col + 9 data cols */

static int cmp_name(const void *a, const void *b)
{
    return strcmp((const char *)a, (const char *)b);
}

/* Populate the supplier "Supplies" multi-select list with the unique
   medicine names, sorted alphabetically. Called from refresh_matrix()
   so it stays in sync with stock changes. */
static void refresh_supplies_list(void)
{
    static char names[MAX_MEDICINES][128];
    int i, j, n = 0;

    if (!ui.sup_meds) return;
    list_clear(ui.sup_meds);
    for (i = 0; i < medicine_count; i++) {
        int dup = 0;
        for (j = 0; j < n; j++)
            if (strcmp(names[j], medicines[i].name) == 0) { dup = 1; break; }
        if (!dup)
            snprintf(names[n++], sizeof names[0], "%s", medicines[i].name);
    }
    qsort(names, n, sizeof names[0], cmp_name);
    for (i = 0; i < n; i++)
        list_add(ui.sup_meds, names[i]);
}

static void refresh_matrix(void)
{
    int i, c;
    char num[64];

    /* NUMCOL/NUMLIN auto-add 1 for the title row/column, so pass
       (cells - 1): we use cols 0-9 and lines 0..medicine_count. */
    IupSetfAttribute(ui.matrix, "NUMLIN", "%d", medicine_count);
    IupSetAttribute(ui.matrix, "NUMCOL", "9");

    matrix_cell(ui.matrix, 0, 0, "#");
    matrix_cell(ui.matrix, 0, 1, "ID");
    matrix_cell(ui.matrix, 0, 2, "Name");
    matrix_cell(ui.matrix, 0, 3, "Category");
    matrix_cell(ui.matrix, 0, 4, "Qty");
    matrix_cell(ui.matrix, 0, 5, "Unit Price");
    matrix_cell(ui.matrix, 0, 6, "Batch");
    matrix_cell(ui.matrix, 0, 7, "Expiry");
    matrix_cell(ui.matrix, 0, 8, "PrescriptionOnly");
    matrix_cell(ui.matrix, 0, 9, "Status");

    for (i = 0; i < medicine_count; i++) {
        Medicine *m = &medicines[i];
        int lin = i + 1;
        int exp_days = days_to_expiry(m);
        char idb[16], price[32], qty[16], presc[8], exp[16], status[48];
        const char *bg = NULL, *fg = NULL;

        snprintf(idb, sizeof idb, "%d", m->id);
        snprintf(price, sizeof price, "%.2f", m->price);
        snprintf(qty, sizeof qty, "%d", m->qty);
        strcpy(presc, m->prescriptionOnly ? "Yes" : "No");
        fmt_date(m->expiry, exp, sizeof exp);

        /* precedence: expired (grey) > low stock (red) > near expiry (orange) */
        if (med_is_expired(m)) {
            strcpy(status, "EXPIRED");
            bg = "#C8C8C8"; fg = "#404040";
        } else if (m->qty < LOW_STOCK) {
            strcpy(status, "LOW STOCK");
            bg = "#FFB3B3"; fg = "#000000";
        } else if (exp_days <= EXPIRY_WARN) {
            snprintf(status, sizeof status, "Expires in %dd", exp_days);
            bg = "#FFD9A0"; fg = "#000000";
        } else {
            strcpy(status, "OK");
        }

        matrix_cell(ui.matrix, lin, 0, "");
        matrix_cell(ui.matrix, lin, 1, idb);
        matrix_cell(ui.matrix, lin, 2, m->name);
        matrix_cell(ui.matrix, lin, 3, m->category);
        matrix_cell(ui.matrix, lin, 4, qty);
        matrix_cell(ui.matrix, lin, 5, price);
        matrix_cell(ui.matrix, lin, 6, m->batch);
        matrix_cell(ui.matrix, lin, 7, exp);
        matrix_cell(ui.matrix, lin, 8, presc);
        matrix_cell(ui.matrix, lin, 9, status);

        for (c = 1; c <= 9; c++)
            matrix_color(ui.matrix, lin, c, bg, fg);
    }

    snprintf(num, sizeof num, "%d medicine(s) in stock", medicine_count);
    IupSetAttribute(ui.status, "TITLE", num);
    IupSetAttribute(ui.matrix, "REDRAW", "ALL");

    refresh_supplies_list();
}

static int matrix_enteritem_cb(Ihandle *ih, int lin, int col)
{
    (void)ih; (void)col;
    if (lin >= 1 && lin <= medicine_count) sel_med = lin - 1;
    return IUP_DEFAULT;
}

static void refresh_sup_matrix(void)
{
    int i;

    IupSetfAttribute(ui.sup_matrix, "NUMLIN", "%d", supplier_count);
    IupSetAttribute(ui.sup_matrix, "NUMCOL", "4");

    matrix_cell(ui.sup_matrix, 0, 0, "#");
    matrix_cell(ui.sup_matrix, 0, 1, "Supplier ID");
    matrix_cell(ui.sup_matrix, 0, 2, "Name");
    matrix_cell(ui.sup_matrix, 0, 3, "Phone");
    matrix_cell(ui.sup_matrix, 0, 4, "Supplies");

    for (i = 0; i < supplier_count; i++) {
        Supplier *s = &suppliers[i];
        int lin = i + 1;
        char idb[16];
        snprintf(idb, sizeof idb, "%d", s->id);
        matrix_cell(ui.sup_matrix, lin, 0, "");
        matrix_cell(ui.sup_matrix, lin, 1, idb);
        matrix_cell(ui.sup_matrix, lin, 2, s->name);
        matrix_cell(ui.sup_matrix, lin, 3, s->phone);
        matrix_cell(ui.sup_matrix, lin, 4, s->supplies);
    }

    IupSetAttribute(ui.sup_matrix, "REDRAW", "ALL");
}

static int sup_matrix_enteritem_cb(Ihandle *ih, int lin, int col)
{
    (void)ih; (void)col;
    if (lin >= 1 && lin <= supplier_count) sel_sup = lin - 1;
    return IUP_DEFAULT;
}

static void refresh_sales_matrix(void)
{
    int i;

    IupSetfAttribute(ui.sales_matrix, "NUMLIN", "%d", sale_count);
    IupSetAttribute(ui.sales_matrix, "NUMCOL", "8");

    matrix_cell(ui.sales_matrix, 0, 0, "#");
    matrix_cell(ui.sales_matrix, 0, 1, "Date");
    matrix_cell(ui.sales_matrix, 0, 2, "Medicine");
    matrix_cell(ui.sales_matrix, 0, 3, "Qty");
    matrix_cell(ui.sales_matrix, 0, 4, "Unit Price");
    matrix_cell(ui.sales_matrix, 0, 5, "Total");
    matrix_cell(ui.sales_matrix, 0, 6, "Customer");
    matrix_cell(ui.sales_matrix, 0, 7, "PrescriptionNum");
    matrix_cell(ui.sales_matrix, 0, 8, "Discount");

    for (i = 0; i < sale_count; i++) {
        Sale *s = &sales[i];
        int lin = i + 1;
        char qty[16], price[32], total[32], disc[32], medid[16], rxnum[16];

        snprintf(medid, sizeof medid, "%d", s->med_id);
        if (s->prescriptionNum > 0)
            snprintf(rxnum, sizeof rxnum, "%d", s->prescriptionNum);
        else
            strcpy(rxnum, "-");
        snprintf(qty, sizeof qty, "%d", s->qty);
        snprintf(price, sizeof price, "%.2f", s->unit_price);
        snprintf(total, sizeof total, "%.2f", s->total);
        if (s->discount_pct > 0)
            snprintf(disc, sizeof disc, "%.0f%% %s", s->discount_pct,
                     s->discount_reason);
        else
            strcpy(disc, "-");

        matrix_cell(ui.sales_matrix, lin, 0, "");
        matrix_cell(ui.sales_matrix, lin, 1, s->timestamp);
        matrix_cell(ui.sales_matrix, lin, 2, medid);
        matrix_cell(ui.sales_matrix, lin, 3, qty);
        matrix_cell(ui.sales_matrix, lin, 4, price);
        matrix_cell(ui.sales_matrix, lin, 5, total);
        matrix_cell(ui.sales_matrix, lin, 6, s->customer);
        matrix_cell(ui.sales_matrix, lin, 7, rxnum);
        matrix_cell(ui.sales_matrix, lin, 8, disc);
    }

    IupSetAttribute(ui.sales_matrix, "REDRAW", "ALL");
}

static void refresh_sales_list(Ihandle *l)
{
    int i;
    char line[256];

    list_clear(l);
    for (i = 0; i < sale_count; i++) {
        Sale *s = &sales[i];
        /* keep items short: long text would widen the gridbox column and
           push the discount fields out of the visible pane */
        snprintf(line, sizeof line, "#%d | Med %d x%d @ %.2f = %.2f",
                 i + 1, s->med_id, s->qty, s->unit_price, s->total);
        list_add(l, line);
    }
    if (sale_count == 0)
        list_add(l, "(no sales recorded)");
    if (sale_count > 0) IupSetInt(l, "VALUE", 1);
}

static void refresh_history(void)
{
    char fid[64], fdate[16];
    const HistoryEntry *matches[MAX_HISTORY];
    int i, n;

    snprintf(fid, sizeof fid, "%s", ent_val(ui.hist_fid));   trim(fid);

    /* IupDatePick always holds a date, so the filter only applies
       when the "Apply date filter" toggle is on */
    fdate[0] = '\0';
    if (IupGetInt(ui.hist_fdate_on, "VALUE")) {
        int y, m, d;
        if (datepick_ymd(ui.hist_fdate, &y, &m, &d))
            snprintf(fdate, sizeof fdate, "%04d-%02d-%02d", y, m, d);
    }

    n = history_filter(*fid ? fid : NULL, *fdate ? fdate : NULL,
                       matches, MAX_HISTORY);

    /* lines 0..n -> NUMLIN n ; cols 0..4 -> NUMCOL 4 */
    IupSetfAttribute(ui.hist_matrix, "NUMLIN", "%d", n > 0 ? n : 1);
    IupSetAttribute(ui.hist_matrix, "NUMCOL", "4");

    matrix_cell(ui.hist_matrix, 0, 0, "#");
    matrix_cell(ui.hist_matrix, 0, 1, "Timestamp");
    matrix_cell(ui.hist_matrix, 0, 2, "User");
    matrix_cell(ui.hist_matrix, 0, 3, "Action");
    matrix_cell(ui.hist_matrix, 0, 4, "Detail");

    if (n == 0) {
        matrix_cell(ui.hist_matrix, 1, 0, "");
        matrix_cell(ui.hist_matrix, 1, 1, "(no matching records)");
    }
    for (i = 0; i < n; i++) {
        const HistoryEntry *h = matches[i];
        int lin = i + 1;
        matrix_cell(ui.hist_matrix, lin, 0, "");
        matrix_cell(ui.hist_matrix, lin, 1, h->timestamp);
        matrix_cell(ui.hist_matrix, lin, 2, h->user);
        matrix_cell(ui.hist_matrix, lin, 3, h->action);
        matrix_cell(ui.hist_matrix, lin, 4, h->detail);
    }
    IupSetAttribute(ui.hist_matrix, "REDRAW", "ALL");
}

static void generate_report(char *buf, size_t n)
{
    int i;
    int total_qty = 0;
    double stock_value, expired_loss;
    double sales_total = 0.0;
    int sales_today = 0;
    char best_id[32] = "", best_name[128] = "", best[256] = "";
    int best_qty = -1;
    int low_count = 0, expiring_count = 0;
    char today[16];
    size_t off = 0;
    Date t = today_date();
    fmt_date(t, today, sizeof today);

    stock_value = medicine_stock_value(0);
    expired_loss = medicine_stock_value(1);

    for (i = 0; i < medicine_count; i++) {
        Medicine *m = &medicines[i];
        total_qty += m->qty;
        if (m->qty < LOW_STOCK)
            low_count++;
        if (!med_is_expired(m) && days_to_expiry(m) <= EXPIRY_WARN)
            expiring_count++;
    }

    for (i = 0; i < sale_count; i++) {
        Sale *s = &sales[i];
        sales_total += s->total;
        if (strncmp(s->timestamp, today, 10) == 0) sales_today++;
    }

    for (i = 0; i < medicine_count; i++) {
        int k, qty_sold = 0;
        for (k = 0; k < sale_count; k++)
            if (sales[k].med_id == medicines[i].id)
                qty_sold += sales[k].qty;
        if (qty_sold > best_qty) {
            best_qty = qty_sold;
            snprintf(best_id, sizeof best_id, "%d", medicines[i].id);
            snprintf(best_name, sizeof best_name, "%s", medicines[i].name);
        }
    }
    if (best_qty > 0)
        snprintf(best, sizeof best, "%s %s", best_id, best_name);
    else {
        strcpy(best, "(no sales yet)");
        best_qty = 0;
    }

    off = 0;
    off += snprintf(buf + off, n - off, "=========== PHARMACY REPORT ===========\n");
    off += snprintf(buf + off, n - off, "Generated : %s\n", today);
    off += snprintf(buf + off, n - off, "User      : %s\n", current_user());
    off += snprintf(buf + off, n - off, "--------------------------------------\n");
    off += snprintf(buf + off, n - off, "Total medicines          : %d\n", medicine_count);
    off += snprintf(buf + off, n - off, "Total quantity in stock  : %d\n", total_qty);
    off += snprintf(buf + off, n - off, "Stock value              : %.2f\n", stock_value);
    off += snprintf(buf + off, n - off, "Expired stock (loss)     : %.2f\n", expired_loss);
    off += snprintf(buf + off, n - off, "--------------------------------------\n");
    off += snprintf(buf + off, n - off, "Total sales amount       : %.2f\n", sales_total);
    off += snprintf(buf + off, n - off, "Number of sales today    : %d\n", sales_today);
    off += snprintf(buf + off, n - off, "--------------------------------------\n");
    off += snprintf(buf + off, n - off, "Best-selling medicine    : %s (%d units)\n",
                    best, best_qty);
    off += snprintf(buf + off, n - off, "Below re-order (<10)     : %d\n",
                    low_count);
    off += snprintf(buf + off, n - off, "Expiring within 30 days  : %d\n",
                    expiring_count);
    off += snprintf(buf + off, n - off, "======================================\n");
}

static void refresh_report(void)
{
    char *buf = malloc(8192);
    if (buf) {
        generate_report(buf, 8192);
        IupSetAttribute(ui.rep_text, "VALUE", buf);
        free(buf);
    }
}

/* Bar chart: stock quantity expiring in each of the next 12 months. */
static void refresh_plot(void)
{
    static const char *mon_names[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    int buckets[12] = {0};
    int i, k;
    Date today = today_date();
    char label[16];

    for (i = 0; i < medicine_count; i++) {
        Medicine *m = &medicines[i];
        int months_ahead;
        if (med_is_expired(m)) continue;
        months_ahead = (m->expiry.y - today.y) * 12 + (m->expiry.m - today.m);
        if (months_ahead >= 0 && months_ahead < 12)
            buckets[months_ahead] += m->qty;
    }

    IupSetAttribute(ui.plot, "CLEAR", "ALL");

    IupPlotBegin(ui.plot, 1);   /* string X labels */
    for (k = 0; k < 12; k++) {
        int total = today.y * 12 + (today.m - 1) + k;
        int y = total / 12;
        int m = total % 12 + 1;
        snprintf(label, sizeof label, "%s %02d", mon_names[m - 1], y % 100);
        IupPlotAddStr(ui.plot, label, (double)buckets[k]);
    }
    IupPlotEnd(ui.plot);

    IupSetAttribute(ui.plot, "DS_MODE", "BAR");
    IupSetAttribute(ui.plot, "DS_NAME", "Units expiring");
    IupSetAttribute(ui.plot, "DS_COLOR", "255 90 0");
    IupSetAttribute(ui.plot, "TITLE",
                    "Stock expiring per month (next 12 months)");
}

/* ------------------------------------------------------------------ */
/* Tab-change callback: lazy refresh                                   */
/* ------------------------------------------------------------------ */

static int cb_tab_change(Ihandle *ih, Ihandle *child)
{
    (void)ih;
    if (child == page_sup)       refresh_sup_matrix();
    else if (child == page_sales) { refresh_sales_matrix(); refresh_sales_list(ui.disc_sales); }
    else if (child == page_hist)  refresh_history();
    else if (child == page_home)  { refresh_report(); refresh_plot(); }
    return IUP_DEFAULT;
}

/* ------------------------------------------------------------------ */
/* Medicines tab callbacks                                             */
/* ------------------------------------------------------------------ */

static int cb_add_save(Ihandle *self)
{
    char id[32], name[128], cat[16], batch[32];
    char price_s[32], qty_s[16];
    double price;
    int qty, med_id, y, mm, dd;
    Date mfg, exp, today;
    Medicine *m;
    char detail[512];

    (void)self;

    snprintf(id, sizeof id, "%s", ent_val(ui.add_id));       trim(id);
    snprintf(name, sizeof name, "%s", ent_val(ui.add_name)); trim(name);
    snprintf(cat, sizeof cat, "%s", list_sel_text(ui.add_cat)); trim(cat);
    snprintf(batch, sizeof batch, "%s", ent_val(ui.add_batch)); trim(batch);
    snprintf(price_s, sizeof price_s, "%s", ent_val(ui.add_price)); trim(price_s);
    snprintf(qty_s, sizeof qty_s, "%s", ent_val(ui.add_qty)); trim(qty_s);

    if (!*id)   { show_error(ui.main_dlg, "Medicine ID is required."); return IUP_DEFAULT; }
    if (!parse_int_field(id, &med_id) || med_id <= 0) {
        show_error(ui.main_dlg, "Medicine ID must be a positive integer.");
        return IUP_DEFAULT;
    }
    if (!*name) { show_error(ui.main_dlg, "Medicine name is required."); return IUP_DEFAULT; }
    if (!*cat)  { show_error(ui.main_dlg, "Please choose a category."); return IUP_DEFAULT; }
    if (!parse_double_field(price_s, &price) || price <= 0) {
        show_error(ui.main_dlg, "Unit price must be a number greater than 0.");
        return IUP_DEFAULT;
    }
    if (!parse_int_field(qty_s, &qty) || qty <= 0) {
        show_error(ui.main_dlg, "Quantity must be an integer greater than 0.");
        return IUP_DEFAULT;
    }
    if (!*batch) { show_error(ui.main_dlg, "Batch number is required."); return IUP_DEFAULT; }
    if (!datepick_ymd(ui.add_mfg, &y, &mm, &dd) || !date_valid_ymd(y, mm, dd)) {
        show_error(ui.main_dlg, "Invalid manufacture date.");
        return IUP_DEFAULT;
    }
    mfg.y = y; mfg.m = mm; mfg.d = dd;
    if (!datepick_ymd(ui.add_exp, &y, &mm, &dd) || !date_valid_ymd(y, mm, dd)) {
        show_error(ui.main_dlg, "Invalid expiry date.");
        return IUP_DEFAULT;
    }
    exp.y = y; exp.m = mm; exp.d = dd;
    today = today_date();
    if (date_days(&exp) <= date_days(&mfg)) {
        show_error(ui.main_dlg, "Expiry date must be later than the manufacture date.");
        return IUP_DEFAULT;
    }
    if (date_days(&exp) <= date_days(&today)) {
        show_error(ui.main_dlg, "Expiry date must be later than today.");
        return IUP_DEFAULT;
    }
    if (date_days(&mfg) > date_days(&today)) {
        show_error(ui.main_dlg, "Manufacture date cannot be in the future.");
        return IUP_DEFAULT;
    }
    if (find_medicine(med_id) >= 0) {
        show_error(ui.main_dlg, "Medicine ID already exists.");
        return IUP_DEFAULT;
    }
    if (medicine_count >= MAX_MEDICINES) {
        show_error(ui.main_dlg, "Medicine database is full.");
        return IUP_DEFAULT;
    }

    m = &medicines[medicine_count++];
    memset(m, 0, sizeof *m);
    m->id = med_id;
    snprintf(m->name, sizeof m->name, "%s", name);
    snprintf(m->category, sizeof m->category, "%s", cat);
    snprintf(m->batch, sizeof m->batch, "%s", batch);
    m->price = price;
    m->qty = qty;
    m->prescriptionOnly = (strcmp(ent_val(ui.add_prescriptionOnly), "ON") == 0) ? 1 : 0;
    m->mfg = mfg;
    m->expiry = exp;
    m->supplier_id = 0;

    storage_save_all();
    refresh_matrix();
    refresh_plot();
    snprintf(detail, sizeof detail, "Added medicine %s (%s), qty=%d, price=%.2f",
             id, name, qty, price);
    log_history("ADD", detail);

    snprintf(detail, sizeof detail, "Medicine '%s' (ID %s) saved to inventory.",
             name, id);
    show_info("Medicine saved", detail);

    IupSetAttribute(ui.add_msg, "TITLE", "Medicine saved to inventory.");
    ent_clear(ui.add_id); ent_clear(ui.add_name); ent_clear(ui.add_price);
    ent_clear(ui.add_qty); ent_clear(ui.add_batch);
    IupSetAttribute(ui.add_prescriptionOnly, "VALUE", "OFF");
    return IUP_DEFAULT;
}

static int cb_upd_save(Ihandle *self)
{
    char id[32], qty_s[16], batch[32];
    int add_qty, idx, med_id, y, mm, dd;
    Date exp, today;
    Medicine *m;
    char detail[512];

    (void)self;

    snprintf(id, sizeof id, "%s", ent_val(ui.upd_id));    trim(id);
    snprintf(qty_s, sizeof qty_s, "%s", ent_val(ui.upd_qty)); trim(qty_s);
    snprintf(batch, sizeof batch, "%s", ent_val(ui.upd_batch)); trim(batch);

    if (!*id) { show_error(ui.main_dlg, "Medicine ID is required."); return IUP_DEFAULT; }
    if (!parse_int_field(id, &med_id) || med_id <= 0) {
        show_error(ui.main_dlg, "Medicine ID must be a positive integer.");
        return IUP_DEFAULT;
    }
    idx = find_medicine(med_id);
    if (idx < 0) { show_error(ui.main_dlg, "Medicine not found."); return IUP_DEFAULT; }
    if (!parse_int_field(qty_s, &add_qty) || add_qty <= 0) {
        show_error(ui.main_dlg, "Quantity to add must be an integer greater than 0.");
        return IUP_DEFAULT;
    }
    if (!*batch) { show_error(ui.main_dlg, "New batch number is required."); return IUP_DEFAULT; }
    if (!datepick_ymd(ui.upd_exp, &y, &mm, &dd) || !date_valid_ymd(y, mm, dd)) {
        show_error(ui.main_dlg, "Invalid expiry date.");
        return IUP_DEFAULT;
    }
    exp.y = y; exp.m = mm; exp.d = dd;
    today = today_date();
    if (date_days(&exp) <= date_days(&today)) {
        show_error(ui.main_dlg, "New expiry date must not be in the past.");
        return IUP_DEFAULT;
    }

    m = &medicines[idx];
    m->qty += add_qty;
    snprintf(m->batch, sizeof m->batch, "%s", batch);
    m->expiry = exp;

    storage_save_all();
    refresh_matrix();
    refresh_plot();
    snprintf(detail, sizeof detail, "Updated stock of %s: +%d (batch %s)",
             id, add_qty, batch);
    log_history("UPDATE", detail);

    IupSetAttribute(ui.upd_msg, "TITLE", "Stock level updated.");
    ent_clear(ui.upd_id); ent_clear(ui.upd_qty);
    ent_clear(ui.upd_batch);
    return IUP_DEFAULT;
}

static int cb_chk_show(Ihandle *self)
{
    char id[32], msg[512], exp[16];
    int idx, med_id, days;
    Medicine *m;

    (void)self;

    snprintf(id, sizeof id, "%s", ent_val(ui.chk_id)); trim(id);
    if (!*id) { show_error(ui.main_dlg, "Medicine ID is required."); return IUP_DEFAULT; }
    if (!parse_int_field(id, &med_id) || med_id <= 0) {
        show_error(ui.main_dlg, "Medicine ID must be a positive integer.");
        return IUP_DEFAULT;
    }
    idx = find_medicine(med_id);
    if (idx < 0) { show_error(ui.main_dlg, "Medicine not found."); return IUP_DEFAULT; }
    m = &medicines[idx];
    days = days_to_expiry(m);
    fmt_date(m->expiry, exp, sizeof exp);

    snprintf(msg, sizeof msg,
             "Medicine : %s (%d)\n"
             "Stock    : %d unit(s)\n"
             "Expiry   : %s\n"
             "Batch    : %s",
             m->name, m->id, m->qty, exp, m->batch);
    show_info("Stock / Expiry", msg);

    if (med_is_expired(m))
        show_info("Expired stock",
                  "This medicine is EXPIRED and cannot be sold.");
    else if (days <= EXPIRY_WARN)
        show_info("Expiry warning",
                  "This medicine expires within 30 days.");
    if (m->qty < LOW_STOCK)
        show_info("Low stock warning",
                  "Stock level is below the re-order level (10 units).");

    return IUP_DEFAULT;
}

static int cb_val_show(Ihandle *self)
{
    double good, expired;
    char out[256];
    (void)self;

    good = medicine_stock_value(0);
    expired = medicine_stock_value(1);

    snprintf(out, sizeof out,
             "Total stock value (non-expired): %.2f\n"
             "Expired stock (loss)         : %.2f",
             good, expired);
    show_info("Stock Value", out);
    return IUP_DEFAULT;
}

static int cb_remove_selected(Ihandle *self)
{
    char msg[256], detail[512];
    int idx;
    (void)self;

    if (sel_med < 0 || sel_med >= medicine_count) {
        show_error(ui.main_dlg, "Please select a medicine row in the table first.");
        return IUP_DEFAULT;
    }
    idx = sel_med;
    snprintf(msg, sizeof msg, "Remove medicine '%s' (%d)?",
             medicines[idx].name, medicines[idx].id);
    if (IupMessageAlarm(ui.main_dlg, "Confirm removal", msg, "YESNO") != 1)
        return IUP_DEFAULT;

    snprintf(detail, sizeof detail, "Removed medicine %d (%s)",
             medicines[idx].id, medicines[idx].name);
    med_remove(idx);
    sel_med = -1;

    storage_save_all();
    refresh_matrix();
    refresh_plot();
    log_history("REMOVE", detail);
    return IUP_DEFAULT;
}

static int cb_remove_expired(Ihandle *self)
{
    int i, removed = 0;
    (void)self;

    for (i = medicine_count - 1; i >= 0; i--) {
        if (med_is_expired(&medicines[i])) {
            char detail[512];
            snprintf(detail, sizeof detail, "Removed expired medicine %d (%s)",
                     medicines[i].id, medicines[i].name);
            med_remove(i);
            removed++;
            log_history("REMOVE", detail);
        }
    }
    if (removed == 0) {
        show_info("No expired medicines", "There are no expired medicines to remove.");
        return IUP_DEFAULT;
    }
    storage_save_all();
    refresh_matrix();
    refresh_plot();
    show_info("Expired removed", "Expired medicines have been removed from stock.");
    return IUP_DEFAULT;
}

/* ------------------------------------------------------------------ */
/* Suppliers tab callbacks                                             */
/* ------------------------------------------------------------------ */

/* Join the texts of all selected items in the Supplies multi-list
   into a comma-separated string. For MULTIPLE lists, VALUE is a
   '+'/'-' pattern string indexed by 0-based position; item n (1-based)
   text is fetched with IDVALUE=n. */
static void supplies_from_selection(char *buf, size_t n)
{
    const char *v = IupGetAttribute(ui.sup_meds, "VALUE");
    size_t off = 0;
    int i;

    buf[0] = '\0';
    if (!v) return;
    for (i = 0; v[i]; i++) {
        if (v[i] == '+') {
            const char *t = IupGetAttributeId(ui.sup_meds, "IDVALUE", i + 1);
            if (t) {
                if (off) off += snprintf(buf + off, n - off, ", ");
                off += snprintf(buf + off, n - off, "%s", t);
                if (off >= n) break;
            }
        }
    }
}

static int cb_sup_add(Ihandle *self)
{
    char id[32], name[128], phone[32], supplies[256];
    int sup_id;
    Supplier *s;
    char detail[512];

    (void)self;

    snprintf(id, sizeof id, "%s", ent_val(ui.sup_id));      trim(id);
    snprintf(name, sizeof name, "%s", ent_val(ui.sup_name)); trim(name);
    snprintf(phone, sizeof phone, "%s", ent_val(ui.sup_phone)); trim(phone);
    supplies_from_selection(supplies, sizeof supplies);

    if (!*id)   { show_error(ui.main_dlg, "Supplier ID is required."); return IUP_DEFAULT; }
    if (!parse_int_field(id, &sup_id) || sup_id <= 0) {
        show_error(ui.main_dlg, "Supplier ID must be a positive integer.");
        return IUP_DEFAULT;
    }
    if (!*name) { show_error(ui.main_dlg, "Supplier name is required."); return IUP_DEFAULT; }
    if (find_supplier(sup_id) >= 0) {
        show_error(ui.main_dlg, "Supplier ID already exists.");
        return IUP_DEFAULT;
    }
    if (supplier_count >= MAX_SUPPLIERS) {
        show_error(ui.main_dlg, "Supplier list is full.");
        return IUP_DEFAULT;
    }

    s = &suppliers[supplier_count++];
    memset(s, 0, sizeof *s);
    s->id = sup_id;
    snprintf(s->name, sizeof s->name, "%s", name);
    snprintf(s->phone, sizeof s->phone, "%s", phone);
    snprintf(s->supplies, sizeof s->supplies, "%s", supplies);
    sanitize(s->name); sanitize(s->phone); sanitize(s->supplies);

    storage_save_all();
    refresh_sup_matrix();
    snprintf(detail, sizeof detail, "Added supplier %s (%s)", id, name);
    log_history("SUPPLIER_ADD", detail);
    IupSetAttribute(ui.sup_msg, "TITLE", "Supplier registered.");
    ent_clear(ui.sup_id); ent_clear(ui.sup_name);
    ent_clear(ui.sup_phone);
    IupSetAttribute(ui.sup_meds, "VALUE", "");   /* clear multi-selection */
    return IUP_DEFAULT;
}

static int cb_sup_del(Ihandle *self)
{
    char detail[512];
    (void)self;

    if (sel_sup < 0 || sel_sup >= supplier_count) {
        show_error(ui.main_dlg, "Please select a supplier row in the table first.");
        return IUP_DEFAULT;
    }
    if (IupMessageAlarm(ui.main_dlg, "Confirm",
            "Remove this supplier?", "YESNO") != 1)
        return IUP_DEFAULT;

    snprintf(detail, sizeof detail, "Removed supplier %d (%s)",
             suppliers[sel_sup].id, suppliers[sel_sup].name);
    sup_remove(sel_sup);
    sel_sup = -1;

    storage_save_all();
    refresh_sup_matrix();
    log_history("SUPPLIER_DEL", detail);
    return IUP_DEFAULT;
}

/* ------------------------------------------------------------------ */
/* Sales tab callbacks                                                 */
/* ------------------------------------------------------------------ */

static int cb_sell_save(Ihandle *self)
{
    char id[32], qty_s[16], cust[64], prescriptionNum[32];
    int qty, idx, med_id, sale_idx, rx_num;
    Medicine *m;
    Sale s;
    char *receipt, detail[512];

    (void)self;

    snprintf(id, sizeof id, "%s", ent_val(ui.sell_id));    trim(id);
    snprintf(qty_s, sizeof qty_s, "%s", ent_val(ui.sell_qty)); trim(qty_s);
    snprintf(cust, sizeof cust, "%s", ent_val(ui.sell_cust)); trim(cust);
    snprintf(prescriptionNum, sizeof prescriptionNum, "%s", ent_val(ui.sell_prescriptionNum)); trim(prescriptionNum);
    rx_num = 0;
    if (*prescriptionNum &&
        (!parse_int_field(prescriptionNum, &rx_num) || rx_num <= 0)) {
        show_error(ui.main_dlg, "PrescriptionNum must be a positive integer.");
        return IUP_DEFAULT;
    }

    if (!*id) { show_error(ui.main_dlg, "Medicine ID is required."); return IUP_DEFAULT; }
    if (!parse_int_field(id, &med_id) || med_id <= 0) {
        show_error(ui.main_dlg, "Medicine ID must be a positive integer.");
        return IUP_DEFAULT;
    }
    idx = find_medicine(med_id);
    if (idx < 0) { show_error(ui.main_dlg, "Medicine not found."); return IUP_DEFAULT; }
    m = &medicines[idx];

    if (med_is_expired(m)) {
        show_error(ui.main_dlg, "Expired medicine cannot be sold.");
        return IUP_DEFAULT;
    }
    if (!parse_int_field(qty_s, &qty) || qty <= 0) {
        show_error(ui.main_dlg, "Quantity must be an integer greater than 0.");
        return IUP_DEFAULT;
    }
    if (qty > m->qty) {
        show_error(ui.main_dlg, "Insufficient stock.");
        return IUP_DEFAULT;
    }
    if (m->prescriptionOnly && rx_num <= 0) {
        show_error(ui.main_dlg, "Prescription required for this medicine.");
        return IUP_DEFAULT;
    }

    m->qty -= qty;

    memset(&s, 0, sizeof s);
    s.med_id = m->id;
    s.qty = qty;
    s.unit_price = m->price;
    s.total = qty * m->price;
    snprintf(s.customer, sizeof s.customer, "%s", *cust ? cust : "Walk-in");
    s.prescriptionNum = rx_num;
    now_string(s.timestamp, sizeof s.timestamp);

    sale_idx = sales_add(&s);
    if (sale_idx < 0) {
        m->qty += qty;
        show_error(ui.main_dlg, "Sales log is full.");
        return IUP_DEFAULT;
    }

    storage_save_all();
    refresh_matrix();
    refresh_sales_matrix();
    refresh_sales_list(ui.disc_sales);
    refresh_plot();
    snprintf(detail, sizeof detail, "Sold %d x %d to %s, total %.2f",
             qty, m->id, s.customer, s.total);
    log_history("SELL", detail);

    receipt = sale_receipt_text(&sales[sale_idx], m);
    if (receipt) {
        show_info("Sales Receipt", receipt);
        free(receipt);
    }
    ent_clear(ui.sell_id); ent_clear(ui.sell_qty);
    ent_clear(ui.sell_cust); ent_clear(ui.sell_prescriptionNum);
    return IUP_DEFAULT;
}

static int cb_disc_apply(Ihandle *self)
{
    char pct_s[16], reason[64], out[512], detail[512];
    int sel, pct, idx;
    double discounted, disc_amt;
    Sale *s;

    (void)self;

    sel = IupGetInt(ui.disc_sales, "VALUE");
    if (sel < 1 || sel > sale_count) {
        show_error(ui.main_dlg, "Please select a sale from the list.");
        return IUP_DEFAULT;
    }
    idx = sel - 1;
    s = &sales[idx];

    snprintf(pct_s, sizeof pct_s, "%s", ent_val(ui.disc_pct)); trim(pct_s);
    if (!parse_int_field(pct_s, &pct) || pct < 5 || pct > 15) {
        show_error(ui.main_dlg, "Discount must be between 5 and 15 percent.");
        return IUP_DEFAULT;
    }
    snprintf(reason, sizeof reason, "%s", list_sel_text(ui.disc_reason)); trim(reason);

    disc_amt = s->total * pct / 100.0;
    discounted = sales_apply_discount(idx, pct, reason);

    storage_save_all();
    refresh_sales_matrix();
    snprintf(out, sizeof out,
             "Sale %d updated:\n"
             "Original total : %.2f\n"
             "Discount %.0f%% (%s): -%.2f\n"
             "Discounted amount: %.2f",
             sel, discounted + disc_amt, (double)pct, reason, disc_amt, discounted);
    show_info("Discount Applied", out);
    snprintf(detail, sizeof detail,
             "Applied %.0f%% discount (%s) to sale %d (%d): new total %.2f",
             (double)pct, reason, sel, s->med_id, discounted);
    log_history("DISCOUNT", detail);

    return IUP_DEFAULT;
}

/* ------------------------------------------------------------------ */
/* History / Home / exit callbacks                                     */
/* ------------------------------------------------------------------ */

static int cb_hist_refresh(Ihandle *self)
{
    (void)self;
    refresh_history();
    return IUP_DEFAULT;
}

static int cb_rep_refresh(Ihandle *self)
{
    (void)self;
    refresh_report();
    refresh_plot();
    return IUP_DEFAULT;
}

static int cb_reload(Ihandle *self)
{
    (void)self;
    storage_reload_all();
    sel_med = -1;
    sel_sup = -1;
    refresh_matrix();
    refresh_sup_matrix();
    refresh_sales_matrix();
    refresh_sales_list(ui.disc_sales);
    refresh_history();
    refresh_report();
    refresh_plot();
    show_info("Data reloaded",
              "All data/*.dat files were reloaded from disk.");
    return IUP_DEFAULT;
}

static int cb_rep_export(Ihandle *self)
{
    FILE *f;
    char *buf;
    (void)self;

    buf = malloc(8192);
    if (!buf) return IUP_DEFAULT;
    generate_report(buf, 8192);

    f = fopen("data/pharmacy_report.txt", "w");
    if (f) {
        fputs(buf, f);
        fclose(f);
        show_info("Export complete",
                  "Report exported to data/pharmacy_report.txt");
    } else {
        show_error(ui.main_dlg, "Could not write data/pharmacy_report.txt.");
    }
    free(buf);
    return IUP_DEFAULT;
}

static int cb_exit(Ihandle *self)
{
    (void)self;
    storage_save_all();
    IupExitLoop();
    return IUP_DEFAULT;
}

/* ------------------------------------------------------------------ */
/* Layout builders                                                     */
/* ------------------------------------------------------------------ */

static Ihandle *tab_page(const char *title, Ihandle *content)
{
    Ihandle *page = IupVbox(content, NULL);
    IupSetAttribute(page, "TABTITLE", title);
    IupSetAttribute(page, "EXPAND", "YES");
    return page;
}

static Ihandle *frame(Ihandle *child, const char *title)
{
    Ihandle *f = IupFrame(child);
    if (title) IupSetAttribute(f, "TITLE", title);
    return f;
}

static void build_meds_tab(void)
{
    Ihandle *form_add, *form_upd, *form_chk, *form_val;
    Ihandle *btn_save, *btn_upd, *btn_chk, *btn_val;
    Ihandle *btn_rem, *btn_rexp;
    Ihandle *left, *right, *btns;

    /* ----- add medicine form (2-column grid) ----- */
    ui.add_id    = entry();
    IupSetAttribute(ui.add_id, "MASKINT", "1:999999999");
    ui.add_name  = entry();
    ui.add_cat   = IupList(NULL);
    IupSetAttribute(ui.add_cat, "DROPDOWN", "YES");
    IupSetAttribute(ui.add_cat, "EXPAND", "HORIZONTAL");
    ui.add_price = entry();
    IupSetAttribute(ui.add_price, "MASKFLOAT", "0.01:9999999");
    ui.add_qty   = entry();
    IupSetAttribute(ui.add_qty, "MASKINT", "1:999999");
    ui.add_batch = entry();
    ui.add_mfg   = date_pick();
    ui.add_exp   = date_pick();
    ui.add_prescriptionOnly    = IupToggle("PrescriptionOnly", NULL);
    IupSetAttribute(ui.add_prescriptionOnly, "SIZE", "180x");
    ui.add_msg   = msg_label();

    form_add = IupGridBox(
        IupLabel("Medicine ID:"),   ui.add_id,
        IupLabel("Name:"),          ui.add_name,
        IupLabel("Category:"),      ui.add_cat,
        IupLabel("Unit price:"),    ui.add_price,
        IupLabel("Quantity:"),      ui.add_qty,
        IupLabel("Batch no:"),      ui.add_batch,
        IupLabel("Mfg date:"),      ui.add_mfg,
        IupLabel("Expiry date:"),   ui.add_exp,
        IupLabel(""),               ui.add_prescriptionOnly,
        IupLabel(""),               ui.add_msg,
        NULL);
    IupSetAttribute(form_add, "NUMDIV", "2");
    IupSetAttribute(form_add, "EXPAND", "HORIZONTAL");
    IupSetAttribute(form_add, "EXPANDCHILDREN", "HORIZONTAL");
    IupSetAttribute(form_add, "GAPCOL", "10");
    IupSetAttribute(form_add, "GAPLIN", "6");
    IupSetAttribute(form_add, "CMARGIN", "4x4");

    btn_save = IupButton("Save Medicine", NULL);
    IupSetCallback(btn_save, "ACTION", (Icallback)cb_add_save);
    form_add = IupVbox(frame(form_add, "Add Medicine"), btn_save, NULL);

    /* ----- update stock form ----- */
    ui.upd_id    = entry();
    IupSetAttribute(ui.upd_id, "MASKINT", "1:999999999");
    ui.upd_qty   = entry();
    IupSetAttribute(ui.upd_qty, "MASKINT", "1:999999");
    ui.upd_batch = entry();
    ui.upd_exp   = date_pick();
    ui.upd_msg   = msg_label();

    {
        Ihandle *f = IupGridBox(
            IupLabel("Medicine ID:"),    ui.upd_id,
            IupLabel("Add quantity:"),   ui.upd_qty,
            IupLabel("New batch no:"),   ui.upd_batch,
            IupLabel("New expiry:"),     ui.upd_exp,
            IupLabel(""),                ui.upd_msg,
            NULL);
        IupSetAttribute(f, "NUMDIV", "2");
        IupSetAttribute(f, "EXPAND", "HORIZONTAL");
    IupSetAttribute(f, "EXPANDCHILDREN", "HORIZONTAL");
    IupSetAttribute(f, "GAPCOL", "10");
    IupSetAttribute(f, "GAPLIN", "6");
    IupSetAttribute(f, "CMARGIN", "4x4");
        btn_upd = IupButton("Update Stock", NULL);
        IupSetCallback(btn_upd, "ACTION", (Icallback)cb_upd_save);
        form_upd = IupVbox(frame(f, "Update Stock"), btn_upd, NULL);
    }

    /* ----- check stock/expiry form ----- */
    ui.chk_id  = entry();
    IupSetAttribute(ui.chk_id, "MASKINT", "1:999999999");
    {
        Ihandle *f = IupGridBox(
            IupLabel("Medicine ID:"), ui.chk_id,
            NULL);
        IupSetAttribute(f, "NUMDIV", "2");
        IupSetAttribute(f, "EXPAND", "HORIZONTAL");
    IupSetAttribute(f, "EXPANDCHILDREN", "HORIZONTAL");
    IupSetAttribute(f, "GAPCOL", "10");
    IupSetAttribute(f, "GAPLIN", "6");
    IupSetAttribute(f, "CMARGIN", "4x4");
        btn_chk = IupButton("Check", NULL);
        IupSetCallback(btn_chk, "ACTION", (Icallback)cb_chk_show);
        form_chk = IupVbox(frame(f, "Check Stock / Expiry"), btn_chk, NULL);
    }

    /* ----- stock value ----- */
    btn_val = IupButton("Calculate Stock Value", NULL);
    IupSetCallback(btn_val, "ACTION", (Icallback)cb_val_show);
    form_val = IupVbox(btn_val, NULL);

    /* ----- remove buttons ----- */
    btn_rem  = IupButton("Remove Selected", NULL);
    btn_rexp = IupButton("Remove Expired", NULL);
    IupSetCallback(btn_rem,  "ACTION", (Icallback)cb_remove_selected);
    IupSetCallback(btn_rexp, "ACTION", (Icallback)cb_remove_expired);
    btns = IupHbox(btn_rem, btn_rexp, NULL);
    IupSetAttribute(btns, "GAP", "6");

    /* ----- assemble: forms left, table right ----- */
    left = IupVbox(form_add, form_upd, form_chk, form_val, btns, NULL);
    IupSetAttribute(left, "GAP", "12");
    IupSetAttribute(left, "EXPAND", "HORIZONTAL");
    IupSetAttribute(left, "FONT", "Segoe UI, 10");

    ui.matrix = IupMatrix(NULL);
    IupSetAttribute(ui.matrix, "READONLY", "YES");
    IupSetAttribute(ui.matrix, "EXPAND", "YES");
    IupSetCallback(ui.matrix, "ENTERITEM_CB", (Icallback)matrix_enteritem_cb);

    ui.status = IupLabel("");
    IupSetAttribute(ui.status, "EXPAND", "HORIZONTAL");

    right = IupVbox(frame(ui.matrix, "Medicines"), ui.status, NULL);
    IupSetAttribute(right, "GAP", "6");
    IupSetAttribute(right, "EXPAND", "YES");

    {
        Ihandle *split = IupSplit(left, right);
        IupSetAttribute(split, "VALUE", "430");
        IupSetAttribute(split, "MINMAX", "240:900");
        IupSetAttribute(split, "EXPAND", "YES");
        page_meds = tab_page("Medicines", split);
    }
}

static void build_sup_tab(void)
{
    Ihandle *f, *btn_add, *btn_del, *left, *right, *content;

    ui.sup_id    = entry();
    IupSetAttribute(ui.sup_id, "MASKINT", "1:999999999");
    ui.sup_name  = entry();
    ui.sup_phone = entry();
    /* multi-select list of unique medicine names this supplier provides.
       VISIBLELINES caps the natural height — without it the list demands
       one row per item and the form gets crushed by SHRINK layout. */
    ui.sup_meds  = IupList(NULL);
    IupSetAttribute(ui.sup_meds, "MULTIPLE", "YES");
    IupSetAttribute(ui.sup_meds, "VISIBLELINES", "8");
    IupSetAttribute(ui.sup_meds, "EXPAND", "HORIZONTAL");
    ui.sup_msg   = msg_label();

    f = IupGridBox(
        IupLabel("Supplier ID:"),  ui.sup_id,
        IupLabel("Name:"),         ui.sup_name,
        IupLabel("Phone:"),        ui.sup_phone,
        IupLabel("Supplies:"),     ui.sup_meds,
        IupLabel(""),              ui.sup_msg,
        NULL);
    IupSetAttribute(f, "NUMDIV", "2");
    IupSetAttribute(f, "EXPAND", "HORIZONTAL");
    IupSetAttribute(f, "EXPANDCHILDREN", "HORIZONTAL");
    IupSetAttribute(f, "GAPCOL", "10");
    IupSetAttribute(f, "GAPLIN", "6");
    IupSetAttribute(f, "CMARGIN", "4x4");

    btn_add = IupButton("Add Supplier", NULL);
    IupSetCallback(btn_add, "ACTION", (Icallback)cb_sup_add);
    btn_del = IupButton("Remove Selected", NULL);
    IupSetCallback(btn_del, "ACTION", (Icallback)cb_sup_del);

    left = IupVbox(frame(f, "Add Supplier"),
                   IupHbox(btn_add, btn_del, NULL), NULL);
    IupSetAttribute(left, "GAP", "12");
    IupSetAttribute(left, "EXPAND", "HORIZONTAL");
    IupSetAttribute(left, "FONT", "Segoe UI, 10");

    ui.sup_matrix = IupMatrix(NULL);
    IupSetAttribute(ui.sup_matrix, "READONLY", "YES");
    IupSetAttribute(ui.sup_matrix, "EXPAND", "YES");
    IupSetIntId(ui.sup_matrix, "WIDTH", 0, 30);
    IupSetIntId(ui.sup_matrix, "WIDTH", 1, 90);
    IupSetIntId(ui.sup_matrix, "WIDTH", 2, 160);
    IupSetIntId(ui.sup_matrix, "WIDTH", 3, 110);
    IupSetIntId(ui.sup_matrix, "WIDTH", 4, 500);
    IupSetCallback(ui.sup_matrix, "ENTERITEM_CB", (Icallback)sup_matrix_enteritem_cb);

    right = IupVbox(frame(ui.sup_matrix, "Registered Suppliers"), NULL);
    IupSetAttribute(right, "EXPAND", "YES");

    content = IupSplit(left, right);
    IupSetAttribute(content, "VALUE", "430");
    IupSetAttribute(content, "MINMAX", "240:900");
    IupSetAttribute(content, "EXPAND", "YES");
    page_sup = tab_page("Suppliers", content);
}

static void build_sales_tab(void)
{
    Ihandle *f_sell, *f_disc, *left, *right, *content;
    Ihandle *btn_sell, *btn_disc;

    /* ----- sell form ----- */
    ui.sell_id   = entry();
    IupSetAttribute(ui.sell_id, "MASKINT", "1:999999999");
    ui.sell_qty  = entry();
    IupSetAttribute(ui.sell_qty, "MASKINT", "1:999999");
    ui.sell_cust = entry();
    ui.sell_prescriptionNum   = entry();
    IupSetAttribute(ui.sell_prescriptionNum, "MASKINT", "1:999999999");
    IupSetAttribute(ui.sell_prescriptionNum, "TOOLTIP", "Required for PrescriptionOnly medicines");
    f_sell = IupGridBox(
        IupLabel("Medicine ID:"),   ui.sell_id,
        IupLabel("Quantity:"),      ui.sell_qty,
        IupLabel("Customer:"),      ui.sell_cust,
        IupLabel("PrescriptionNum:"), ui.sell_prescriptionNum,
        NULL);
    IupSetAttribute(f_sell, "NUMDIV", "2");
    IupSetAttribute(f_sell, "EXPAND", "HORIZONTAL");
    IupSetAttribute(f_sell, "EXPANDCHILDREN", "HORIZONTAL");
    IupSetAttribute(f_sell, "GAPCOL", "10");
    IupSetAttribute(f_sell, "GAPLIN", "6");
    IupSetAttribute(f_sell, "CMARGIN", "4x4");
    btn_sell = IupButton("Sell", NULL);
    IupSetCallback(btn_sell, "ACTION", (Icallback)cb_sell_save);
    f_sell = IupVbox(frame(f_sell, "Sell Medicine"), btn_sell, NULL);

    /* ----- discount form ----- */
    /* dropdown (not a tall flat list) so it can't overlap the rows below */
    ui.disc_sales  = IupList(NULL);
    IupSetAttribute(ui.disc_sales, "DROPDOWN", "YES");
    IupSetAttribute(ui.disc_sales, "EXPAND", "HORIZONTAL");
    ui.disc_pct    = entry();
    /* mask only restricts to digits; the 5-15 range is validated
       in cb_disc_apply (a 5:15 mask would block typing "1" for 10-15) */
    IupSetAttribute(ui.disc_pct, "MASKINT", "0:99");
    ui.disc_reason = IupList(NULL);
    IupSetAttribute(ui.disc_reason, "DROPDOWN", "YES");
    IupSetAttribute(ui.disc_reason, "EXPAND", "HORIZONTAL");
    f_disc = IupGridBox(
        IupLabel("Select sale:"),       ui.disc_sales,
        IupLabel("Discount % (5-15):"), ui.disc_pct,
        IupLabel("Reason:"),            ui.disc_reason,
        NULL);
    IupSetAttribute(f_disc, "NUMDIV", "2");
    IupSetAttribute(f_disc, "EXPAND", "HORIZONTAL");
    IupSetAttribute(f_disc, "EXPANDCHILDREN", "HORIZONTAL");
    IupSetAttribute(f_disc, "GAPCOL", "10");
    IupSetAttribute(f_disc, "GAPLIN", "6");
    IupSetAttribute(f_disc, "CMARGIN", "4x4");
    btn_disc = IupButton("Apply Discount", NULL);
    IupSetCallback(btn_disc, "ACTION", (Icallback)cb_disc_apply);
    f_disc = IupVbox(frame(f_disc, "Apply Discount"), btn_disc, NULL);

    left = IupVbox(f_sell, f_disc, NULL);
    IupSetAttribute(left, "GAP", "12");
    IupSetAttribute(left, "EXPAND", "HORIZONTAL");
    IupSetAttribute(left, "FONT", "Segoe UI, 10");

    ui.sales_matrix = IupMatrix(NULL);
    IupSetAttribute(ui.sales_matrix, "READONLY", "YES");
    IupSetAttribute(ui.sales_matrix, "EXPAND", "YES");

    right = IupVbox(frame(ui.sales_matrix, "Sales Records"), NULL);
    IupSetAttribute(right, "EXPAND", "YES");

    content = IupSplit(left, right);
    IupSetAttribute(content, "VALUE", "430");
    IupSetAttribute(content, "MINMAX", "240:900");
    IupSetAttribute(content, "EXPAND", "YES");
    page_sales = tab_page("Sales", content);
}

static void build_hist_tab(void)
{
    Ihandle *f, *btn, *left, *content;

    ui.hist_fid   = entry();
    ui.hist_fdate = date_pick();
    ui.hist_fdate_on = IupToggle("Apply date filter", NULL);
    IupSetAttribute(ui.hist_fdate_on, "VALUE", "OFF");
    IupSetCallback(ui.hist_fdate_on, "ACTION", (Icallback)cb_hist_refresh);

    ui.hist_matrix = IupMatrix(NULL);
    IupSetAttribute(ui.hist_matrix, "READONLY", "YES");
    IupSetAttribute(ui.hist_matrix, "EXPAND", "YES");
    IupSetIntId(ui.hist_matrix, "WIDTH", 0, 30);
    IupSetIntId(ui.hist_matrix, "WIDTH", 1, 140);
    IupSetIntId(ui.hist_matrix, "WIDTH", 2, 90);
    IupSetIntId(ui.hist_matrix, "WIDTH", 3, 90);
    IupSetIntId(ui.hist_matrix, "WIDTH", 4, 600);

    f = IupGridBox(
        IupLabel("Filter by medicine ID:"), ui.hist_fid,
        IupLabel("Filter by date:"),         ui.hist_fdate,
        IupLabel(""),                        ui.hist_fdate_on,
        NULL);
    IupSetAttribute(f, "NUMDIV", "2");
    IupSetAttribute(f, "EXPAND", "HORIZONTAL");
    IupSetAttribute(f, "EXPANDCHILDREN", "HORIZONTAL");
    IupSetAttribute(f, "GAPCOL", "10");
    IupSetAttribute(f, "GAPLIN", "6");
    IupSetAttribute(f, "CMARGIN", "4x4");

    btn = IupButton("Refresh / Apply Filter", NULL);
    IupSetCallback(btn, "ACTION", (Icallback)cb_hist_refresh);

    left = IupVbox(frame(f, "Filters"), btn, NULL);
    IupSetAttribute(left, "EXPAND", "HORIZONTAL");
    IupSetAttribute(left, "FONT", "Segoe UI, 10");

    content = IupSplit(left, frame(ui.hist_matrix, "Transaction History"));
    IupSetAttribute(content, "VALUE", "430");
    IupSetAttribute(content, "MINMAX", "240:900");
    IupSetAttribute(content, "EXPAND", "YES");
    page_hist = tab_page("History", content);
}

static void build_home_tab(void)
{
    Ihandle *btn_r, *btn_e, *btn_load, *btn_exit, *btns, *content;

    ui.rep_text = entry();
    IupSetAttribute(ui.rep_text, "MULTILINE", "YES");
    IupSetAttribute(ui.rep_text, "READONLY", "YES");
    IupSetAttribute(ui.rep_text, "EXPAND", "YES");
    IupSetAttribute(ui.rep_text, "SCROLLBAR", "VERTICAL");
    IupSetAttribute(ui.rep_text, "FONT", "Courier, 10");

    ui.plot = IupPlot();
    IupSetAttribute(ui.plot, "EXPAND", "YES");
    /* one x tick per month (default auto-spacing skips crowded labels)
       and rotate them 45 deg so all 12 month labels stay visible */
    IupSetAttribute(ui.plot, "AXS_XTICKAUTO", "NO");
    IupSetAttribute(ui.plot, "AXS_XTICKROTATENUMBER", "YES");
    IupSetAttribute(ui.plot, "AXS_XTICKROTATENUMBERANGLE", "45");

    btn_r = IupButton("Refresh", NULL);
    IupSetCallback(btn_r, "ACTION", (Icallback)cb_rep_refresh);
    btn_e = IupButton("Export Report", NULL);
    IupSetCallback(btn_e, "ACTION", (Icallback)cb_rep_export);
    btn_load = IupButton("Reload Data", NULL);
    IupSetCallback(btn_load, "ACTION", (Icallback)cb_reload);
    btn_exit = IupButton("Save && Exit", NULL);  /* && renders a literal & */
    IupSetCallback(btn_exit, "ACTION", (Icallback)cb_exit);
    btns = IupHbox(btn_r, btn_e, btn_load, btn_exit, NULL);
    IupSetAttribute(btns, "GAP", "6");

    {
        /* top/bottom split: report above, chart below, draggable */
        Ihandle *split = IupSplit(frame(ui.rep_text, "Summary Report"),
                                  frame(ui.plot, "Stock Expiry Forecast"));
        IupSetAttribute(split, "ORIENTATION", "HORIZONTAL");
        IupSetAttribute(split, "VALUE", "300");
        IupSetAttribute(split, "MINMAX", "120:600");
        IupSetAttribute(split, "EXPAND", "YES");

        content = IupVbox(split, btns, NULL);
    }
    IupSetAttribute(content, "GAP", "8");
    IupSetAttribute(content, "EXPAND", "YES");
    page_home = tab_page("Home", content);
}

static void build_tabs(void)
{
    build_home_tab();
    build_meds_tab();
    build_sup_tab();
    build_sales_tab();
    build_hist_tab();

    ui.tabs = IupTabs(page_home, page_meds, page_sup, page_sales, page_hist, NULL);
    IupSetAttribute(ui.tabs, "EXPAND", "YES");
    IupSetAttribute(ui.tabs, "TABTYPE", "TOP");
    IupSetCallback(ui.tabs, "TABCHANGE_CB", (Icallback)cb_tab_change);

    ui.main_dlg = IupDialog(ui.tabs);
    IupSetAttribute(ui.main_dlg, "TITLE", "Pharmacy Management System");
    IupSetAttribute(ui.main_dlg, "RASTERSIZE", "1100x700");
    IupSetAttribute(ui.main_dlg, "RESIZE", "YES");
    /* allow containers (frames/splits) to shrink below their natural
       size so every pane stays resizable; matrices scroll internally */
    IupSetAttribute(ui.main_dlg, "SHRINK", "YES");
}

/* ------------------------------------------------------------------ */
/* Entry point                                                         */
/* ------------------------------------------------------------------ */

int gui_run(int argc, char **argv)
{
    IupOpen(&argc, &argv);
    IupControlsOpen();
    IupPlotOpen();

    storage_load_all();
    build_tabs();

    IupShowXY(ui.main_dlg, IUP_CENTER, IUP_CENTER);

    /* IupList items can only be appended after the widget is mapped. */
    list_add(ui.add_cat, "Tablet");
    list_add(ui.add_cat, "Syrup");
    list_add(ui.add_cat, "Injection");
    list_add(ui.add_cat, "Ointment");
    IupSetInt(ui.add_cat, "VALUE", 1);

    list_add(ui.disc_reason, "Senior citizen");
    list_add(ui.disc_reason, "Insurance");
    list_add(ui.disc_reason, "Bulk purchase");
    IupSetInt(ui.disc_reason, "VALUE", 1);

    /* IupMatrix cells can only be filled after the widget is mapped. */
    refresh_matrix();
    refresh_sup_matrix();
    refresh_sales_matrix();
    refresh_sales_list(ui.disc_sales);
    refresh_history();
    refresh_report();
    refresh_plot();

    IupMainLoop();

    storage_save_all();
    IupDestroy(ui.main_dlg);
    IupClose();
    return 0;
}

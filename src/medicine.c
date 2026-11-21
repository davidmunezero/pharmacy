/*
 * medicine.c - medicine database, date utilities and stock valuation.
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#include "medicine.h"

Medicine medicines[MAX_MEDICINES];
int      medicine_count = 0;

/* ------------------------------------------------------------------ */
/* Date utilities (days-from-civil algorithm)                          */
/* ------------------------------------------------------------------ */

static long days_from_civil(int y, int m, int d)
{
    long era;
    unsigned yoe, doy, doe;
    y -= m <= 2;
    era = (y >= 0 ? y : y - 399) / 400;
    yoe = (unsigned)(y - era * 400);
    doy = (153 * (unsigned)(m + (m > 2 ? -3 : 9)) + 2) / 5 + (unsigned)d - 1;
    doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (long)doe - 719468;
}

static void civil_from_days(long z, Date *out)
{
    long era;
    unsigned doe, yoe, y, doy, mp;
    z += 719468;
    era = (z >= 0 ? z : z - 146096) / 146097;
    doe = (unsigned)(z - era * 146097);
    yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    y = yoe + era * 400;
    doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    mp = (5 * doy + 2) / 153;
    out->d = (int)(doy - (153 * mp + 2) / 5 + 1);
    out->m = (int)(mp + (mp < 10 ? 3 : -9));
    out->y = (int)(y + (out->m <= 2));
}

int date_valid_ymd(int y, int m, int d)
{
    Date t;
    if (y < 1900 || y > 2200 || m < 1 || m > 12 || d < 1 || d > 31) return 0;
    /* round-trip check catches impossible dates like Feb 30 */
    civil_from_days(days_from_civil(y, m, d), &t);
    return t.y == y && t.m == m && t.d == d;
}

int parse_date(const char *s, Date *d)
{
    int y, m, dd;
    if (sscanf(s, "%d-%d-%d", &y, &m, &dd) != 3) return 0;
    if (!date_valid_ymd(y, m, dd)) return 0;
    d->y = y; d->m = m; d->d = dd;
    return 1;
}

long date_days(const Date *d) { return days_from_civil(d->y, d->m, d->d); }

Date today_date(void)
{
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    Date d = { tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday };
    return d;
}

void fmt_date(Date d, char *buf, size_t n)
{
    snprintf(buf, n, "%04d-%02d-%02d", d.y, d.m, d.d);
}

void now_string(char *buf, size_t n)
{
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    strftime(buf, n, "%Y-%m-%d %H:%M:%S", tm);
}

/* ------------------------------------------------------------------ */
/* Medicine queries                                                    */
/* ------------------------------------------------------------------ */

int find_medicine(int id)
{
    int i;
    for (i = 0; i < medicine_count; i++)
        if (medicines[i].id == id) return i;
    return -1;
}

int days_to_expiry(const Medicine *m)
{
    Date t = today_date();
    return (int)(date_days(&m->expiry) - date_days(&t));
}

int med_is_expired(const Medicine *m) { return days_to_expiry(m) < 0; }

void med_remove(int idx)
{
    memmove(&medicines[idx], &medicines[idx + 1],
            (medicine_count - idx - 1) * sizeof(Medicine));
    medicine_count--;
}

double medicine_stock_value(int only_expired)
{
    int i;
    double total = 0.0;
    for (i = 0; i < medicine_count; i++) {
        int exp = med_is_expired(&medicines[i]);
        if (exp == only_expired)
            total += medicines[i].qty * medicines[i].price;
    }
    return total;
}

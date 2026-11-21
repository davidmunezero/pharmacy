#ifndef MEDICINE_H
#define MEDICINE_H

#include <stddef.h>

#define MAX_MEDICINES  1000
#define LOW_STOCK      10
#define EXPIRY_WARN    30

typedef struct {
    int y, m, d;
} Date;

typedef struct {
    int id;
    char name[128];
    char category[16];      /* Tablet, Syrup, Injection, Ointment */
    double price;
    int qty;
    char batch[32];
    int prescriptionOnly;   /* prescription required to sell */
    Date mfg;
    Date expiry;
    int supplier_id;            /* 0 = no supplier */
} Medicine;

/* medicine database (defined in src/medicine.c) */
extern Medicine medicines[MAX_MEDICINES];
extern int      medicine_count;

/* date utilities */
Date today_date(void);
int  date_valid_ymd(int y, int m, int d);
int  parse_date(const char *s, Date *d);
long date_days(const Date *d);
void fmt_date(Date d, char *buf, size_t n);
void now_string(char *buf, size_t n);

/* medicine queries */
int  find_medicine(int id);
int  days_to_expiry(const Medicine *m);
int  med_is_expired(const Medicine *m);
void med_remove(int idx);

/* stock valuation: only_expired=0 -> live stock, 1 -> expired stock */
double medicine_stock_value(int only_expired);

#endif /* MEDICINE_H */

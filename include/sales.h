#ifndef SALES_H
#define SALES_H

#define MAX_SALES 5000

#include "medicine.h"

typedef struct {
    int med_id;
    int qty;
    double unit_price;
    double total;           /* after discount, if any */
    char customer[64];
    int prescriptionNum;    /* 0 = none */
    char timestamp[32];     /* "YYYY-MM-DD HH:MM:SS" */
    double discount_pct;
    char discount_reason[64];
} Sale;

/* sales log (defined in src/sales.c) */
extern Sale sales[MAX_SALES];
extern int  sale_count;

/* returns index of the new sale, or -1 when the log is full */
int sales_add(const Sale *s);

/* applies a discount (percent) to an existing sale.
 * returns the new total, or -1 on invalid index */
double sales_apply_discount(int idx, int pct, const char *reason);

char *sale_receipt_text(const Sale *s, const Medicine *m);

#endif /* SALES_H */

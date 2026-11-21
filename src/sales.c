/*
 * sales.c - sales log, receipts support and discount handling.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sales.h"
#include "medicine.h"

Sale sales[MAX_SALES];
int  sale_count = 0;

int sales_add(const Sale *s)
{
    if (sale_count >= MAX_SALES) return -1;
    sales[sale_count] = *s;
    return sale_count++;
}

double sales_apply_discount(int idx, int pct, const char *reason)
{
    double disc_amt, discounted;
    if (idx < 0 || idx >= sale_count) return -1.0;

    disc_amt = sales[idx].total * pct / 100.0;
    discounted = sales[idx].total - disc_amt;

    sales[idx].discount_pct = pct;
    snprintf(sales[idx].discount_reason,
             sizeof sales[idx].discount_reason, "%s", reason);
    sales[idx].total = discounted;
    return discounted;
}

/* builds a printable receipt for a sale; returns a malloc'd string */
char *sale_receipt_text(const Sale *s, const Medicine *m)
{
    char *buf = malloc(1024);
    char idbuf[16], rxbuf[16];
    if (!buf) return NULL;
    if (!m) snprintf(idbuf, sizeof idbuf, "%d", s->med_id);
    if (s->prescriptionNum > 0)
        snprintf(rxbuf, sizeof rxbuf, "%d", s->prescriptionNum);
    else
        rxbuf[0] = 0;
    snprintf(buf, 1024,
             "----- SALES RECEIPT -----\n"
             "Medicine : %s\n"
             "Batch    : %s\n"
             "Qty      : %d\n"
             "Price    : %.2f each\n"
             "TOTAL    : %.2f\n"
             "Customer : %s\n"
             "PrescriptionNum: %s\n"
             "Date     : %s\n"
             "-------------------------",
             m ? m->name : idbuf,
             m ? m->batch : "-",
             s->qty, s->unit_price, s->total,
             s->customer, *rxbuf ? rxbuf : "-",
             s->timestamp);
    return buf;
}

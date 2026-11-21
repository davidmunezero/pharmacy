/*
 * supplier.c - supplier database helpers.
 */

#include <string.h>
#include <ctype.h>

#include "supplier.h"

Supplier suppliers[MAX_SUPPLIERS];
int      supplier_count = 0;

int find_supplier(int id)
{
    int i;
    for (i = 0; i < supplier_count; i++)
        if (suppliers[i].id == id) return i;
    return -1;
}

void sup_remove(int idx)
{
    memmove(&suppliers[idx], &suppliers[idx + 1],
            (supplier_count - idx - 1) * sizeof(Supplier));
    supplier_count--;
}

#ifndef SUPPLIER_H
#define SUPPLIER_H

#define MAX_SUPPLIERS 500

typedef struct {
    int id;
    char name[128];
    char phone[32];
    char supplies[256];     /* comma separated medicine ids/names */
} Supplier;

/* supplier database (defined in src/supplier.c) */
extern Supplier suppliers[MAX_SUPPLIERS];
extern int      supplier_count;

int find_supplier(int id);
void sup_remove(int idx);

#endif /* SUPPLIER_H */

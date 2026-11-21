# Pharmacy Management System (PMS)

## Project Information
- **Institution:** Adventist University of Central Africa
- **Faculty:** Faculty of Information Technology
- **Course:** INSY 8212 — Programming with C (Group J)
- **Instructor:** Tr. Justin Murenzi
- **Student:** Munezero David
- **Student ID:** 20251SEN104
- **Deadline:** November 22, 2026

## Introduction

This is a **Pharmacy Management System** implemented in C using IUP (Portable User Interface) for graphical user interface development. IUP is a native C library used for building graphical user interfaces.

The Pharmacy Management System supports the following functionalities:

1. Managing Medicines
2. Managing Suppliers
3. Sales Transactions and Reports
4. History (a log of all operations that occur in the system)

The system is designed with usability in mind and aims to replace manual pharmacy management processes with an intuitive graphical user interface. It provides menus, buttons, and text fields that facilitate accurate data entry. It also supports dynamic data views with sorting and filtering functionalities, making data analysis significantly easier than working with paper-based records.

The application is designed to be accessible to organizations without requiring prior technical knowledge of the system. Users simply download and run the executable file to start using the application. All data is persisted in files that remain under the organization's control, ensuring that its information stays private and accessible to the organization.

## Background

This system addresses several real-world challenges associated with managing pharmacy operations manually.

1. **Stock Management and Expiry Tracking:** Managing stock, ensuring that supplier information is accurately captured, and maintaining proper records of medicine sales are essential aspects of pharmacy operations. Recording information such as medicine expiry dates helps identify medicines that need to be replaced before they expire. Similarly, tracking the available quantity of each medicine makes it easier to determine when stock needs replenishment and to schedule resupply orders with suppliers.

2. **Transaction History, Accountability, and Auditing:** Maintaining transaction history is important for accountability and auditing. For example, if a customer experiences problems associated with a medicine they purchased, the pharmacy can refer to its transaction records to retrieve relevant information. Such records can also serve as supporting documentation in potential legal cases.

3. **Business Reporting and Decision-Making:** Reporting capabilities allow management to evaluate business performance, identify areas that require attention, and make informed decisions regarding marketing and sales strategies. Employees also benefit from more efficient day-to-day pharmacy operations through a system that simplifies data entry, record management, and reporting.

4. **Replacing Manual Record-Keeping:** Unlike manual record-keeping methods that rely on sheets of paper, registers, and other physical documents, a computerized system makes information easier to retrieve, organize, and analyze. Paper-based records can be misplaced or lost, and performing quick analyses using them can be difficult. By storing information digitally and providing automated reporting, the Pharmacy Management System makes pharmacy operations more efficient, organized, and reliable.

## Design

The **PMS** is designed using a multi-tab structure that separates sections in the application based on the kinds of operations you can do in those tabs.

| Tab | Description |
|---|---|
| **Home** | A report tab with summary figures, stock information, sales performance, an `IupPlot` chart, and Refresh / Export Report / Reload Data / Save & Exit buttons. |
| **Medicines** | Medicine registration, stock updates, removal, medicine table and stock valuation. |
| **Suppliers** | Supplier registration, removal, and supplier table. |
| **Sales** | Medicine sales form, discounts, receipts, and sales table. |
| **History** | Transaction records, sorting and filtering. |

Every data-entry form lives **inline** inside its tab as a two-column
`IupGridBox` (label | field) — there are no modal input dialogs.
Medicines, Suppliers and Sales each use a split layout: forms on the
left, a scrollable `IupMatrix` table on the right. Tab contents
refresh through the `TABCHANGE_CB` callback when a tab is selected.

### Project file structure

```text
pharmacy/
│
├── main.c                  // Entry point; calls gui_run()
│
├── include/                // Contains header files (function prototypes and structures)
│   ├── medicine.h          // Date/Medicine types, medicine DB, date utilities
│   ├── supplier.h          // Supplier type + supplier DB
│   ├── sales.h             // Sale type + sales log, discounts, receipts
│   ├── history.h           // HistoryEntry type + history recording/filtering
│   ├── storage.h           // storage_load_all() / storage_save_all()
│   └── gui.h               // gui_run() - builds the whole IUP interface
│
├── src/                    // Contains the core implementation of the program
│   ├── medicine.c          // Adding, removing, viewing, updating medicines,
│   │                       // date arithmetic, stock value
│   ├── supplier.c          // Adding, removing, and viewing suppliers
│   ├── sales.c             // Processing sales, calculating discounts,
│   │                       // generating receipts
│   ├── history.c           // Recording, displaying, sorting, and filtering
│   │                       // transactions
│   ├── storage.c           // Saving and loading data from files
│   └── gui.c               // Creating IUP windows, layouts, buttons,
│                           // tables and callbacks
│
├── data/                   // Stores files that persist after the program closes
│   ├── medicines.dat
│   ├── suppliers.dat
│   ├── sales.dat
│   ├── history.dat
│   └── pharmacy_report.txt // Written by "Export Report" on the Home tab
│
├── dist/                   // Ready-to-run distribution bundle
│   ├── pharmacy.exe        // Standalone executable (all libraries linked in)
│   ├── data/               // Copy of the data folder
│   └── README.txt          // End-user run instructions
│
├── Makefile                // Build rules (MSYS2 UCRT64 GCC)
└── README.md               // This file
```

This design ensures **separation of concerns** and **code modularity**, which in turn enhances **code readability** and **testing** compared to having all the app logic in one file. The in-memory databases are plain global arrays declared `extern` in the headers and defined once in the matching `.c` file:

| Module    | Array               | Capacity |
|-----------|---------------------|----------|
| medicine  | `medicines[]`       | 1000     |
| supplier  | `suppliers[]`       | 500      |
| sales     | `sales[]`           | 5000     |
| history   | `history_entries[]` | 5000     |

## Implementation

### Data Models

All identifiers are stored as **`int`** throughout the application:

- `Medicine.id`, `Medicine.supplier_id` (`0` = no supplier),
  `Supplier.id`, `Sale.med_id`, `Sale.prescriptionNum`
  (`0` = no prescription number).

```c
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
    int supplier_id;        /* 0 = no supplier */
} Medicine;

typedef struct {
    int id;
    char name[128];
    char phone[32];
    char supplies[256];     /* comma-separated medicine names */
} Supplier;

typedef struct {
    int med_id;
    int qty;
    double unit_price;
    double total;               /* after discount, if any */
    char customer[64];
    int prescriptionNum;        /* 0 = none */
    char timestamp[32];         /* "YYYY-MM-DD HH:MM:SS" */
    double discount_pct;
    char discount_reason[64];
} Sale;

typedef struct {
    char timestamp[32];
    char user[64];
    char action[24];
    char detail[512];
} HistoryEntry;
```

Dates are stored as a small `Date { y, m, d }` struct rather than
strings, which makes comparisons and arithmetic straightforward.

### Page Navigation

The main window is an `IupDialog` containing an `IupTabs` widget with
five tabs: **Home**, **Medicines**, **Suppliers**, **Sales** and
**History**. Each tab page is built once at startup; the
`TABCHANGE_CB` callback refreshes the visible tab's dynamic content
(matrices, lists, report, chart) so data is always current when the
user switches tabs.

All data entry is done through **inline forms** — `IupLabel` +
`IupText`/`IupList`/`IupDatePick`/`IupToggle` pairs arranged in a
two-column `IupGridBox` on the left side of each tab, with the
`IupMatrix` table on the right inside an `IupSplit` so the user can
drag the divider to resize panes.

### Managing Medicines

The Medicines tab provides five inline forms plus action buttons:

1. **Add Medicine** — ID, name, category (dropdown), unit price,
   quantity, batch number, manufacture and expiry dates
   (`IupDatePick`), supplier ID, and a "Prescription only" toggle.
   Validates unique ID, positive price/quantity, `expiry > mfg`,
   `expiry > today`, and a valid supplier reference.
2. **Update Stock** — medicine ID, additional quantity, new batch
   number and expiry date; adds to the existing quantity.
3. **Check Stock/Expiry** — enters a medicine ID and shows stock
   level, expiry date and batch in a dialog, followed by a
   low-stock warning (qty < 10) and/or an expiry warning (expired or
   expiring within 30 days) when applicable.
4. **Calculate Stock Value** — pops up a dialog with the total value
   of non-expired stock and the value of expired stock (the loss)
   computed separately via `medicine_stock_value()`.
5. **Remove Selected / Remove Expired** — removes one medicine by ID
   (with confirmation) or bulk-removes every expired medicine,
   logging each removal to history.

The medicine table colour-codes each row by status with the
precedence **grey → red → orange**: grey for expired (highest
priority), red for below-reorder stock, orange for expiring within
30 days, and default for OK.

### Managing Suppliers

The Suppliers tab has an inline form (ID, name, phone) and a
**multi-select `IupList`** (`MULTIPLE="YES"`) for the supplies field,
populated with unique medicine names sorted alphabetically. Selected
names are joined into a comma-separated string on save. The supplier
table shows ID, name, phone and supplies, with add and
remove-selected actions; every change is logged to history.

### Reporting

The Home tab shows a live summary report and an `IupPlot` bar chart
of units expiring per month over the next 12 months. The report
includes: total medicines, total stock units, stock value
(non-expired), expired-stock loss, total sales revenue, number of
sales, best-selling medicine (ID + name + units), the count of
medicines below the reorder level (<10), and the count expiring
within 30 days. "Export Report" writes the report to
`data/pharmacy_report.txt`, and "Reload Data" re-reads all `.dat`
files from disk without restarting.

#### The expiry chart

The chart answers one question for the pharmacist: **"How much stock
is about to expire, and when?"** Each bar represents one calendar
month (labelled e.g. `Nov 25` … `Oct 26`), and its height is the
**total quantity of units whose expiry date falls inside that month**.
A tall bar means a large batch is about to expire — a signal to run
promotions, transfer stock, or place smaller future orders. Already
expired medicines are excluded because they are dead stock, not
upcoming losses.

The chart is built in `refresh_plot()` (`src/gui.c`). Medicines are
first bucketed by how many months ahead they expire:

```c
for (i = 0; i < medicine_count; i++) {
    Medicine *m = &medicines[i];
    int months_ahead;
    if (med_is_expired(m)) continue;          /* skip dead stock */
    months_ahead = (m->expiry.y - today.y) * 12 + (m->expiry.m - today.m);
    if (months_ahead >= 0 && months_ahead < 12)
        buckets[months_ahead] += m->qty;      /* sum units per month */
}
```

Only medicines expiring within the next 12 months contribute; older
stock is already flagged grey in the Medicines table. The buckets are
then drawn as a bar series with month-name X labels:

```c
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
IupSetAttribute(ui.plot, "DS_COLOR", "255 90 0");   /* orange-red */
```

`IupPlotBegin/End` wraps the dataset, `IupPlotAddStr` supplies a
string label plus value per point, and `DS_MODE="BAR"` selects the
bar style. `refresh_plot()` is called on startup, on every
`TABCHANGE_CB` to Home, and after every stock-changing action (add,
update, sell, remove, discount, reload), so the chart always reflects
current data.

The text report is produced by `generate_report()` into an 8192-byte
buffer using successive `snprintf(buf + off, n - off, ...)` calls —
the offset pattern prevents buffer overruns. The best-seller is found
by summing `sales[k].qty` per `med_id` and keeping the maximum:

```c
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
```

Both expired-loss and near-expiry figures reuse the same date
utilities as the rest of the app: `days_to_expiry()` returns days
until expiry (negative = expired) and `medicine_stock_value(1)` sums
`qty * price` over expired items only.

### Handling Sales

The Sales tab has a **Sell** form (medicine ID, quantity, customer
name, prescription number) and an **Apply Discount** form (sale
picker dropdown, discount % with reason dropdown). Selling validates
the medicine exists, is not expired, meets the prescription
requirement, and has sufficient stock; it decrements stock, records
the sale and shows a formatted **receipt popup**. Discounts are
limited to 5–15% and show a popup with the original total, discount
amount and final total. Sales table lists all transactions.

### Storing Transaction History

Every operation — `ADD`, `UPDATE`, `SELL`, `REMOVE`, `DISCOUNT`,
`SUPPLIER_ADD`, `SUPPLIER_DEL`, `EXPIRED_REMOVED` — is appended to
`history.dat` via `log_history()` with a timestamp
(`YYYY-MM-DD HH:MM:SS`), the Windows user name, the action and a
detail string. The History tab shows these records in an `IupMatrix`
table with an `IupDatePick`-based date filter (optional toggle) and a
medicine-ID filter; `history_filter()` returns matching entries and
the table is re-rendered on refresh.

### File Persistence

All data is stored as plain text, one record per line, fields
separated by `|` (the `|` and newline characters are stripped from
user input):

- `data/medicines.dat` — `id|name|category|price|qty|batch|rx|mfg|expiry|supplier`
- `data/suppliers.dat` — `id|name|phone|supplies`
- `data/sales.dat`      — `med_id|qty|unit_price|total|customer|rx_no|timestamp|discount_pct|discount_reason`
- `data/history.dat`    — `timestamp|user|action|detail` (appended on every operation)
- `data/pharmacy_report.txt` — generated by Export Report

Data is re-saved to disk after every mutation, so nothing is lost
even if the program is closed unexpectedly. Dates use `YYYY-MM-DD`.
Loading is tolerant: missing files are ignored, and ID fields are
parsed with `atoi` so legacy non-numeric values degrade to `0`
rather than crashing.

### Input Validation

- **Masks:** `IupText` fields use `MASKINT`/`MASKFLOAT` so non-numeric
  input is rejected as it is typed (e.g. `MASKINT "1:999999999"` for
  IDs, `MASKINT "0:99"` for discount % — the range mask validates per
  keystroke, so the 5–15% rule is enforced in the callback instead).
- **Callbacks:** every save handler re-validates — unique IDs,
  positive quantities/prices, `expiry > mfg`, `expiry > today`,
  prescription number required for `prescriptionOnly` medicines,
  sufficient stock, discount between 5–15%, and supplier existence.
- **Dates:** `IupDatePick` guarantees a valid `YYYY-MM-DD` value;
  `parse_date()`/`date_valid_ymd()` double-check before storing.

## Building

From an MSYS2/UCRT64 shell (or Git Bash) in this folder:

```bash
export PATH="/c/msys64/ucrt64/bin:$PATH"
make            # produces pharmacy.exe
```

The Makefile compiles with `-static -static-libgcc -static-libstdc++`,
so the resulting `pharmacy.exe` links IUP, CD, freetype and the C/C++
runtimes statically — **no DLLs are required** at run time.

## Running

```bash
./pharmacy.exe
```

The program loads `data/medicines.dat`, `data/suppliers.dat`,
`data/sales.dat` and `data/history.dat` at start-up; missing files
are tolerated and created on the first save. The `data/` directory
must sit next to the exe — the program uses relative paths.

**It does not crash if a data file is missing** — every loader returns
early on a failed `fopen`, and saving creates the files as needed.

## Distribution

The `dist/` folder is a ready-to-run bundle for end users:

```
dist/
├── pharmacy.exe   # ~4 MB standalone executable (Windows 10+, 64-bit)
├── data/          # data files — must stay next to the exe
└── README.txt     # end-user instructions
```

Zip the **contents** of `dist/` and hand it in — recipients only need
to unzip and double-click `pharmacy.exe`. No compiler, installer or
extra libraries are required.

## Testing

The following manual test cases were executed against the built
`pharmacy.exe` (Windows 11, MSYS2 UCRT64 build):

| # | Test Case | Expected | Result |
|---|-----------|----------|--------|
| 1 | Launch app with existing `.dat` files | All 300 medicines, suppliers, sales and history load | Pass |
| 2 | Launch app with all `.dat` files deleted | App starts with empty tables, no crash | Pass |
| 3 | Add a medicine with valid data | Row appears in table, file updated, history logged | Pass |
| 4 | Add medicine with duplicate ID | Rejected with error dialog | Pass |
| 5 | Add medicine with expiry before manufacture date | Rejected | Pass |
| 6 | Add medicine with expiry in the past | Rejected | Pass |
| 7 | Add medicine with qty or price <= 0 | Rejected | Pass |
| 8 | Type letters into an ID field | Mask blocks non-numeric input | Pass |
| 9 | Update stock of existing medicine | Quantity increases, history logged | Pass |
| 10 | Check Stock/Expiry on low-stock item | Stock/expiry dialog + low-stock warning popup | Pass |
| 11 | Check Stock/Expiry on near-expiry item | Expiry warning popup shown | Pass |
| 12 | Sell in-stock, non-expired medicine | Stock decremented, receipt popup shown | Pass |
| 13 | Sell medicine requiring prescription without Rx no | Rejected | Pass |
| 14 | Sell more than available stock | Rejected | Pass |
| 15 | Sell expired medicine | Rejected (cannot be sold) | Pass |
| 16 | Apply 5–15% discount to a sale | Discount popup with new total; sales.dat updated | Pass |
| 17 | Enter discount <5% or >15% | Rejected by callback validation | Pass |
| 18 | Add supplier with multiple supplies selected | Comma-separated names saved, shown in table | Pass |
| 19 | Remove selected medicine / supplier | Confirmation, removal, history logged | Pass |
| 20 | Remove Expired with expired items present | All expired medicines deleted, history logged | Pass |
| 21 | Remove Expired with none expired | "No expired medicines" popup | Pass |
| 22 | Home report correctness | Counts for below-reorder and expiring items accurate | Pass |
| 23 | Home chart | 12-month expiry bar chart renders | Pass |
| 24 | Export Report | `data/pharmacy_report.txt` written | Pass |
| 25 | History date filter (DatePick) + ID filter | Table shows only matching entries | Pass |
| 26 | Reload Data after editing `.dat` files | Tables refresh from disk without restart | Pass |
| 27 | Save & Exit | All files saved, app closes cleanly | Pass |
| 28 | Restart app | All data persisted correctly | Pass |
| 29 | Run `dist/pharmacy.exe` standalone on clean folder | Launches with only system DLLs, no crash | Pass |

## Conclusion

The Pharmacy Management System provides a complete, persistent,
graphical pharmacy-management solution in C using IUP. Its tabbed
interface keeps medicines, suppliers, sales, reporting and audit
history organized and easy to use, while plain-text file storage
keeps all data under the organization's control. The modular
design — separate headers and source files per concern — made the
application straightforward to extend and test, and the statically
linked `dist/` build lets any Windows user run it without installing
a compiler or additional libraries.

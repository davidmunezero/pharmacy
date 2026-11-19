# Pharmacy Management System (PMS)

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
| **Home** | A report tab with summary figures, stock information, sales performance, and an export report button. |
| **Medicines** | Medicine registration, stock updates, removal, medicine table and stock valuation. |
| **Suppliers** | Supplier registration, removal, and supplier table. |
| **Sales** | Medicine sales form, discounts, receipts, and sales table. |
| **History** | Transaction records, sorting and filtering. |

The **project file structure** is as follows:
```text
pharmacy/
│
├── main.c                  // Entry point; starts the application
│
├── include/                // Contains header files (function prototypes and structures)
│   ├── medicine.h
│   ├── supplier.h
│   ├── sales.h
│   ├── history.h
│   └── gui.h
│
├── src/                    // Contains the core implementation of the program
│   ├── medicine.c          // Adding, removing, viewing, updating medicines, calculating stock value
│   ├── supplier.c          // Adding, removing, and viewing suppliers
│   ├── sales.c             // Processing sales, calculating discounts and generating receipts
│   ├── history.c           // Recording, displaying, sorting and filtering transactions
│   ├── storage.c           // Saving and loading data from files
│   └── gui.c               // Creating IUP windows, layouts, buttons, tables and callbacks
│
└── data/                   // Stores files that persist after the program closes
    ├── medicines.dat
    ├── suppliers.dat
    ├── supplies.dat
    ├── sales.dat
    └── history.dat
```
This design ensures **separation of concerns** and **code modularity**, which in turn enhances **code readability** and **testing** compared to having all the app logic in one file.

## Implementation

## Testing 

## Conclusion

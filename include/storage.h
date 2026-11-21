#ifndef STORAGE_H
#define STORAGE_H

/* All persistent data lives in the data/ directory:
 *   data/medicines.dat  data/suppliers.dat
 *   data/sales.dat      data/history.dat
 * Missing files are tolerated; directories/files are created on demand.
 */

void storage_load_all(void);
void storage_save_all(void);
/* resets all in-memory data and reloads every file in data/.
 * use after the files were edited externally while the app runs. */
void storage_reload_all(void);

#endif /* STORAGE_H */

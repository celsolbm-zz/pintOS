#ifndef _SUPTABLE_H
#define _SUPTABLE_H

#include <hash.h>
#include "filesys/file.h"

enum type_data {
    FILE_DATA,
    SWAP_FILE, 
    ZERO_PAGE,
};

struct sup_page_entry {
	struct hash_elem page_elem;
	void *addr;
	int usls;
	enum type_data type;
	bool alloced;
	uint32_t read_bytes;
	uint32_t zero_bytes;
	uint32_t file_page;
	off_t file_oft;
	bool writable;
	struct file *arq;
	void **esp1;
	void (**eip1) (void);
	void *pont;
	char **save_ptr1;	
};

bool init_sup_table (void);
struct sup_page_entry *sup_lookup (void *, struct hash);
void save_sup_page (void *, uint32_t, uint32_t, uint32_t, bool, int,
										struct file *, void **, void (**eip) (void),
										char **, void *);

#endif /* _SUPTABLE_H */

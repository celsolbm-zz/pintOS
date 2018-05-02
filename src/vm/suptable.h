#ifndef _SUPTABLE_H
#define _SUPTABLE_H

#include <hash.h>
#include "filesys/file.h"

enum type_data {
    FILE_DATA		= 1 << 0,
    SWAP_FILE		= 1 << 1, 
    ZERO_PAGE		= 1 << 2,
		PAGE_TABLE	= 1 << 3,
};

struct sup_page_entry {
	struct hash_elem page_elem;		/* hash table element */
	void *addr;										/* user vaddr of this entry */
	int usls;//debugging
	enum type_data type;					/* data location of this entry */
	bool alloced;									/* whether this entry is mapped with kernel */
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
void change_sup_data_location (struct sup_page_entry *, enum type_data);
struct sup_page_entry *sup_lookup (void *, struct hash *);
void save_sup_page (void *, uint32_t, uint32_t, uint32_t, bool, int,
										struct file *, void **, void (**eip) (void),
										char **, void *);

#endif /* _SUPTABLE_H */

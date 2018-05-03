#ifndef _VM_SUPTABLE_H
#define _VM_SUPTABLE_H

#include <hash.h>
#include "filesys/file.h"

#define STACK_HEURISTIC 40
#define MAX_STACK_SIZE	(8 * 1024 * 1024)		/* Max stack size is 8MB */

enum type_data {
    FILE_DATA		= 1 << 0,
    SWAP_FILE		= 1 << 1, 
		PAGE_TABLE	= 1 << 2,
#if 0 /* Currently, we don't need this */
    ZERO_PAGE		= 1 << 3,
#endif
};

struct sup_page_entry {
	void *upage;									/* user page addr of this entry */
	struct file *file;						/* associated file if any */
	off_t file_ofs;								/* file position for this spte */
	uint32_t read_bytes;					/* read bytes from file for this page */
	uint32_t zero_bytes;					/* zero bytes from file for this page */
	bool writable;								/* whether this page is writable */
	enum type_data type;					/* data location of this entry */
	bool alloced;									/* whether this entry has allocated frame */
	struct hash_elem page_elem;		/* hash table element */
};

bool init_sup_table (void);
bool change_sup_data_location (struct sup_page_entry *, enum type_data);
struct sup_page_entry *sup_lookup (void *); 
struct sup_page_entry *save_sup_page (void *, struct file *, off_t,
																			uint32_t, uint32_t, bool,
																			enum type_data);
bool stack_growth (void *);

#define DUMP_SUP_PAGE_ENTRY(spte)													\
	do {																										\
		printf ("=== SUP PAGE ENTRY DUMP ===\n");							\
		printf ("upage: %p\n", spte->upage);									\
		printf ("file_ofs: %d\n", spte->file_ofs);						\
		printf ("read_bytes: %u\n", spte->read_bytes);				\
		printf ("zero_bytes: %u\n", spte->zero_bytes);				\
		printf ("writable: %s\n", (spte->writable == true) ?	\
						"TRUE" : "FALSE");														\
		printf ("type: %s\n", (spte->type == FILE_DATA) ?			\
					 "FILE_DATA"	: (spte->type == SWAP_FILE) ?			\
					 "SWAP_FILE"	: (spte->type == ZERO_PAGE) ?			\
					 "ZERO_PAGE"	: (spte->type == PAGE_TABLE) ?		\
					 "PAGE_TABLE" : "STACK_PAGE");									\
		printf ("=== END SUP PAGE ENTRY DUMP ===\n\n");				\
	} while (0)

#endif /* _VM_SUPTABLE_H */

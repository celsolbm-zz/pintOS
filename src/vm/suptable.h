#ifndef _VM_SUPTABLE_H
#define _VM_SUPTABLE_H

#include <hash.h>
#include "filesys/file.h"
#include "vm/frame.h"

#define STACK_HEURISTIC 40
#define MAX_STACK_SIZE	(8 * 1024 * 1024)		/* Max stack size is 8MB */

enum type_data {
    FILE_DATA		= 1 << 0,
    SWAP_FILE		= 1 << 1, 
		PAGE_TABLE	= 1 << 2,
};

struct sup_page_entry {
	void *upage;										/* user page addr of this entry */
	struct file *file;							/* associated file if any */
	off_t file_ofs;									/* file position for this spte */
	uint32_t read_bytes;						/* read bytes from file for this page */
	uint32_t zero_bytes;						/* zero bytes from file for this page */
	bool writable;									/* whether this page is writable */
	enum type_data type;						/* data location of this entry */
	bool alloced;										/* whether this entry has allocated frame */
	size_t sw_addr;               	/* offset address of when the data is moved
																		 to the swap table */
	struct hash_elem page_elem;			/* hash table element */
};

bool init_sup_table (void);
void destroy_sup_table (void);
bool load_sup_data_to_frame (struct sup_page_entry *);
bool save_sup_data_to_swap (struct sup_page_entry *,
														struct frame_table_entry *);
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
					 "FILE_DATA" : (spte->type == SWAP_FILE) ?			\
					 "SWAP_FILE" : "PAGE_TABLE");										\
		printf ("allocated: %s\n", (spte->alloced == true) ?	\
				    "TRUE" : "FALSE");														\
		printf ("=== END SUP PAGE ENTRY DUMP ===\n\n");				\
	} while (0)

#endif /* _VM_SUPTABLE_H */

#ifndef _SWAPTABLE_H
#define _SWAPTABLE_H

#include <hash.h>
#include "devices/block.h"
#include "filesys/file.h"
#include "lib/kernel/bitmap.h"
#include "vm/suptable.h"
#include "frame.h"
struct swap_entry
{
	struct hash_elem swap_elem;
	struct block *swap_block;
	void *addr;
	int usls; //debugging
	bool alloced;
	uint32_t swap_off;
	uint32_t read_bytes;
	uint32_t zero_bytes;
	uint32_t file_page;
	off_t file_oft;
	bool writable;
};
struct hash swap_table;
struct bitmap *sw_table;
struct lock sw_lock;
#if 0
void 
init_swap_table (void);

struct 
swap_entry *swap_lookup (void *);

void 
save_swap (void *, uint32_t, uint32_t, uint32_t, bool);
#endif

void 
init_swap_table (void);

void 
swap_load (struct sup_page_entry *);

void
swap_read (struct sup_page_entry *, struct frame_table_entry *);

size_t
get_swap_address(struct sup_page_entry *);

#endif /* _SWAPTABLE */

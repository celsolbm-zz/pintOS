#ifndef _SWAPTABLE_H
#define _SWAPTABLE_H

#include <bitmap.h>
#include "vm/suptable.h"
#include "vm/frame.h"

#if 0
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

void init_swap_table (void);
struct swap_entry *swap_lookup (void *);
void save_swap (void *, uint32_t, uint32_t, uint32_t, bool);
#endif

void init_swap_table (void);
void swap_out (struct frame_table_entry *);
void swap_read (struct sup_page_entry *, struct frame_table_entry *);
void invalidate_swap_table (size_t);

#endif /* _SWAPTABLE */

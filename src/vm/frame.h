#ifndef _VM_FRAME_H
#define _VM_FRAME_H

#include <hash.h>
#include <list.h>
#include "threads/palloc.h"
#include "threads/synch.h"

struct frame_table_entry {
	void *kpage;									/* Kernel virtual address of this frame */
	struct sup_page_entry *spte;	/* Associated supplemental page table entry */
	struct thread *owner;					/* Owner thread of this frame */
	struct list_elem frame_elem;	/* Frame table list element */
};

struct list frame_table;	/* Lists for frame table entry */
struct lock frame_lock;		/* Lock for accessing frame table */

void init_frame_table (void);
struct frame_table_entry *get_user_frame (struct sup_page_entry *,
																					enum palloc_flags);
void free_user_frame (struct frame_table_entry *);
void *evict_frame_entry (enum palloc_flags);

#endif /* _VM_FRAME_H */

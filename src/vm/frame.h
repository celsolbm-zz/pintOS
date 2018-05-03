#ifndef _VM_FRAME_H
#define _VM_FRAME_H

#include <hash.h>
#include <list.h>
#include "threads/palloc.h"

struct frame_table_entry {
	void *kpage;									/* Kernel virtual address of this frame */
	struct sup_page_entry *spte;	/* Associated supplemental page table entry */
	struct thread *owner;					/* Owner thread of this frame */
	struct list_elem frame_elem;	/* Frame table list element */
};

void init_frame_table (void);
struct frame_table_entry *get_user_frame (struct sup_page_entry *,
																					enum palloc_flags);
void free_user_frame (struct frame_table_entry *);

#define DUMP_FRAME_TABLE_ENTRY(fte)												\
	do {																										\
		printf ("=== FRAME TABLE ENTRY DUMP ===\n");					\
		printf ("kpage: %p\n", fte->kpage);										\
		DUMP_SUP_PAGE_ENTRY(fte->spte);												\
		printf ("owner thread: %s\n", fte->owner->name);			\
		printf ("=== END FRAME TABLE ENTRY DUMP ===\n\n");		\
	} while (0)

#endif /* _VM_FRAME_H */

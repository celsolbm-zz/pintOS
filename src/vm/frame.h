#ifndef _FRAME_H
#define _FRAME_H

#include <hash.h>
#include <list.h>

struct frame_table_entry {
	void *paddr;									/* Physical address of this frame */
	struct sup_page_entry *spte;	/* Associated supplemental page table entry */
	struct thread *owner;					/* Owner thread of this frame */
	struct list_elem frame_elem;	/* Frame table list element */
};

struct list frame_table;	/* Lists for frame table entry */

bool init_frame_table (void);
struct frame_table_entry *frame_lookup (void *);
void save_frame_entry (void *);
void *evict_frame_entry (void);

#endif /* _FRAME_H */

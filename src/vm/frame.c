#include "vm/frame.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "vm/suptable.h"
#include <stdio.h>

static void *evict_frame_entry (enum palloc_flags);

static struct list frame_table;		/* List for frame table entry */
static struct lock frame_lock;		/* Lock for accessing frame table */

/*----------------------------------------------------------------------------*/
/* Initialize frame table */
void
init_frame_table (void)
{
	lock_init (&frame_lock);
	list_init (&frame_table);
}
/*----------------------------------------------------------------------------*/
/* Allocate a frame from user pool and return kernel virtual address */
struct frame_table_entry *
get_user_frame (struct sup_page_entry *spte, enum palloc_flags pal_flag)
{
	struct frame_table_entry *new_fte;
	uint8_t *kpage;

	kpage = palloc_get_page (pal_flag);
	if (kpage == NULL) {
		kpage = evict_frame_entry (pal_flag);
		if (kpage == NULL) {
			ASSERT (0);
		}
	}

	if (!lock_held_by_current_thread (&frame_lock))
		lock_acquire (&frame_lock);

	new_fte = malloc (sizeof(struct frame_table_entry));
	if (new_fte == NULL) {
		printf("[SAVE] FATAL! ALLOCATE NEW FRAME TABLE ENTRY FAILED!!!\n");
		return NULL;
	}

	new_fte->kpage = kpage;
	new_fte->spte = spte;
	new_fte->owner = thread_current ();

	list_push_back (&frame_table, &new_fte->frame_elem);
	lock_release (&frame_lock);

	return new_fte;
}
/*----------------------------------------------------------------------------*/
/* Remove frame from frame table and user pool */
void
free_user_frame (struct frame_table_entry *fte)
{

	if (!lock_held_by_current_thread (&frame_lock))
		lock_acquire (&frame_lock);
	list_remove (&fte->frame_elem);
	palloc_free_page (fte->kpage);
	free (fte);
	lock_release (&frame_lock);
}
/*----------------------------------------------------------------------------*/
/* Evict one frame table entry and return new user pool page */ 
static void *
evict_frame_entry (enum palloc_flags pal_flag)
{
	struct list_elem *e;
	struct frame_table_entry *fte;

	/* Find eviction entry until victim is found */
	
	if (!lock_held_by_current_thread (&frame_lock))
		lock_acquire (&frame_lock);

	e = list_begin (&frame_table);
	while (1) {
		fte = list_entry (e, struct frame_table_entry, frame_elem);

		if (pagedir_is_accessed (fte->owner->pagedir, fte->spte->upage)) {
			pagedir_set_accessed (fte->owner->pagedir, fte->spte->upage, false);
		} else {
			if (pagedir_is_dirty (fte->owner->pagedir, fte->spte->upage))	{
				/* Evict this entry (not accessed, dirty), move to swap */
				// printf ("(evict_frame_entry) VICTIM FOUND!!!\n");
				// DUMP_FRAME_TABLE_ENTRY (fte);
				change_sup_data_location (fte->spte, SWAP_FILE);
				list_remove (&fte->frame_elem);
				pagedir_clear_page (fte->owner->pagedir, fte->spte->upage);
				free_user_frame (fte);

				return palloc_get_page (pal_flag);
			}
		}

		e = list_next (e);
		if (e == list_end (&frame_table))
			e = list_begin (&frame_table);
	}

	return NULL;
}
/*----------------------------------------------------------------------------*/

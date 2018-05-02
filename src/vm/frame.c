#include "vm/frame.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "vm/suptable.h"
#include <stdio.h>

struct lock frame_lock;

/******************************************************************************/
/* Initialize frame table */
void
init_frame_table (void)
{
	lock_init (&frame_lock);
	list_init (&lru_list);
}
/******************************************************************************/
void
save_frame_entry (void *upage)
{
	struct frame_table_entry *new_fte;
	struct thread *cur;
	uint8_t *kpage;

	kpage = palloc_get_page (PAL_USER);
	cur = thread_current ();
	kpage = pagedir_get_page (cur->pagedir, upage);
	if (kpage == NULL) {
		printf("[SAVE] FATAL! NO VALID MAPPING FOR UPAGE(%p) EXISTS!!!\n", upage);
		return;
	}

	lock_acquire (&frame_lock);

	new_fte = malloc (sizeof(struct frame_table_entry));
	if (new_fte == NULL) {
		printf("[SAVE] FATAL! ALLOCATE NEW FRAME TABLE ENTRY FAILED!!!\n");
		return;
	}

	new_fte->paddr = (void *)vtop (kpage);
	new_fte->upage = upage;

	hash_insert (&frame_table, &new_fte->frame_elem);
	list_push_front (&lru_list, &new_fte->lru_elem);

	lock_release (&frame_lock);
}
/******************************************************************************/
/* Evict one frame table entry and return evicted entry's kernel virtual addr
   to the calling function */
void *
evict_frame_entry (void)
{
	struct list_elem *e, *not_access_not_dirty, *not_access_dirty,
									 *access_not_dirty, *access_dirty;
	struct frame_table_entry *fte;
	struct sup_page_entry *spte;
	struct thread *cur;
	uint32_t *pd;
	void *kpage, *ret_kpage;

	cur = thread_current ();
	pd = cur->pagedir;

	not_access_not_dirty = NULL;
	not_access_dirty = NULL;
	access_not_dirty = NULL;
	access_dirty = NULL;

	lock_acquire (&frame_lock);
	
	for (e = list_begin (&lru_list); e != list_end (&lru_list);
			 e = list_next (e)) {
		fte = list_entry (e, struct frame_table_entry, lru_elem);

		if (not_access_not_dirty == NULL) {
			if (!pagedir_is_dirty (pd, (const void *)fte->upage) &&
					!pagedir_is_accessed (pd, (const void *)fte->upage))
				not_access_not_dirty = e;
		} else if (not_access_dirty == NULL) {
			if (pagedir_is_dirty (pd, (const void *)fte->upage) &&
					!pagedir_is_accessed (pd, (const void *)fte->upage))
				not_access_dirty = e;
		} else if (access_not_dirty == NULL) {
			if (!pagedir_is_dirty (pd, (const void *)fte->upage) &&
					pagedir_is_accessed (pd, (const void *)fte->upage))
				access_not_dirty = e;
		} else if (access_dirty == NULL) {
			if (pagedir_is_dirty (pd, (const void *)fte->upage) &&
					pagedir_is_accessed (pd, (const void *)fte->upage))
				access_dirty = e;
		}
	}

	if (not_access_not_dirty != NULL) {
		/* First, evict not accessed, not dirty */
		fte = list_entry (not_access_not_dirty,
											struct frame_table_entry, lru_elem);
		list_remove (not_access_not_dirty);

		/* Save this user page to swap */
		spte = sup_lookup (fte->upage, &cur->page_table);
		change_sup_data_location (spte, SWAP_FILE);

		/* Return current kpage to user pool,
			 invalidate page tabe entry,
			 and get new kpage */
		kpage = pagedir_get_page (pd, fte->upage);
		palloc_free_page (kpage);
		pagedir_clear_page (pd, fte->upage);
		ret_kpage = palloc_get_page (PAL_USER);

		hash_delete (&frame_table, &fte->frame_elem);
		free (fte);
	} else if (not_access_dirty != NULL) {
		/* If fisrt case isn't found, then evict not accessed, dirty */
		fte = list_entry (not_access_dirty, struct frame_table_entry, lru_elem);
		list_remove (not_access_dirty);

		/* Save this user page to swap */
		spte = sup_lookup (fte->upage, &cur->page_table);
		change_sup_data_location (spte, SWAP_FILE);

		/* Clear dirty bit */
		pagedir_set_dirty (pd, fte->upage, false);

		/* Return current kpage to user pool,
			 invalidate page tabe entry,
			 and get new kpage */
		kpage = pagedir_get_page (pd, fte->upage);
		palloc_free_page (kpage);
		pagedir_clear_page (pd, fte->upage);
		ret_kpage = palloc_get_page (PAL_USER);

		hash_delete (&frame_table, &fte->frame_elem);
		free (fte);
	} else if (access_not_dirty != NULL) {
		/* If second case failed, then evict accessed, not dirty */
		fte = list_entry (access_not_dirty, struct frame_table_entry, lru_elem);
		list_remove (access_not_dirty);

		/* Save this user page to swap */
		spte = sup_lookup (fte->upage, &cur->page_table);
		change_sup_data_location (spte, SWAP_FILE);

		/* Clear access bit */
		pagedir_set_accessed (pd, fte->upage, false);

		/* Return current kpage to user pool,
			 invalidate page tabe entry,
			 and get new kpage */
		kpage = pagedir_get_page (pd, fte->upage);
		palloc_free_page (kpage);
		pagedir_clear_page (pd, fte->upage);
		ret_kpage = palloc_get_page (PAL_USER);

		hash_delete (&frame_table, &fte->frame_elem);
		free (fte);
	} else if (access_dirty != NULL) {
		/* Finally, evict accessed, dirty */
		fte = list_entry (access_dirty, struct frame_table_entry, lru_elem);
		list_remove (access_dirty);

		/* Save this user page to swap */
		spte = sup_lookup (fte->upage, &cur->page_table);
		change_sup_data_location (spte, SWAP_FILE);

		/* Clear dirty and access bit */
		pagedir_set_dirty (pd, fte->upage, false);
		pagedir_set_accessed (pd, fte->upage, false);

		/* Return current kpage to user pool,
			 invalidate page tabe entry,
			 and get new kpage */
		kpage = pagedir_get_page (pd, fte->upage);
		palloc_free_page (kpage);
		pagedir_clear_page (pd, fte->upage);
		ret_kpage = palloc_get_page (PAL_USER);

		hash_delete (&frame_table, &fte->frame_elem);
		free (fte);
	} else {
		printf("PANIC! NOTHING TO EVICT!!!\n");
		ret_kpage = NULL;
	}

	lock_release (&frame_lock);

	return ret_kpage;
}

#include "vm/suptable.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "vm/swaptable.h"
#include "vm/frame.h"
#include <stdio.h>
#include <string.h>

struct lock sup_lock;

/*----------------------------------------------------------------------------*/
static inline unsigned
page_hash (const struct hash_elem *po_, void *aux UNUSED)
{
  const struct sup_page_entry *po = hash_entry (po_,
																								struct sup_page_entry,
																								page_elem);
  return hash_bytes (&po->upage, sizeof po->upage);
}
/*----------------------------------------------------------------------------*/
/* Returns true if page a precedes page b. */
static inline bool
page_less (const struct hash_elem *sa_, const struct hash_elem *sb_,
           void *aux UNUSED)
{
  const struct sup_page_entry *sa = hash_entry (sa_,
																								struct sup_page_entry,
																								page_elem);
  const struct sup_page_entry *sb = hash_entry (sb_,
																								struct sup_page_entry,
																								page_elem);

  return sa->upage < sb->upage;
}
/*----------------------------------------------------------------------------*/
bool
init_sup_table (void)
{
	struct thread *cur = thread_current ();
	bool result;

	lock_init (&sup_lock);
	result = hash_init (&cur->page_table, page_hash, page_less, NULL);
	
	return result;
}
/*----------------------------------------------------------------------------*/
bool
change_sup_data_location (struct sup_page_entry *spte, enum type_data type)
{
	struct frame_table_entry *fte;
	enum palloc_flags pal_flag;

	switch (type) {
		case FILE_DATA:
			/* XXX: This case may not necessary */
			break;

		case SWAP_FILE:
			/* Move to swap space */

			ASSERT (spte->alloced == true);
			/* XXX: Call swap_out like function that move this page to swap */

			spte->alloced = false;
			break;

		case PAGE_TABLE:
			ASSERT (spte->alloced == false);
			/* Read acutal data from file or swap */
			
			if (spte->type == FILE_DATA) {
				/* Read file and set frame according to read_bytes and zero_bytes */
				pal_flag = PAL_USER;
				if (spte->read_bytes == 0)
					pal_flag |= PAL_ZERO;

				fte = get_user_frame (spte, pal_flag);
				if ((uint32_t)file_read_at (spte->file, fte->kpage,
						 spte->read_bytes, spte->file_ofs) != spte->read_bytes) {
					free_user_frame (fte);
					return false;
				}
				memset (fte->kpage + spte->read_bytes, 0, spte->zero_bytes);

				if (!install_page (spte->upage, fte->kpage, spte->writable)) {
					free_user_frame (fte);
					return false;
				}
				//printf ("INSTALL PAGE SUCCESS! upage: %p, kpage: %p\n", spte->upage, fte->kpage);
			} else if (spte->type == SWAP_FILE) {
				/* Read swap and set frame according to read_bytes and zero_bytes */
				/* XXX: swap_in like function may be called here */
			}
			spte->alloced = true;
			break;
	}

	spte->type = type;

	return true;
}
/*----------------------------------------------------------------------------*/
struct sup_page_entry *
sup_lookup (void *upage)
{
	struct sup_page_entry pg;
	struct hash_elem *e;
	struct hash *sup_table = &(thread_current ()->page_table);
	
	pg.upage = upage;
	e = hash_find (sup_table, &pg.page_elem);

	return (e != NULL) ? (hash_entry (e, struct sup_page_entry, page_elem)) :
											 (NULL);
}
/*----------------------------------------------------------------------------*/
struct sup_page_entry *
save_sup_page (void *upage, struct file *file, off_t ofs, uint32_t r_bytes,
							 uint32_t z_bytes, bool writable, enum type_data type)
{
	struct thread *cur = thread_current ();
	struct sup_page_entry *spte;

	lock_acquire (&sup_lock);
	spte = malloc (sizeof(struct sup_page_entry));
	if (spte == NULL) {
		printf ("FATAL! CAN'T ALLOCATE SUP PAGE ENTRY!!!\n");
		lock_release (&sup_lock);
		return NULL;
	}

	spte->upage = upage;
	spte->file = file;
	spte->file_ofs = ofs;
	spte->read_bytes = r_bytes;
	spte->zero_bytes = z_bytes;
	spte->writable = writable;
	spte->type = type;
	spte->alloced = false;

	hash_insert(&cur->page_table, &spte->page_elem);
	lock_release (&sup_lock);

	return spte;
}
/*----------------------------------------------------------------------------*/
bool
stack_growth (void *new_stack_ptr)
{
	bool success;
	void *kpage;
	struct sup_page_entry *spte;
	struct frame_table_entry *fte;

	new_stack_ptr = pg_round_down (new_stack_ptr);
	// printf ("(stack_growth) PHYS_BASE - new_stack_ptr: %u, MAX_STACK_SIZE: %u\n",
	// 				(uint32_t)(PHYS_BASE - new_stack_ptr), MAX_STACK_SIZE);
	if ((uint32_t)(PHYS_BASE - new_stack_ptr) > MAX_STACK_SIZE)
		return false;

	spte = save_sup_page (new_stack_ptr, NULL, 0, 0, 0, true, PAGE_TABLE);
	fte = get_user_frame (spte, PAL_USER | PAL_ZERO);
	kpage = fte->kpage;

	// printf ("(stack_growth) NEW_STACK_PTR: %p\n", new_stack_ptr);
	success = install_page (new_stack_ptr, kpage, true);
	if (!success) {
		free_user_frame (fte);
		return success;
	}
	
	// printf ("(stack_growth) INSTALL PAGE SUCCESS! upage: %p, kpage: %p\n",
	//				 spte->upage, fte->kpage);

	return success;
}
/*----------------------------------------------------------------------------*/

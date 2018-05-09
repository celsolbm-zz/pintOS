#include "vm/suptable.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "userprog/syscall.h"
#include "vm/swaptable.h"
#include <stdio.h>
#include <string.h>

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
/* sup page table entry deletion function when destroying sup page table */
static inline void
sup_table_delete (struct hash_elem *e, void *aux UNUSED)
{
	struct sup_page_entry *spte;
	struct thread *cur = thread_current ();
	void *kpage;

	spte = hash_entry (e, struct sup_page_entry, page_elem);
	if (spte->type == PAGE_TABLE) {
		ASSERT (spte->alloced);
		kpage = pagedir_get_page (cur->pagedir, spte->upage);
		free_user_frame (search_user_frame (kpage));
		pagedir_clear_page (cur->pagedir, spte->upage);
	} else if (spte->type == SWAP_FILE) {
		ASSERT (!spte->alloced);
		invalidate_swap_table (spte->sw_addr);
	}
	
	free (spte);
}
/*----------------------------------------------------------------------------*/
/* Initialize supplemental page table */
bool
init_sup_table (void)
{
	struct thread *cur = thread_current ();
	bool result;

	lock_init (&cur->sup_lock);
	result = hash_init (&cur->page_table, page_hash, page_less, NULL);
	
	return result;
}
/*----------------------------------------------------------------------------*/
void
destroy_sup_table (void)
{
	struct thread *cur = thread_current ();

	hash_destroy (&cur->page_table, sup_table_delete);
}
/*----------------------------------------------------------------------------*/
bool
load_sup_data_to_frame (struct sup_page_entry *spte)
{
	struct frame_table_entry *new_fte;
	enum palloc_flags pal_flag;
	struct thread *cur;

	ASSERT (spte->alloced == false);

	cur = thread_current ();
	
	if (!lock_held_by_current_thread (&cur->sup_lock))
		lock_acquire (&cur->sup_lock);

	if (spte->type == FILE_DATA) {
		/* Read file and set frame according to read_bytes and zero_bytes */
		pal_flag = PAL_USER;
		if (spte->read_bytes == 0)
			pal_flag |= PAL_ZERO;

		new_fte = get_user_frame (spte, pal_flag);

		lock_acquire (&filesys_lock);
		if (file_read_at (spte->file, new_fte->kpage,
				spte->read_bytes, spte->file_ofs) != (off_t)spte->read_bytes)
			goto fail;

		lock_release (&filesys_lock);
		memset (new_fte->kpage + spte->read_bytes, 0, spte->zero_bytes);

		if (!install_page (spte->upage, new_fte->kpage, spte->writable))
			goto fail;

	} else if (spte->type == SWAP_FILE) {
		/* Read swap and set frame according to read_bytes and zero_bytes */
		pal_flag = PAL_USER;
		if (spte->read_bytes == 0)
			pal_flag |= PAL_ZERO;

		new_fte = get_user_frame (spte, pal_flag);
		if (!install_page (spte->upage, new_fte->kpage, spte->writable))
			goto fail;

		swap_read (spte, new_fte);
	} else {
		ASSERT (0);
	}

	spte->type = PAGE_TABLE;
	spte->alloced = true;
	lock_release (&cur->sup_lock);
	return true;

fail:
	free_user_frame (new_fte);
	if (lock_held_by_current_thread (&filesys_lock))
		lock_release (&filesys_lock);
	lock_release (&cur->sup_lock);
	return false;
}
/*----------------------------------------------------------------------------*/
/* Move to swap space */
bool
save_sup_data_to_swap (struct sup_page_entry *spte,
											 struct frame_table_entry *fte)
{
	struct thread *cur = thread_current ();

	ASSERT (spte->alloced == true);
	if (cur != fte->owner)
		lock_acquire (&fte->owner->sup_lock);

	swap_out (fte);
	spte->alloced = false;
	spte->type = SWAP_FILE;

	if (cur != fte->owner)
		lock_release (&fte->owner->sup_lock);

	return true;
}
/*----------------------------------------------------------------------------*/
/* Look up sup page table using UPAGE address */
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
/* Create and save new sup apge table entry */
struct sup_page_entry *
save_sup_page (void *upage, struct file *file, off_t ofs, uint32_t r_bytes,
							 uint32_t z_bytes, bool writable, enum type_data type)
{
	struct thread *cur = thread_current ();
	struct sup_page_entry *spte;

	if (!lock_held_by_current_thread (&cur->sup_lock))
		lock_acquire (&cur->sup_lock);

	spte = malloc (sizeof(struct sup_page_entry));
	if (spte == NULL) {
		printf ("FATAL! CAN'T ALLOCATE SUP PAGE ENTRY!!!\n");
		lock_release (&cur->sup_lock);
		return NULL;
	}

	spte->upage = upage;
	spte->file = file;
	spte->file_ofs = ofs;
	spte->read_bytes = r_bytes;
	spte->zero_bytes = z_bytes;
	spte->writable = writable;
	spte->type = type;
	spte->alloced = (type == PAGE_TABLE) ? true : false;

	hash_insert (&cur->page_table, &spte->page_elem);
	lock_release (&cur->sup_lock);

	return spte;
}
/*----------------------------------------------------------------------------*/
/* Allocate new stack page at NEW_STACK_PTR address */
bool
stack_growth (void *new_stack_ptr)
{
	bool success;
	void *kpage;
	struct sup_page_entry *spte;
	struct frame_table_entry *fte;

	new_stack_ptr = pg_round_down (new_stack_ptr);
	if ((uint32_t)(PHYS_BASE - new_stack_ptr) > MAX_STACK_SIZE)
		return false;

	spte = save_sup_page (new_stack_ptr, NULL, 0, PGSIZE, 0, true, PAGE_TABLE);
	fte = get_user_frame (spte, PAL_USER | PAL_ZERO);
	kpage = fte->kpage;

	success = install_page (new_stack_ptr, kpage, true);
	if (!success) {
		free_user_frame (fte);
		return success;
	}
	
	return success;
}
/*----------------------------------------------------------------------------*/

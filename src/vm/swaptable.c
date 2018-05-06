#include "swaptable.h"
#include "userprog/pagedir.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include <round.h>
#include "lib/kernel/bitmap.h"
#include "suptable.h"
#include "frame.h"
#include "threads/vaddr.h"
#include <stdio.h>
#include <string.h>
#define SECTOR_SIZE 512


#if 0
struct lock swap_lock;

/******************************************************************************/
static inline unsigned
swap_hash (const struct hash_elem *sw_, void *aux UNUSED)
{
  const struct swap_entry *sw = hash_entry (sw_, struct swap_entry, swap_elem);

  return hash_bytes (&sw->addr, sizeof sw->addr);
}
/******************************************************************************/
/* Returns true if swap a precedes swap b. */
static inline bool
swap_less (const struct hash_elem *sa_, const struct hash_elem *sb_,
           void *aux UNUSED)
{
  const struct swap_entry *sa = hash_entry (sa_, struct swap_entry, swap_elem);
  const struct swap_entry *sb = hash_entry (sb_, struct swap_entry, swap_elem);

  return sa->addr < sb->addr;
}

/******************************************************************************/
struct swap_entry *
swap_lookup (void *address)
{
	struct swap_entry swp_ent;
	struct hash_elem *e;
	
	swp_ent.addr = address;
	e = hash_find (&swap_table, &swp_ent.swap_elem);

	return (e != NULL) ? (hash_entry (e, struct swap_entry, swap_elem)) :
											 (NULL);
}
/******************************************************************************/
void
save_swap (void *address, uint32_t r_bytes, uint32_t z_bytes,
					 uint32_t file_p, bool wr)
{
	struct swap_entry *new_swap = malloc (sizeof(struct swap_entry));

	lock_acquire (&swap_lock);
	new_swap->read_bytes = r_bytes;
	new_swap->zero_bytes = z_bytes;
	new_swap->addr = address;
	new_swap->writable = wr;
	new_swap->file_page = file_p;
	new_swap->swap_block=block_get_role(BLOCK_SWAP);

	hash_insert(&swap_table, &new_swap->swap_elem);
	lock_release (&swap_lock);
}
/******************************************************************************/

#endif 	
	
/******************************************************************************/
void
init_swap_table (void)
{
  sw_table = bitmap_create(8192);
  lock_init(&sw_lock);
	
	
}
	
/******************************************************************************/	
void 
swap_load (struct sup_page_entry *swap)
{



	uint32_t sector = swap->sw_addr;
	struct thread *cur;
	void *kaddr;
	int loops, index;



	if (!lock_held_by_current_thread(&sw_lock))
		lock_acquire(&sw_lock);	

	cur = thread_current ();
	kaddr = pagedir_get_page (cur->pagedir, swap->upage);
	//kaddr=swap->upage;
	loops = DIV_ROUND_UP(swap->read_bytes+swap->zero_bytes,SECTOR_SIZE);
	index = 0;
	while (index < loops) {
		block_write (block_get_role(BLOCK_SWAP), sector, kaddr); 
		kaddr += 0x200;
		sector++;
		index++;
	}
	swap->type = SWAP_FILE;	
	lock_release(&sw_lock);

}
/******************************************************************************/

void
swap_read (struct sup_page_entry *swap, struct frame_table_entry *fte)
{

	int loops = DIV_ROUND_UP(swap->read_bytes+swap->zero_bytes,SECTOR_SIZE);
	int index = 0;
	int sector = swap->sw_addr;
	if (!lock_held_by_current_thread(&sw_lock))
		lock_acquire(&sw_lock);	
	void *paddr=fte->kpage;
	//void *paddr=swap->upage;
	while (index < loops) {
		block_read (block_get_role(BLOCK_SWAP), sector, paddr);
		paddr += 0x200;
		sector++;
		index++;
	}
	memset(fte->kpage+swap->read_bytes,0,swap->zero_bytes);
  bitmap_set_multiple(sw_table,swap->sw_addr,(size_t) loops,false);
	lock_release(&sw_lock);
}



size_t
get_swap_address(struct sup_page_entry *entry)
{	//bool tst;
//tst = lock_held_by_current_thread(&sw_lock);
//printf("\n  value of bool is %d \n",tst); 
	if (!lock_held_by_current_thread(&sw_lock))
		lock_acquire(&sw_lock);	
	size_t cnt = DIV_ROUND_UP(entry->read_bytes+entry->zero_bytes,SECTOR_SIZE);
	size_t ret = bitmap_scan_and_flip(sw_table,0,cnt,false);
	lock_release(&sw_lock);
	return ret;
}

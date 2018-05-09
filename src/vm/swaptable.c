#include "vm/swaptable.h"
#include "devices/block.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include <round.h>
#include <stdio.h>
#include <string.h>

#define SECTORS_PER_PAGE (PGSIZE / BLOCK_SECTOR_SIZE)

struct block *swap;
struct lock sw_lock;
struct bitmap *sw_table;

/*----------------------------------------------------------------------------*/
void
init_swap_table (void)
{
	swap = block_get_role (BLOCK_SWAP);
  sw_table = bitmap_create (block_size (swap));
	bitmap_set_all (sw_table, false);
  lock_init (&sw_lock);
}
/*----------------------------------------------------------------------------*/
void 
swap_out (struct frame_table_entry *fte)
{
	struct sup_page_entry *spte;
	void *kpage;
	size_t i;

	if (!lock_held_by_current_thread (&sw_lock))
		lock_acquire (&sw_lock);	

	spte = fte->spte;
	kpage = fte->kpage;
	spte->sw_addr = bitmap_scan_and_flip (sw_table, 0, SECTORS_PER_PAGE, false);
	ASSERT (spte->sw_addr % SECTORS_PER_PAGE == 0);

	for (i = 0; i < SECTORS_PER_PAGE; i++) {
		block_write (swap, spte->sw_addr + i,
								 (uint8_t *)kpage + i * BLOCK_SECTOR_SIZE); 
	}

	lock_release (&sw_lock);
}
/*----------------------------------------------------------------------------*/
void
swap_read (struct sup_page_entry *spte, struct frame_table_entry *fte)
{
	size_t i;
	void *kpage;

	if(!lock_held_by_current_thread(&sw_lock))
		lock_acquire (&sw_lock);	
	kpage = fte->kpage;

	for (i = 0; i < SECTORS_PER_PAGE; i++) {
		ASSERT (bitmap_test (sw_table, spte->sw_addr + i) == true);
		block_read (swap, spte->sw_addr + i,
								 (uint8_t *)kpage + i * BLOCK_SECTOR_SIZE); 
	}

	for (i = 0; i < SECTORS_PER_PAGE; i++) {
		bitmap_flip (sw_table, spte->sw_addr + i);
		ASSERT (bitmap_test (sw_table, spte->sw_addr + i) == false);
	}
	lock_release (&sw_lock);
}
/*----------------------------------------------------------------------------*/
void
invalidate_swap_table (size_t sector)
{
	size_t i;

	if(!lock_held_by_current_thread(&sw_lock))
		lock_acquire (&sw_lock);	
	for (i = 0; i < SECTORS_PER_PAGE; i++) {
		ASSERT (bitmap_test (sw_table, sector + i) == true);
		bitmap_flip (sw_table, sector + i);
	}
	lock_release (&sw_lock);
}
/*----------------------------------------------------------------------------*/

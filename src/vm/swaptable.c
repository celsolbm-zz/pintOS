#include "vm/swaptable.h"
#include "devices/block.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include <round.h>
#include <stdio.h>
#include <string.h>

#define SECTORS_PER_PAGE (PGSIZE / BLOCK_SECTOR_SIZE)

static struct block *swap;
static struct lock sw_lock;
static struct bitmap *sw_table;

#if 0
struct lock swap_lock;

/*----------------------------------------------------------------------------*/
static inline unsigned
swap_hash (const struct hash_elem *sw_, void *aux UNUSED)
{
  const struct swap_entry *sw = hash_entry (sw_, struct swap_entry, swap_elem);

  return hash_bytes (&sw->addr, sizeof sw->addr);
}
/*----------------------------------------------------------------------------*/
/* Returns true if swap a precedes swap b. */
static inline bool
swap_less (const struct hash_elem *sa_, const struct hash_elem *sb_,
           void *aux UNUSED)
{
  const struct swap_entry *sa = hash_entry (sa_, struct swap_entry, swap_elem);
  const struct swap_entry *sb = hash_entry (sb_, struct swap_entry, swap_elem);

  return sa->addr < sb->addr;
}
/*----------------------------------------------------------------------------*/
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
/*----------------------------------------------------------------------------*/
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
#endif 	
/*----------------------------------------------------------------------------*/
void
init_swap_table (void)
{
	swap = block_get_role (BLOCK_SWAP);
  sw_table = bitmap_create (block_size (swap));
	//printf ("=== (init_swap_table) bitmap size: %zu ===\n",
	//				bitmap_size (sw_table));
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

	lock_acquire (&sw_lock);	
	
	spte = fte->spte;
	kpage = fte->kpage;
	spte->sw_addr = bitmap_scan_and_flip (sw_table, 0, SECTORS_PER_PAGE, false);
	ASSERT (spte->sw_addr % SECTORS_PER_PAGE == 0);

	for (i = 0; i < SECTORS_PER_PAGE; i++) {
		ASSERT (bitmap_test (sw_table, spte->sw_addr + i) == true);
		block_write (swap, spte->sw_addr + i,
								 (uint8_t *)kpage + i * BLOCK_SECTOR_SIZE); 
	}

#if 0
	if (spte->sw_addr == 0) {
		printf ("\n ===== START SWAP_OUT =====\n");
		DUMP_FRAME_TABLE_ENTRY (fte);
		hex_dump (0, (const void *)kpage, PGSIZE, true);
		printf ("===== END SWAP_OUT =====\n");
	}
#endif

	lock_release (&sw_lock);
}
/*----------------------------------------------------------------------------*/
void
swap_read (struct sup_page_entry *spte, struct frame_table_entry *fte)
{
	size_t i;
	void *kpage;

	lock_acquire (&sw_lock);	

	kpage = fte->kpage;

	//printf ("(swap_read) start sector: %zu start kpage: %p\n",
	//				spte->sw_addr, kpage);
	for (i = 0; i < SECTORS_PER_PAGE; i++) {
		ASSERT (bitmap_test (sw_table, spte->sw_addr + i) == true);
		block_read (swap, spte->sw_addr + i,
								 (uint8_t *)kpage + i * BLOCK_SECTOR_SIZE); 
		//printf ("(swap_read) cur sector: %zu, kpage: %p\n",
		//				spte->sw_addr + i, kpage + i * BLOCK_SECTOR_SIZE);
	}

#if 0
	if (spte->sw_addr == 0) {
		printf ("\n ===== START SWAP_READ =====\n");
		DUMP_FRAME_TABLE_ENTRY (fte);
		hex_dump (0, (const void *)kpage, PGSIZE, true);
		printf ("===== END SWAP_READ =====\n");
	}
#endif

	for (i= 0; i < SECTORS_PER_PAGE; i++) {
		bitmap_flip (sw_table, spte->sw_addr + i);
		ASSERT (bitmap_test (sw_table, spte->sw_addr + i) == false);
	}

	lock_release (&sw_lock);
}
/*----------------------------------------------------------------------------*/

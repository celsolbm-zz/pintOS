#include "vm/swaptable.h"
#include "userprog/pagedir.h"
#include "threads/thread.h"
#include "threads/malloc.h"

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
bool
init_swap_table (void)
{
	
	bool result;

	lock_init (&swap_lock);
	result = hash_init (&swap_table, swap_hash, swap_less, NULL);
	
	return result;
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
	struct swap_entry *new_swap = malloc (sizeof (struct swap_entry));

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
void 
load_frame (struct swap_entry *swap)
{
	unsigned long long sectors_written;
	uint32_t sector;
	struct thread *cur;
	void *kaddr;
	int loops, index;

	sectors_written = get_sectors_written (swap->swap_block);
	sector = (sectors_written == 0) ? 0 : (uint32_t)sectors_written;
	
	cur = thread_current ();
	kaddr = pagedir_get_page (cur->pagedir, swap->addr);
	
	loops = (swap->read_bytes + swap->zero_bytes) / SECTOR_SIZE;
	if (loops == 0) {
		block_write (swap->swap_block, sector, kaddr); 
		swap->swap_off = 0;
		return;
	}

	index = 0;
	swap->swap_off = sector;
	while (index < loops) {
		block_write (swap->swap_block, sector, kaddr); 
		kaddr += 0x200;
		sector++;
		index++;
	}
}
/******************************************************************************/
#if 0
void
read_frame (struct swap_entry *swap, void *paddr)
{
	int loops = (swap->read_bytes + swap->zero_bytes) / SECTOR_SIZE;
	int index = 0;
	int sector = swap->swap_off;

	while (index < loops) {
		block_read (swap->swap_block, sector, paddr);
		paddr += 0x200;
		sector++;
		index++;
	}
}
#endif

#include <hash.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include <stdio.h>
#include "threads/malloc.h"
#include "userprog/syscall.h"
#include "vm/frame.h"
#include "swaptable.h"
#include "devices/block.h"

#define sector_size 512

/******************************************************************************/
static inline unsigned
swap_hash (const struct hash_elem *po_, void *aux UNUSED)
{
  const struct swap_entry *po = hash_entry (po_,
																								struct swap_entry,
																								swap_elem);
  return hash_bytes (&po->addr, sizeof po->addr);
}
/******************************************************************************/
/* Returns true if swap a precedes swap b. */
static inline bool
swap_less (const struct hash_elem *sa_, const struct hash_elem *sb_,
           void *aux UNUSED)
{
  const struct swap_entry *sa = hash_entry (sa_,
																								struct swap_entry,
																								swap_elem);
  const struct swap_entry *sb = hash_entry (sb_,
																								struct swap_entry,
																								swap_elem);

  return sa->addr < sb->addr;
}
/******************************************************************************/

bool
init_swap_table (void)
{
	
	bool result = hash_init (&swap_table, swap_hash, swap_less, NULL);
	
	return result;
}
/******************************************************************************/
struct swap_entry *
swap_lookup (void *address)
{
	struct swap_entry pg;
	struct hash_elem *e;
	
	pg.addr = address;
	e = hash_find (&swap_table, &pg.swap_elem);

	return (e != NULL) ? (hash_entry (e, struct swap_entry, swap_elem)) :
											 (NULL);
}
/******************************************************************************/

void
save_swap (struct swap_entry *swap, void *address,
							 uint32_t r_bytes, uint32_t z_bytes, uint32_t file_p,
							 bool wr)
{

	swap->read_bytes = r_bytes;
	swap->zero_bytes = z_bytes;
	swap->addr = address;
	swap->writable = wr;
	swap->file_page = file_p;
	swap->swap_block=block_get_role(BLOCK_SWAP);
	hash_insert(&swap_table, &swap->swap_elem);
}
/******************************************************************************/

void 
load_frame (struct swap_entry *swap)
{
int sectors_written;
sectors_written=(int)get_sectors_written(swap->swap_block);
uint32_t sector;
if (sectors_written==0)
sector = 0;
else
sector =sectors_written;

void *kaddr=pagedir_get_page(thread_current()->pagedir,swap->addr);

int loops=(swap->read_bytes+swap->zero_bytes)/sector_size;
int index=0;
if (loops==0)
{block_write(swap->swap_block, sector,kaddr); 
swap->swap_off=0;
return;
}
swap->swap_off=sector;
while (index<loops)
{	block_write(swap->swap_block, sector,kaddr); 
	sector+=1;
	kaddr=kaddr+0x200;
	index=index+1;
}

}
/*
void read_frame(struct swap_entry *swap, void *paddr)
{

int loops=(swap->read_bytes+swap->zero_bytes)/sector_size;
int index=0;
int sector=swap->swap_off;
while (index<loops)
{
	block_read(swap->swap_block,sector,paddr);
	paddr=paddr+0x200;
	index=index+1;
	sector+=1;
}


}

*/

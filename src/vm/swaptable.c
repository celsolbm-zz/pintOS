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
	struct thread *cur = thread_current ();
	bool result = hash_init (&cur->swap_table, swap_hash, swap_less, NULL);
	
	return result;
}
/******************************************************************************/
struct swap_entry *
swap_lookup (void *address, struct hash hash_list)
{
	struct swap_entry pg;
	struct hash_elem *e;
	
	pg.addr = address;
	e = hash_find (&hash_list, &pg.swap_elem);

	return (e != NULL) ? (hash_entry (e, struct swap_entry, swap_elem)) :
											 (NULL);
}
/******************************************************************************/

void
save_swap (struct swap_entry *swap, void *address,
							 uint32_t r_bytes, uint32_t z_bytes, uint32_t file_p,
							 bool wr)
{
	struct thread *cur = thread_current();

	swap->read_bytes = r_bytes;
	swap->zero_bytes = z_bytes;
	swap->addr = address;
	swap->writable = wr;
	swap->file_page = file_p;
	
	hash_insert(&cur->swap_table, &swap->swap_elem);
}

#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
#include "suptable.h"



/******************************************************************************/
static inline unsigned
page_hash (const struct hash_elem *po_, void *aux UNUSED)
{
  const struct sup_page_entry *po = hash_entry (po_,
																								struct sup_page_entry,
																								page_elem);
  return hash_bytes (&po->addr, sizeof po->addr);
}
/******************************************************************************/
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

  return sa->addr < sb->addr;
}
/******************************************************************************/
bool
init_sup_table (void)
{
	struct thread *cur = thread_current ();
	bool result = hash_init (&cur->page_table, page_hash, page_less, NULL);
	
	return result;
}
/******************************************************************************/
struct sup_page_entry *
sup_lookup (void *address, struct hash hash_list)
{
	struct sup_page_entry pg;
	struct hash_elem *e;
	
	pg.addr = address;
	e = hash_find (&hash_list, &pg.page_elem);

	return (e != NULL) ? (hash_entry (e, struct sup_page_entry, page_elem)) :
											 (NULL);
}
/******************************************************************************/
void
save_sup_page (struct sup_page_entry *sup_page, void *address,
							 uint32_t r_bytes, uint32_t z_bytes, uint32_t file_p,
							 bool wr, int tp, struct file *file, void **esp,
							 void (**eip) (void), char **save_ptr, void *ptr)
{
	struct thread *cur = thread_current();

	sup_page->read_bytes = r_bytes;
	sup_page->zero_bytes = z_bytes;
	sup_page->type = tp;
	sup_page->addr = address;
	sup_page->writable = wr;
	sup_page->file_page = file_p;
	sup_page->arq = file;
	sup_page->esp1 = esp;
	sup_page->eip1 = eip;
	*sup_page->eip1 = ptr;
	sup_page->save_ptr1 = save_ptr;
	
	hash_insert(&cur->page_table, &sup_page->page_elem);
}




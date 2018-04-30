#include "suptable.h"
#include "threads/thread.h"
#include "threads/malloc.h"

struct lock sup_lock;

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
	bool result;

	lock_init (&sup_lock);
	result = hash_init (&cur->page_table, page_hash, page_less, NULL);
	
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
save_sup_page (void *address, uint32_t r_bytes, uint32_t z_bytes,
							 uint32_t file_p, bool wr, int tp, struct file *file, void **esp,
							 void (**eip) (void), char **save_ptr, void *ptr)
{
	struct thread *cur = thread_current();
	struct sup_page_entry *spe = malloc (sizeof(struct sup_page_entry));

	lock_acquire (&sup_lock);
	spe->read_bytes = r_bytes;
	spe->zero_bytes = z_bytes;
	spe->type = tp;
	spe->addr = address;
	spe->writable = wr;
	spe->file_page = file_p;
	spe->arq = file;
	spe->esp1 = esp;
	spe->eip1 = eip;
	spe->eip1 = ptr;
	spe->save_ptr1 = save_ptr;

	hash_insert(&cur->page_table, &spe->page_elem);
	lock_release (&sup_lock);
}

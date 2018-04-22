#include "suptable.h"
#include "threads/thread.h"




unsigned
page_hash (const struct hash_elem *po_, void *aux UNUSED)
{
  const struct sup_page_entry *po = hash_entry (po_, struct sup_page_entry, page_elem);
  return hash_bytes (&po->addr, sizeof po->addr);
}

/* Returns true if page a precedes page b. */
bool
page_less (const struct hash_elem *sa_, const struct hash_elem *sb_,
           void *aux UNUSED)
{
  const struct sup_page_entry *sa = hash_entry (sa_, struct sup_page_entry, page_elem);
  const struct sup_page_entry *sb = hash_entry (sb_, struct sup_page_entry, page_elem);

  return sa->addr < sb->addr;
}

bool init_sup_table(void)
{

//printf("\n \n fuck you im millwall \n \n");

struct thread *cur = thread_current();

bool result = hash_init(&cur->page_table,page_hash,page_less,NULL);

return result;
}



struct sup_page_entry *sup_lookup (const void *address,struct hash hash_list)
{
struct sup_page_entry pg;

struct hash_elem *e;

pg.addr = address;

e = hash_find (&hash_list,&pg.page_elem);
return e != NULL ? hash_entry (e, struct sup_page_entry, page_elem):NULL;
}









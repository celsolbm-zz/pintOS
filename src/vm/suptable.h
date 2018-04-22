#ifndef _SUPTABLE_H
#define _SUPTABLE_H

#include <hash.h>

struct sup_page_entry
{
    struct hash_elem page_elem;
    void *addr;
    int usls;
};

unsigned page_hash (const struct hash_elem *, void *aux );

bool page_less (const struct hash_elem *, const struct hash_elem *,void *aux );

bool init_sup_table (void);

struct sup_page_entry *sup_lookup (const void *,struct hash);
#endif
#ifndef _SWAPTABLE_H
#define _SWAPTABLE_H

#include <hash.h>
#include "filesys/file.h"
#include "devices/block.h"

struct swap_entry
{

struct hash_elem swap_elem;
struct block *swap_block ;
void *addr;
int usls; //debugging
bool alloced;
uint32_t swap_off;
uint32_t read_bytes;
uint32_t zero_bytes;
uint32_t file_page;
off_t file_oft;
bool writable;

};

struct lock swap_lock;
struct hash swap_table;
//bool swap_init=false;

bool init_swap_table (void);
struct swap_entry *swap_lookup (void *);
void save_swap (struct swap_entry *, void *, uint32_t,
										uint32_t, uint32_t, bool);

void load_frame ( struct swap_entry * ); 





#endif /* _SWAPTABLE */

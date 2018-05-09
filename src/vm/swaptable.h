#ifndef _SWAPTABLE_H
#define _SWAPTABLE_H

#include <bitmap.h>
#include "vm/suptable.h"
#include "vm/frame.h"

void init_swap_table (void);
void swap_out (struct frame_table_entry *);
void swap_read (struct sup_page_entry *, struct frame_table_entry *);
void invalidate_swap_table (size_t);

#endif /* _SWAPTABLE */

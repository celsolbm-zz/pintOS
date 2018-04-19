#ifndef _FRAME_H
#define _FRAME_H

#include <hash.h>

struct frame_table_entry {
	void *paddr;									/* Physical address of this frame */
	void *vpage;									/* Associated virtual page */
	struct hash_elem frame_elem;	/* Hash table element */
	/* XXX Need to add LRU related members */
};

bool init_frame_table(void);

#endif /* _FRAME_H */

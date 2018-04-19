#include "frame.h"
#include "threads/thread.h"

/******************************************************************************/
static inline unsigned
frame_hash (const struct hash_elem *he, void *aux)
{
	const struct frame_table_entry *fte = hash_entry (he,
																									  struct frame_table_entry,
																									  frame_elem);
	return hash_bytes (&fte->addr, sizeof(fte->addr));
}
/******************************************************************************/
static inline bool
frame_less (const struct hash_elem *ha, const struct hash_elem *hb,
						void *aux UNUSED)
{
	const struct frame_table_entry *ftea = hash_entry (ha,
																										 struct frame_table_entry,
																										 frame_elem);
	const struct frame_table_entry *fteb = hash_entry (hb,
																										 struct frame_table_entry,
																										 frame_elem);
	return (ftea->addr < fteb->addr);
}
/******************************************************************************/
/* Initialize frame table */
bool
init_frame_table (void)
{
	struct thread *cur = thread_current ();
	bool ret;

	ret = hash_init (&cur->frame_table, frame_hash, frame_less, NULL);

	return ret;
}
/******************************************************************************/

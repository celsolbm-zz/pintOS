#include "filesys/inode.h"
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"

#include "threads/synch.h"
#include <stdio.h>

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

#define PRINT_INODE_DISK(disk_inode)																			\
	do {																																		\
		printf ("length: %d\n", (disk_inode).length);													\
		printf ("is_dir: %s\n", ((disk_inode).is_dir == 1) ? "TRUE": "FALSE");\
		printf ("parent_dir: %u\n", (disk_inode).parent_dir);									\
		printf ("d_indirect_sector: %u\n", (disk_inode).d_indirect_sector);		\
		printf ("sector_cnt: %u\n", (disk_inode).sector_cnt);									\
		printf ("\n");																												\
	} while (0)

#define INDEXED_FILE
//#define DEBUG_INDEXED_FILE

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    off_t length;											/* File size in bytes. */
    unsigned magic;										/* Magic number. */
		uint32_t is_dir;									/* Is this inode for directory? */
		block_sector_t parent_dir;				/* Parent dir's sector for dir inode */
		block_sector_t d_indirect_sector;	/* Sector # of double indirect ptr */
		uint32_t sector_cnt;							/* # of data sectors for this inode */
    uint32_t unused[122];							/* Not used. */
  };

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;			/* Element in inode list. */
    block_sector_t sector;			/* Sector number of disk location. */
    int open_cnt;								/* Number of openers. */
    bool removed;								/* True if deleted, false otherwise. */
    int deny_write_cnt;					/* 0: writes ok, >0: deny writes. */
		struct lock inode_lock;			/* Inode lock for synchronization */
    struct inode_disk data;			/* Inode content. */
  };

/* One indirect inode sector */
struct indirect_sector {
	block_sector_t inode[128];
};

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/*----------------------------------------------------------------------------*/
/* static function for inode																									*/
/*----------------------------------------------------------------------------*/
/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}
/*----------------------------------------------------------------------------*/
/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);

#ifdef INDEXED_FILE
	uint32_t sector_num = pos / BLOCK_SECTOR_SIZE;
	uint32_t indirect_num = sector_num / 128;
	uint32_t indirect_ofs = sector_num % 128;
	struct indirect_sector d_indirect;
	struct indirect_sector indirect;

	if (pos >= inode->data.length)
		return -1;

	/* Read double, single indirect sector from disk */
	block_read (fs_device, inode->data.d_indirect_sector, (void *)&d_indirect);
	block_read (fs_device, d_indirect.inode[indirect_num], (void *)&indirect);

	return indirect.inode[indirect_ofs];
#else
  if (pos < inode->data.length)
    return inode->data.start + pos / BLOCK_SECTOR_SIZE;
  else
    return -1;
#endif
}
/*----------------------------------------------------------------------------*/
/* Destroy inode by traversing indirect inode pointer */
static void
inode_destroy (struct inode *inode)
{
	struct indirect_sector *d_indirect, *indirect;
	uint32_t sector_to_destroy, indirect_sector;
	uint32_t d_idx; /* for double indirect inode */
	uint32_t i_idx;	/* for single indirect inode */

	free_map_release (inode->sector, 1);
	d_indirect = malloc (sizeof(struct indirect_sector));
	indirect = malloc (sizeof(struct indirect_sector));
	
	block_read (fs_device, inode->data.d_indirect_sector, (void *)d_indirect);
	sector_to_destroy = inode->data.sector_cnt;
	d_idx = 0;

#ifdef DEBUG_INDEXED_FILE
	printf ("=== <inode_desroy> ===\n");
	PRINT_INODE_DISK (inode->data);
#endif

	while (sector_to_destroy > 0) {
		indirect_sector = (sector_to_destroy >= 128) ?
											128 : sector_to_destroy;
		block_read (fs_device, d_indirect->inode[d_idx], (void *)indirect);
		/* Destroy allocated direct inode */
		for (i_idx = 0; i_idx < indirect_sector; i_idx++) {
#ifdef DEBUG_INDEXED_FILE
			printf ("<inode_destroy> check 1\n");
			printf ("<inode_destroy> indirect->inode[%u]: %u\n", i_idx, indirect->inode[i_idx]);
#endif
			free_map_release (indirect->inode[i_idx], 1);
		}

		/* Destroy allocated indirect inode */
#ifdef DEBUG_INDEXED_FILE
		printf ("<inode_destroy> check 2\n");
#endif
		free_map_release (d_indirect->inode[d_idx], 1);

		/* Advance */
		sector_to_destroy -= indirect_sector;
		d_idx++;
	}

	/* Destroy allocated double indirect sector */
#ifdef DEBUG_INDEXED_FILE
	printf ("<inode_destroy> check 3\n");
#endif
	free_map_release (inode->data.d_indirect_sector, 1);

	free (d_indirect);
	free (indirect);
}
/*----------------------------------------------------------------------------*/
static void
inode_extend (struct inode *inode, off_t length)
{
	struct inode_disk *disk_inode = NULL;
	size_t origin_sector_cnt;
	size_t extend_sector_cnt;
	size_t current_sector;
	size_t indirect_idx, indirect_ofs, indirect_left;
	size_t make_sectors, a, i;
	struct indirect_sector d_indirect, indirect;

	ASSERT (length >= 0);

	disk_inode = &inode->data;
	if (disk_inode != NULL) {
		ASSERT (disk_inode->magic == INODE_MAGIC);
		origin_sector_cnt = disk_inode->sector_cnt;
		disk_inode->length += length;

		extend_sector_cnt = bytes_to_sectors (disk_inode->length) -
												origin_sector_cnt;
		disk_inode->sector_cnt += extend_sector_cnt;
		block_read (fs_device, disk_inode->d_indirect_sector, (void *)&d_indirect);

		current_sector = origin_sector_cnt;
		while (extend_sector_cnt > 0) {
			indirect_idx = current_sector	/ 128;
			indirect_ofs = current_sector % 128;
			if (indirect_ofs == 0) {
				free_map_allocate (1, &d_indirect.inode[indirect_idx]);
				memset (&indirect, 0, sizeof(struct indirect_sector));
			}
			else {
				block_read (fs_device, d_indirect.inode[indirect_idx],
										(void *)&indirect);
			}

			indirect_left = 128 - indirect_ofs;
			a = (extend_sector_cnt >= 128) ? 128 : extend_sector_cnt;
			make_sectors = (a > indirect_left) ? indirect_left : a;
			for (i = 0; i < make_sectors; i++) {
				free_map_allocate (1, &indirect.inode[indirect_ofs]);
				indirect_ofs++;
			}

			current_sector += make_sectors;
			extend_sector_cnt -= make_sectors;
			block_write (fs_device, d_indirect.inode[indirect_idx],
									 (void *)&indirect);
		}

		block_write (fs_device, disk_inode->d_indirect_sector,
								 (void *)&d_indirect);
	}
}
/*----------------------------------------------------------------------------*/
/* end of static function for inode																						*/
/*----------------------------------------------------------------------------*/
/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}
/*----------------------------------------------------------------------------*/
/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length, bool is_dir)
{
  struct inode_disk *disk_inode = NULL;
	static char zeros[BLOCK_SECTOR_SIZE];
#ifdef INDEXED_FILE
	int counter;
	size_t sectors;
	size_t i, data_sectors;	/* # of data region sectors for this new inode */
	struct indirect_sector *d_indirect, *indirect;
#endif
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
#ifndef INDEXED_FILE
  if (disk_inode != NULL)
    {
      sectors = bytes_to_sectors (length);
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
			disk_inode->is_dir = is_dir;
      if (free_map_allocate (sectors, &disk_inode->start)) 
        {
          block_write (fs_device, sector, disk_inode);
          if (sectors > 0) 
            {
              size_t i;
              
              for (i = 0; i < sectors; i++) 
                block_write (fs_device, disk_inode->start + i, zeros);
            }
          success = true; 
        } 
      free (disk_inode);
    }
#else
	if (disk_inode != NULL) {
		sectors = bytes_to_sectors (length);
		disk_inode->length = length;
		disk_inode->magic = INODE_MAGIC;
		disk_inode->is_dir = is_dir;
		disk_inode->sector_cnt = sectors;

		/* Allocate double indirect inode sector */
		if (free_map_allocate (1, &disk_inode->d_indirect_sector)) {
			counter = 0;
			d_indirect = malloc (sizeof(struct indirect_sector));
			if (d_indirect == NULL) {
				free_map_release (disk_inode->d_indirect_sector, 1);
				free (disk_inode);
				goto done;
			}
#ifdef DEBUG_INDEXED_FILE
			printf ("<inode_create> d_indirect: %u\n", disk_inode->d_indirect_sector);
#endif

			while (sectors > 0) {
				data_sectors = (sectors >= 128) ? 128 : sectors;

				/* Allocate indirect inode sector */
				if (free_map_allocate (1, &d_indirect->inode[counter])) {
					indirect = malloc (sizeof(struct indirect_sector));
					if (indirect == NULL) {
						disk_inode->sector_cnt -= sectors;
						//free_map_release (d_indirect->inode[counter], 1);
						free (d_indirect);
						free (disk_inode);
						goto done;
					}
#ifdef DEBUG_INDEXED_FILE
					printf ("<inode_create> counter: %d, indirect: %u\n",
									counter, d_indirect->inode[counter]);
#endif

					for (i = 0; i < data_sectors; i++) {
						/* Allocate direct inode sector */
						if (!free_map_allocate (1, &indirect->inode[i]))
							PANIC ("<inode_create> fail to allocate direct inode");

#ifdef DEBUG_INDEXED_FILE
						printf ("<inode_create> data_sectors: %zu\n", data_sectors);
						printf ("<inode_create> indirect->inode[%zu]: %u\n",
										i, indirect->inode[i]);
#endif
						/* Write zero data in allocated direct inode sector */
						block_write (fs_device, indirect->inode[i], zeros);
					}

					/* Write sector for indirect inode */
					block_write (fs_device, d_indirect->inode[counter],
											 (void *)indirect);
					free (indirect);
				}
				else {
					PANIC ("<inode_create> fail to allocate indirect inode");
				}

				counter++;
				sectors -= data_sectors;
			}

			/* Write sector for double indirect inode */
			block_write (fs_device, disk_inode->d_indirect_sector,
									 (void *)d_indirect);
			free (d_indirect);
		}
	}

	block_write (fs_device, sector, (void *)disk_inode);
	free (disk_inode);
	success = true;
#endif

done:
  return success;
}
/*----------------------------------------------------------------------------*/
/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
	
	/* Intialize inode lock */
	lock_init (&inode->inode_lock);

  block_read (fs_device, inode->sector, &inode->data);

#ifdef DEBUG_INDEXED_FILE
	printf ("=== <inode_open> ===\n");
	PRINT_INODE_DISK (inode->data);
#endif
  return inode;
}
/*----------------------------------------------------------------------------*/
/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}
/*----------------------------------------------------------------------------*/
/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}
/*----------------------------------------------------------------------------*/
/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;
	
  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) {
#ifdef INDEXED_FILE
				/* Actually destroy inode from pintos */
				inode_destroy (inode);
#else
				free_map_release (inode->sector, 1);
				free_map_release (inode->data.start,
													bytes_to_sectors (inode->data.length)); 
#endif
      }

      free (inode); 
    }

  	if (lock_held_by_current_thread (&inode->inode_lock))
		  inode_lock_release (inode);
}
/*----------------------------------------------------------------------------*/
/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
#ifdef DEBUG_INDEXED_FILE
	printf ("=== <inode_remove> ===\n");
	PRINT_INODE_DISK (inode->data);
#endif
}
/*----------------------------------------------------------------------------*/
/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Read full sector directly into caller's buffer. */
          block_read (fs_device, sector_idx, buffer + bytes_read);
        }
      else 
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          block_read (fs_device, sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);

  return bytes_read;
}
/*----------------------------------------------------------------------------*/
/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

#ifdef INDEXED_FILE
	inode_lock_acquire (inode);

	while (size > 0) {
		/* Sector to write, starting byte offset within sector. */
		int sector_ofs = offset % BLOCK_SECTOR_SIZE;

		/* Bytes left in inode, bytes left in sector, lesser of the two. */
		off_t inode_left = inode_length (inode) - offset;
		int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
		int sector_write_bytes = (size >= sector_left) ? sector_left : size;

		if (inode_left <= 0) {
			inode_lock_release (inode);
			inode_extend (inode, offset - inode_length (inode) + sector_write_bytes);
			inode_lock_acquire (inode);
		}

		block_sector_t sector_idx = byte_to_sector (inode, offset);

		if ((sector_ofs == 0) && (sector_write_bytes == BLOCK_SECTOR_SIZE)) {
			/* Write full sector */
			block_write (fs_device, sector_idx, buffer + bytes_written);
		}
		else {
			/* We need a bounce buffer */
			if (bounce == NULL) {
				bounce = malloc (BLOCK_SECTOR_SIZE);
				if (bounce == NULL)
					break;
			}

			/* If the sector contains data before or after the chunk
				 we're writing, then we need to read in the sector
				 first.  Otherwise we start with a sector of all zeros. */
			if (sector_ofs > 0 || sector_write_bytes < sector_left) 
				block_read (fs_device, sector_idx, bounce);
			else
				memset (bounce, 0, BLOCK_SECTOR_SIZE);
			memcpy (bounce + sector_ofs, buffer + bytes_written, sector_write_bytes);
			block_write (fs_device, sector_idx, bounce);
		}

		/* Advance. */
		size -= sector_write_bytes;
		offset += sector_write_bytes;
		bytes_written += sector_write_bytes;
	}

	inode_lock_release (inode);
#else
  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
          block_write (fs_device, sector_idx, buffer + bytes_written);
        }
      else 
        {
          /* We need a bounce buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left) 
            block_read (fs_device, sector_idx, bounce);
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          block_write (fs_device, sector_idx, bounce);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);
#endif

  return bytes_written;
}
/*----------------------------------------------------------------------------*/
/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}
/*----------------------------------------------------------------------------*/
/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}
/*----------------------------------------------------------------------------*/
/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}
/*----------------------------------------------------------------------------*/
/* Added functions																														*/
/*----------------------------------------------------------------------------*/
/* Acquire lock of inode */
void
inode_lock_acquire (struct inode *inode)
{
	lock_acquire (&inode->inode_lock);
}
/*----------------------------------------------------------------------------*/
/* Acquire lock of inode */
void
inode_lock_release (struct inode *inode)
{
	lock_release (&inode->inode_lock);
}
/*----------------------------------------------------------------------------*/
/* Set parent directory of child directory in inode */
void
inode_set_parent_dir (struct inode *child_dir, struct inode *parent_dir)
{
	child_dir->data.parent_dir = parent_dir->sector;
}
/*----------------------------------------------------------------------------*/
/* Check whether inode is for directory  */
uint32_t
inode_is_dir (struct inode *inode)
{
	return inode->data.is_dir;
}
/*----------------------------------------------------------------------------*/
/* Return inode is removed or not */
bool
inode_is_removed (struct inode *inode)
{
	return inode->removed;
}
/*----------------------------------------------------------------------------*/
/* Get parent of inode INODE sector number */
block_sector_t
inode_get_parent (struct inode *inode)
{
	return inode->data.parent_dir;
}
/*----------------------------------------------------------------------------*/

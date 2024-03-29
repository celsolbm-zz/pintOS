#include "filesys/directory.h"
#include <stdio.h>
#include <string.h>
#include <list.h>
#include "filesys/filesys.h"
#include "threads/malloc.h"

#include "filesys/inode.h"
#include "threads/thread.h"

//#define DEBUG_DIRECTORY

/* A directory. */
struct dir 
  {
    struct inode *inode;                /* Backing store. */
    off_t pos;                          /* Current position. */
  };

/* A single directory entry. */
struct dir_entry 
  {
		block_sector_t inode_sector;	/* Sector number of header. */
    char name[NAME_MAX + 1];			/* Null terminated file name. */
    bool in_use;									/* In use or free? */
  };

#define PRINT_DIR_ENTRY(e)																						\
	do {																																\
		printf ("inode_sector: %u\n", (e).inode_sector);									\
		printf ("name: %s\n", (strlen ((e).name) != 0) ? (e).name : "");	\
		printf ("in_use: %s\n", ((e).in_use == true) ? "TRUE" : "FALSE");	\
		printf ("\n");																										\
	}	while (0)																													\

static bool dir_is_empty (struct inode *);

/*----------------------------------------------------------------------------*/
/* Creates a directory with space for ENTRY_CNT entries in the
   given SECTOR.  Returns true if successful, false on failure. */
bool
dir_create (block_sector_t sector, size_t entry_cnt)
{
	struct thread *cur;
	struct inode *new_dir_inode;

	if (!inode_create (sector, entry_cnt * sizeof(struct dir_entry), true))
		return false;

	/* Current thread's working directory becomes new directory's parent */
	new_dir_inode = inode_open (sector);
	cur = thread_current ();
	if (cur->curdir != NULL)
		inode_set_parent_dir (new_dir_inode, cur->curdir->inode);

	inode_close (new_dir_inode);

  return true;
}
/*----------------------------------------------------------------------------*/
/* Opens and returns the directory for the given INODE, of which
   it takes ownership.  Returns a null pointer on failure. */
struct dir *
dir_open (struct inode *inode) 
{
  struct dir *dir = calloc (1, sizeof *dir);
  if (inode != NULL && dir != NULL)
    {
      dir->inode = inode;
      dir->pos = 0;
      return dir;
    }
  else
    {
      inode_close (inode);
      free (dir);
      return NULL; 
    }
}
/*----------------------------------------------------------------------------*/
/* Opens the root directory and returns a directory for it.
   Return true if successful, false on failure. */
struct dir *
dir_open_root (void)
{
  return dir_open (inode_open (ROOT_DIR_SECTOR));
}
/*----------------------------------------------------------------------------*/
/* Opens and returns a new directory for the same inode as DIR.
   Returns a null pointer on failure. */
struct dir *
dir_reopen (struct dir *dir) 
{
  return dir_open (inode_reopen (dir->inode));
}
/*----------------------------------------------------------------------------*/
/* Destroys DIR and frees associated resources. */
void
dir_close (struct dir *dir) 
{
  if (dir != NULL)
    {
      inode_close (dir->inode);
      free (dir);
    }
}
/*----------------------------------------------------------------------------*/
/* Returns the inode encapsulated by DIR. */
struct inode *
dir_get_inode (struct dir *dir) 
{
  return dir->inode;
}
/*----------------------------------------------------------------------------*/
/* Searches DIR for a file with the given NAME.
   If successful, returns true, sets *EP to the directory entry
   if EP is non-null, and sets *OFSP to the byte offset of the
   directory entry if OFSP is non-null.
   otherwise, returns false and ignores EP and OFSP. */
static bool
lookup (const struct dir *dir, const char *name,
        struct dir_entry *ep, off_t *ofsp) 
{
  struct dir_entry e;
  size_t ofs;
  
  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e) 
    if (e.in_use && !strcmp (name, e.name)) 
      {
        if (ep != NULL)
          *ep = e;
        if (ofsp != NULL)
          *ofsp = ofs;
        return true;
      }

  return false;
}
/*----------------------------------------------------------------------------*/
/* Searches DIR for a file with the given NAME
   and returns true if one exists, false otherwise.
   On success, sets *INODE to an inode for the file, otherwise to
   a null pointer.  The caller must close *INODE. */
bool
dir_lookup (const struct dir *dir, const char *name,
            struct inode **inode) 
{
  struct dir_entry e;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

	if (!strcmp (name, ".")) {
		/* Reopen current directory DIR if not removed */
		if (inode_is_removed (dir->inode))
			return NULL;

		*inode = inode_reopen (dir->inode);
	}
	else if (!strcmp (name, "..")) {
		/* Open parent directory of DIR if DIR is not removed */
		if (inode_is_removed (dir->inode))
			return NULL;

		*inode = inode_open (inode_get_parent (dir->inode));
	}
	else if (lookup (dir, (const char *)name, &e, NULL)) {
		*inode = inode_open (e.inode_sector);
	}
	else {
		*inode = NULL;
	}

  return *inode != NULL;
}
/*----------------------------------------------------------------------------*/
/* Adds a file named NAME to DIR, which must not already contain a
   file by that name.  The file's inode is in sector
   INODE_SECTOR.
   Returns true if successful, false on failure.
   Fails if NAME is invalid (i.e. too long) or a disk or memory
   error occurs. */
bool
dir_add (struct dir *dir, const char *name, block_sector_t inode_sector)
{
  struct dir_entry e;
  off_t ofs;
  bool success = false;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Check NAME for validity. */
  if (*name == '\0' || strlen (name) > NAME_MAX)
    return false;

  /* Check that NAME is not in use. */
  if (lookup (dir, name, NULL, NULL))
    goto done;

  /* Set OFS to offset of free slot.
     If there are no free slots, then it will be set to the
     current end-of-file.
     
     inode_read_at() will only return a short read at end of file.
     Otherwise, we'd need to verify that we didn't get a short
     read due to something intermittent such as low memory. */
  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e) 
    if (!e.in_use)
      break;

  /* Write slot.
	   inode structure is updated. */
	inode_lock_acquire (dir_get_inode (dir));
  e.in_use = true;
  strlcpy (e.name, name, sizeof e.name);
  e.inode_sector = inode_sector;
	inode_lock_release (dir_get_inode (dir));

  success = inode_write_at (dir->inode, &e, sizeof e, ofs) == sizeof e;

 done:
  return success;
}
/*----------------------------------------------------------------------------*/
/* Removes any entry for NAME in DIR.
   Returns true if successful, false on failure,
   which occurs only if there is no file with the given NAME. */
bool
dir_remove (struct dir *dir, const char *name) 
{
  struct dir_entry e;
  struct inode *inode = NULL;
  bool success = false;
  off_t ofs;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Find directory entry. */
  if (!lookup (dir, name, &e, &ofs))
    goto done;

  /* Open inode. */
  inode = inode_open (e.inode_sector);
  if (inode == NULL)
    goto done;

	if (inode_is_dir (inode))
		if(!dir_is_empty (inode))
			goto done;

  /* Erase directory entry. */
  e.in_use = false;
  if (inode_write_at (dir->inode, &e, sizeof e, ofs) != sizeof e)  {
		inode_lock_acquire (dir_get_inode (dir));
    goto done;
	}

  /* Remove inode. */
  inode_remove (inode);
  success = true;

 done:
  inode_close (inode);
  return success;
}
/*----------------------------------------------------------------------------*/
/* Reads the next directory entry in DIR and stores the name in
   NAME.  Returns true if successful, false if the directory
   contains no more entries. */
bool
dir_readdir (struct dir *dir, char name[NAME_MAX + 1])
{
  struct dir_entry e;

  while (inode_read_at (dir->inode, &e, sizeof e, dir->pos) == sizeof e) 
    {
			inode_lock_acquire (dir_get_inode (dir));
      dir->pos += sizeof e;
      if (e.in_use)
        {
          strlcpy (name, e.name, NAME_MAX + 1);
					inode_lock_release (dir_get_inode (dir));
          return true;
        } 
			inode_lock_release (dir_get_inode (dir));
    }
  return false;
}
/*----------------------------------------------------------------------------*/
/* Added functions																														*/
/*----------------------------------------------------------------------------*/
/* Parse directory name and return final directory */
struct dir *
parse_dir_name (const char *_path)
{
	char path[strlen (_path) + 1];
	char *cur_name, *next_name, *save_ptr;	/* for path parsing */
	struct thread *cur;
	struct dir *ret_dir;
	struct inode *inode;

	cur = thread_current ();

	/* Copy path name and check whether path is absolute */
	memcpy (path, _path, strlen (_path) + 1);
	if ((path[0] == '/') || (cur->curdir == NULL))
		ret_dir = dir_open_root ();
	else
		ret_dir = dir_reopen (cur->curdir);

	/* Parse passed path name */
#ifdef DEBUG_DIRECTORY
	printf ("<parse_dir_name> parsing start, path[0]: %c\n", path[0]);
#endif
	cur_name = strtok_r (path, "/", &save_ptr);
	for (next_name = strtok_r (NULL, "/", &save_ptr);
			 next_name != NULL;
			 next_name = strtok_r (NULL, "/", &save_ptr)) {
#ifdef DEBUG_DIRECTORY
		printf ("<parse_dir_name> In while statement, ");
		printf ("<parse_dir_name> cur_name: %s, next_name: %s\n",
						cur_name, next_name);
#endif

		if (strcmp (cur_name, ".") == 0) {
			/* Read from current working directory */
			if (inode_is_removed (ret_dir->inode))
				return NULL;
		}
		else if (strcmp (cur_name, "..") == 0) {
			/* Read from current working directory's parent directory */
			if (inode_is_removed (ret_dir->inode))
				return NULL;

			/* Open current directory's parent directory */
			inode_open (inode_get_parent (ret_dir->inode));
			dir_close (ret_dir);
			ret_dir = dir_open (inode);
		}
		else {
			/* Whether cur_name is in the current directory */
			if (!dir_lookup (ret_dir, cur_name, &inode)) {
#ifdef DEBUG_DIRECTORY
				printf ("<parse_dir_name> Can't find %s in directory\n", cur_name);
#endif
				dir_close (ret_dir);
				return NULL;
			}

			/* Whether cur_name indicates file */
			if (inode_is_dir (inode) == 0) {
				dir_close (ret_dir);
				return NULL;
			}

#ifdef DEBUG_DIRECTORY
			printf ("<parse_dir_name> Move into directory %s\n", cur_name);
#endif
			dir_close (ret_dir);
			ret_dir = dir_open (inode);
		}

		cur_name = next_name;
	}

	return ret_dir;
}
/*----------------------------------------------------------------------------*/
/* Parse path name and return last destination name.
OBSOLUTE: Return value is malloced value, so caller must free that value */
char *
get_target_name (const char *_path)
{
	char *path, *target;

	path = (char *)_path;
	target = path;
	while (*path != '\0') {
		if (*path == '/')
			target = path + 1;

		path++;
	}

	return target;
}
/*----------------------------------------------------------------------------*/
/* Check whether directory is empty */
static bool
dir_is_empty (struct inode *inode)
{
	struct dir_entry e;
	off_t ofs;

	for (ofs = 0; inode_read_at (inode, &e, sizeof(e), ofs) == sizeof(e);
			 ofs += sizeof(e))  {
		if (e.in_use == true)
			return false;
	}

	return true;
}
/*----------------------------------------------------------------------------*/

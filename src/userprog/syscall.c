#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

#include "devices/shutdown.h"
#include "devices/input.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "userprog/pagedir.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include <string.h>

#ifdef FILESYS
#include "filesys/directory.h"
#include "filesys/inode.h"
#include "filesys/free-map.h"

#define INITIAL_FILE_NUM	16
#define INVALID_SECTOR		0xFFFFFFFF
#endif

#ifdef VM
#include "vm/suptable.h"
#endif

#define MAX_ARG 3 /* Maximum number of system call arguments */

static void syscall_handler (struct intr_frame *);
static int child_number = 0;

/*
 * Helper functions
 */
static void check_user_ptr (const void *uptr, void *esp);
static void check_user_string (const char *ustr, void *esp);
static void check_user_buffer (const void *uptr, unsigned size, void *esp);
static void get_sysarg (struct intr_frame *f, int *sysarg, int arg_num);

/*----------------------------------------------------------------------------*/
void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init (&filesys_lock);
}
/*----------------------------------------------------------------------------*/
static void
syscall_handler (struct intr_frame *f) 
{
  int sysno;
  int sysarg[MAX_ARG];


  check_user_ptr ((const void *)f->esp, f->esp);
	sysno = *(int *)f->esp;
  switch (sysno) {
    case SYS_HALT:
      halt ();
      break;
      
    case SYS_EXIT:
      get_sysarg (f, &sysarg[0], 1);
      exit (sysarg[0]);
      break;

    case SYS_EXEC:
      get_sysarg (f, &sysarg[0], 1);
			check_user_string ((const char *)sysarg[0], f->esp);
      f->eax = exec ((const char *)sysarg[0]);
      break;

    case SYS_WAIT:
      get_sysarg (f, &sysarg[0], 1);
      f->eax = wait ((pid_t)sysarg[0]);
      break;

    case SYS_CREATE:
      get_sysarg (f, &sysarg[0], 2);
			check_user_string ((const char *)sysarg[0], f->esp);
      f->eax = create ((const char *)sysarg[0], (unsigned)sysarg[1]);
      break;

    case SYS_REMOVE:
      get_sysarg (f, &sysarg[0], 1);
			check_user_string ((const char *)sysarg[0], f->esp);
      f->eax = remove ((const char *)sysarg[0]);
      break;

    case SYS_OPEN:
      get_sysarg (f, &sysarg[0], 1);
			check_user_string ((const char *)sysarg[0], f->esp);
      f->eax = open ((const char *)sysarg[0]);
      break;

    case SYS_FILESIZE:
      get_sysarg (f, &sysarg[0], 1);
      f->eax = filesize (sysarg[0]);
      break;

    case SYS_READ:
      get_sysarg (f, &sysarg[0], 3);
      check_user_buffer ((const void *)sysarg[1], (unsigned)sysarg[2], f->esp);
      f->eax = read (sysarg[0], (void *)sysarg[1], (unsigned)sysarg[2]);
      break;

    case SYS_WRITE:
      get_sysarg (f, &sysarg[0], 3);
      check_user_buffer ((const void *)sysarg[1], (unsigned)sysarg[2], f->esp);
      f->eax = write (sysarg[0], (const void *)sysarg[1], (unsigned)sysarg[2]);
      break;

    case SYS_SEEK:
      get_sysarg (f, &sysarg[0], 2);
      seek (sysarg[0], (unsigned)sysarg[1]);
      break;

    case SYS_TELL:
      get_sysarg (f, &sysarg[0], 1);
      f->eax = tell (sysarg[0]);
      break;

    case SYS_CLOSE:
      get_sysarg (f, &sysarg[0], 1);
      close (sysarg[0]);
      break;

#ifdef FILESYS
		case SYS_CHDIR:
      get_sysarg (f, &sysarg[0], 1);
			check_user_string ((const char *)sysarg[0], f->esp);
			f->eax = chdir ((const char *)sysarg[0]);
			break;

		case SYS_MKDIR:
      get_sysarg (f, &sysarg[0], 1);
			check_user_string ((const char *)sysarg[0], f->esp);
			f->eax = mkdir ((const char *)sysarg[0]);
			break;

		case SYS_READDIR:
      get_sysarg (f, &sysarg[0], 2);
			check_user_string ((const char *)sysarg[1], f->esp);
			f->eax = readdir (sysarg[0], (char *)sysarg[1]);
			break;

		case SYS_ISDIR:
      get_sysarg (f, &sysarg[0], 1);
      f->eax = isdir (sysarg[0]);
			break;

		case SYS_INUMBER:
      get_sysarg (f, &sysarg[0], 1);
      f->eax = inumber (sysarg[0]);
			break;
#endif
  }
}
/*----------------------------------------------------------------------------*/
/*
 * System call
 */
/*----------------------------------------------------------------------------*/
void
halt (void)
{
  shutdown_power_off ();
}
/*----------------------------------------------------------------------------*/
void
exit (int status)
{
  struct thread *cur = thread_current ();
  if (check_process_alive (cur->parent_pid) && cur->chinfo_by_parent)
    cur->chinfo_by_parent->exit_code = status;

  child_number--;
  printf ("%s: exit(%d)\n", cur->name, status);
  thread_exit ();
}
/*----------------------------------------------------------------------------*/
pid_t
exec (const char *cmd_line)
{
  pid_t child_pid;
  struct child_info *chinfo;

  if (child_number > 30)
    return PID_ERROR;

  child_pid = (pid_t)process_execute (cmd_line);
  chinfo = get_child_info (child_pid);
  if (chinfo == NULL)
    return PID_ERROR;

  if (chinfo->load_status == LOAD_NOT_BEGIN)
    sema_down (&chinfo->exec_sema);

  if (chinfo->load_status == LOAD_FAIL) {
    remove_child_info (chinfo);
    return PID_ERROR;
  }

  child_number++;    

  return child_pid;
}
/*----------------------------------------------------------------------------*/
int
wait (pid_t pid)
{
  return process_wait (pid);
}
/*----------------------------------------------------------------------------*/
bool
create (const char *file, unsigned initial_size)
{
  bool ret;

	if (file == NULL)
		exit (-1);

  ret = filesys_create (file, (off_t)initial_size);

  return ret;
}
/*----------------------------------------------------------------------------*/
bool
remove (const char *file)
{
  bool ret;

	if (!lock_held_by_current_thread (&filesys_lock))
		lock_acquire (&filesys_lock);
  ret = filesys_remove (file);
  lock_release (&filesys_lock);

  return ret;
}
/*----------------------------------------------------------------------------*/
int
open (const char *file)
{
  struct thread *cur = thread_current ();
  struct file_info *finfo;
  struct file *temp_file;
	struct inode *inode;

  temp_file = filesys_open (file);
  if (temp_file == NULL)
		return -1;

  finfo = malloc (sizeof(struct file_info));
  if (finfo == NULL) {
    lock_release (&filesys_lock);
    return -1;
  }
  finfo->fd = cur->min_fd;
  cur->min_fd++;
  finfo->file = temp_file;
	
	inode = file_get_inode (temp_file);
	finfo->dir = inode_is_dir (inode) ? dir_open (inode) : NULL;

  list_push_back (&cur->open_file, &finfo->file_elem);

  return finfo->fd;
}
/*----------------------------------------------------------------------------*/
int
filesize (int fd)
{
  int ret;
  struct file_info *finfo;

	if (!lock_held_by_current_thread (&filesys_lock))
		lock_acquire (&filesys_lock);

  finfo = get_file_info (fd);
  if (finfo == NULL) {
    lock_release (&filesys_lock);
    return -1;
  }
  ret = (int)file_length (finfo->file);
  lock_release (&filesys_lock);

  return ret;
}
/*----------------------------------------------------------------------------*/
int
read (int fd, void *buffer, unsigned size)
{
	void *kbuf;
  int ret;
  struct file_info *finfo;

	if (size == 0)
		return 0;

  if (fd == STDIN_FILENO) {
    char *bufptr = (char *)buffer;
    unsigned i;

    for (i = 0; i < size; i++) {
      *bufptr = input_getc ();
      bufptr++;
    }
    return (int)size;
  }

  if (!lock_held_by_current_thread (&filesys_lock))
		lock_acquire (&filesys_lock);
  
	finfo = get_file_info (fd);
  if (finfo == NULL) {
    lock_release (&filesys_lock);
    return -1;
  }

	kbuf = malloc (size);
	if (kbuf == NULL) {
    lock_release (&filesys_lock);
    return -1;
	}

  ret = file_read (finfo->file, kbuf, size);//kbuf goes where buffer is
	memcpy (buffer, kbuf, ret);

  lock_release (&filesys_lock);
	free (kbuf);

  return ret;
}
/*----------------------------------------------------------------------------*/
int
write (int fd, const void *buffer, unsigned size)
{
  int ret;
  struct file_info *finfo;
	void *kbuf;

	if (size == 0)
		return 0;

  if (fd == STDOUT_FILENO) {
    putbuf (buffer, (size_t)size);
    return (int)size;
  }

	if (!lock_held_by_current_thread (&filesys_lock))
		lock_acquire (&filesys_lock);

  finfo = get_file_info (fd);
  if (finfo == NULL) {
    lock_release (&filesys_lock);
    return -1;
  }

  kbuf = malloc (size);
	if (kbuf == NULL) {
    lock_release (&filesys_lock);
		return -1;
	}
  memcpy (kbuf, buffer, size);

  ret = file_write (finfo->file, kbuf, size);
	free (kbuf);
  lock_release (&filesys_lock);

  return ret;
}
/*----------------------------------------------------------------------------*/
void
seek (int fd, unsigned position)
{
  struct file_info *finfo;

	if (!lock_held_by_current_thread (&filesys_lock))
		lock_acquire (&filesys_lock);

  finfo = get_file_info (fd);
  if (finfo == NULL) {
    lock_release (&filesys_lock);
    return;
  }

  file_seek (finfo->file, (off_t)position);
  lock_release (&filesys_lock);
}
/*----------------------------------------------------------------------------*/
unsigned
tell (int fd)
{
  struct file_info *finfo;
  unsigned ret;

	if (!lock_held_by_current_thread (&filesys_lock))
		lock_acquire (&filesys_lock);

  finfo = get_file_info (fd);
  if (finfo == NULL) {
    lock_release (&filesys_lock);
    return 0;
  }

  ret = file_tell (finfo->file);
  lock_release (&filesys_lock);

  return ret;
}
/*----------------------------------------------------------------------------*/
void
close (int fd)
{
  struct file_info *finfo;

	if (!lock_held_by_current_thread (&filesys_lock))
		lock_acquire (&filesys_lock);

  finfo = get_file_info (fd);
  if (finfo == NULL) {
    lock_release (&filesys_lock);
    return;
  }

  file_close (finfo->file);
  list_remove (&finfo->file_elem);
  free (finfo);
  lock_release (&filesys_lock);
}
/*----------------------------------------------------------------------------*/
#ifdef FILESYS
bool
chdir (const char *dir)
{
	struct dir *parent_dir;		/* parent dir of destination dir */
	char *target_name;
	struct inode *inode;
	struct thread *cur;

	parent_dir = parse_dir_name (dir);
	if (parent_dir == NULL)
		return false;

	target_name = get_target_name (dir);
	if (!dir_lookup (parent_dir, target_name, &inode))
		return false;

	/* Change current thread's working directory */
	cur = thread_current ();
	dir_close (cur->curdir);
	cur->curdir = dir_open (inode);

	return true;
}
/*----------------------------------------------------------------------------*/
bool
mkdir (const char *dir)
{
	struct dir *parent_dir;		/* parent dir of dir to be made */
	char *target_name;
	struct inode *inode;
	block_sector_t sector;
	bool success;

	parent_dir = parse_dir_name (dir);
	target_name = get_target_name (dir);
	if (target_name[strlen (target_name) - 1] == '/')
		target_name[strlen (target_name) - 1] = 0;

	sector = INVALID_SECTOR;
	success = (parent_dir != NULL
						 && !dir_lookup (parent_dir, target_name, &inode)
						 && free_map_allocate (1, &sector)
						 && dir_create (sector, INITIAL_FILE_NUM)
						 && dir_add (parent_dir, target_name, sector));

	if (!success && (sector != INVALID_SECTOR))
		free_map_release (sector, 1);

	if (parent_dir != NULL)
		dir_close (parent_dir);

	free (target_name);

	return success;
}
/*----------------------------------------------------------------------------*/
bool
readdir (int fd, char *name)
{
  struct file_info *finfo;
	struct inode *inode;
	struct dir *dir;

	finfo = get_file_info (fd);
	inode = file_get_inode (finfo->file);
	if (!inode_is_dir (inode))
		return false;

	dir = finfo->dir;
	if (dir == NULL)
		return false;

	return dir_readdir (dir, name);
}
/*----------------------------------------------------------------------------*/
bool
isdir (int fd)
{
  struct file_info *finfo;
	struct inode *inode;

	finfo = get_file_info (fd);
	inode = file_get_inode (finfo->file);

	return inode_is_dir (inode);
}
/*----------------------------------------------------------------------------*/
int
inumber (int fd)
{
  struct file_info *finfo;
	struct inode *inode;

	finfo = get_file_info (fd);
	inode = file_get_inode (finfo->file);

	return inode_get_inumber (inode);
}
#endif /* FILESYS */
/*----------------------------------------------------------------------------*/
/* Helper functions																														*/
/*----------------------------------------------------------------------------*/
/* Check whether user-provided pointer UPTR is in the valid address space */
#ifdef VM
static void
check_user_ptr (const void *uptr, void *esp)
#else
static void
check_user_ptr (const void *uptr, void *esp UNUSED)
#endif
{
#ifdef VM
	bool success;
	struct sup_page_entry *spte;
	void *user_addr;
#endif

  if (!is_user_vaddr (uptr) || (uptr < BOTTOM_USER_SPACE))
		exit (-1);

#ifdef VM
	success = false;
	user_addr = (void *)((uintptr_t)uptr & ~(uintptr_t)0xfff);
	spte = sup_lookup (user_addr);
	if (spte != NULL) {
		if ((spte->type == FILE_DATA) || (spte->type == SWAP_FILE))
			success = load_sup_data_to_frame (spte);
		else if (spte->type == PAGE_TABLE)
			success = true;
	} else if ((esp - STACK_HEURISTIC) < uptr) {
		success = stack_growth ((void *)uptr);
	}

	if (!success)
		exit (-1);
#endif
}
/*----------------------------------------------------------------------------*/
/* Check whether user-provided string is in the valid address space */
static void
check_user_string (const char *ustr, void *esp)
{
	char *str = (char *)ustr;

	while (*str != 0) {
		check_user_ptr ((const void *)str, esp);
		str++;
	}
}
/*----------------------------------------------------------------------------*/
/* Check whether user-provided buffer UPTR is in the valid address space */
static void
check_user_buffer (const void *uptr, unsigned size, void *esp)
{
  char *ptr = (char *)uptr;
  unsigned i;

  for (i = 0; i < size; i++) {
    check_user_ptr ((const void *)ptr, esp);
    ptr++;
  }
}
/*----------------------------------------------------------------------------*/
/* Get system call arguments from caller's stack */
static void
get_sysarg (struct intr_frame *f, int *sysarg, int arg_num)
{
  int i;
  int *ptr;

  for (i = 0; i < arg_num; i++) {
    ptr = (int *)f->esp + i + 1; /* +1 for syscall number */
    check_user_ptr ((const void *)ptr, f->esp);
    sysarg[i] = *ptr;
	}
}
/*----------------------------------------------------------------------------*/

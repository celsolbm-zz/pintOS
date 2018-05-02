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

#define MAX_ARG 3 /* Maximum number of system call arguments */

static void syscall_handler (struct intr_frame *);
static int child_number = 0;
/*
 * Helper functions
 */
void check_user_ptr (const void *uptr);
void check_user_ptr2 (const void *uptr);
void check_user_buffer (const void *uptr, unsigned size);
void get_sysarg (struct intr_frame *f, int *sysarg, int arg_num);
int get_kernel_vaddr (const void *uaddr);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init (&filesys_lock);
}

static void
syscall_handler (struct intr_frame *f) 
{
  int sysno;
  int sysarg[MAX_ARG];

  check_user_ptr ((const void *)f->esp);
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
      sysarg[0] = get_kernel_vaddr ((const void *)sysarg[0]);
      f->eax = exec ((const char *)sysarg[0]);
      break;

    case SYS_WAIT:
      get_sysarg (f, &sysarg[0], 1);
      f->eax = wait ((pid_t)sysarg[0]);
      break;

    case SYS_CREATE:
      get_sysarg (f, &sysarg[0], 2);
      sysarg[0] = get_kernel_vaddr ((const void *)sysarg[0]);
      f->eax = create ((const char *)sysarg[0], (unsigned)sysarg[1]);
      break;

    case SYS_REMOVE:
      get_sysarg (f, &sysarg[0], 1);
    sysarg[0] = get_kernel_vaddr ((const void *)sysarg[0]);
      f->eax = remove ((const char *)sysarg[0]);
      break;

    case SYS_OPEN:
      get_sysarg (f, &sysarg[0], 1);
      sysarg[0] = get_kernel_vaddr ((const void *)sysarg[0]);
      f->eax = open ((const char *)sysarg[0]);
      break;

    case SYS_FILESIZE:
      get_sysarg (f, &sysarg[0], 1);
      f->eax = filesize (sysarg[0]);
      break;

    case SYS_READ:
      get_sysarg (f, &sysarg[0], 3);
      check_user_buffer ((const void *)sysarg[1], (unsigned)sysarg[2]);
//			sysarg[1] = get_kernel_vaddr ((const void *)sysarg[1]);
			f->eax = read (sysarg[0], (void *)sysarg[1], (unsigned)sysarg[2]);
      break;

    case SYS_WRITE:
      get_sysarg (f, &sysarg[0], 3);
      check_user_buffer ((const void *)sysarg[1], (unsigned)sysarg[2]);
      sysarg[1] = get_kernel_vaddr ((const void *)sysarg[1]);
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
  }
}

/*
 * System call
 */

void
halt (void)
{
  shutdown_power_off ();
}

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

pid_t
exec (const char *cmd_line)
{
  pid_t child_pid;
  struct child_info *chinfo;

  if(child_number > 30)
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

int
wait (pid_t pid)
{
  return process_wait (pid);
}

bool
create (const char *file, unsigned initial_size)
{
  bool ret;

  lock_acquire (&filesys_lock);
  ret = filesys_create (file, (off_t)initial_size);
  lock_release (&filesys_lock);

  return ret;
}

bool
remove (const char *file)
{
  bool ret;

  lock_acquire (&filesys_lock);
  ret = filesys_remove (file);
  lock_release (&filesys_lock);

  return ret;
}

int
open (const char *file)
{
  struct thread *cur = thread_current ();
  struct file_info *finfo;
  struct file *temp_file;

  lock_acquire (&filesys_lock);
  temp_file = filesys_open (file);
  if (temp_file == NULL) {
    lock_release (&filesys_lock);
		return -1;
  }

  finfo = malloc (sizeof(struct file_info));
  if (finfo == NULL) {
    lock_release (&filesys_lock);
    return -1;
  }
  finfo->fd = cur->min_fd;
  cur->min_fd++;
  finfo->file = temp_file;

  list_push_back (&cur->open_file, &finfo->file_elem);
  lock_release (&filesys_lock);

  return finfo->fd;
}

int
filesize (int fd)
{
  int ret;
  struct file_info *finfo;

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

int
read (int fd, void *buffer, unsigned size)
{
  int ret;
  struct file_info *finfo;
  if (fd == STDIN_FILENO) {
    char *bufptr = (char *)buffer;
    unsigned i;
    for (i = 0; i < size; i++) {
      *bufptr = input_getc ();
      bufptr++;
    }
    return (int)size;
  }

  lock_acquire (&filesys_lock);
  finfo = get_file_info (fd);
  if (finfo == NULL) {
    lock_release (&filesys_lock);
    return -1;
  }
  ret = file_read (finfo->file, buffer, size);
  lock_release (&filesys_lock);
  return ret;
}

int
write (int fd, const void *buffer, unsigned size)
{
  int ret;
  struct file_info *finfo;

  if (fd == STDOUT_FILENO) {
    putbuf (buffer, (size_t)size);
    return (int)size;
  }

  lock_acquire (&filesys_lock);
  finfo = get_file_info (fd);
  if (finfo == NULL) {
    lock_release (&filesys_lock);
    return -1;
  }

  ret = file_write (finfo->file, buffer, size);
  lock_release (&filesys_lock);

  return ret;
}

void
seek (int fd, unsigned position)
{
  struct file_info *finfo;

  lock_acquire (&filesys_lock);
  finfo = get_file_info (fd);
  if (finfo == NULL) {
    lock_release (&filesys_lock);
    return;
  }

  file_seek (finfo->file, (off_t)position);
  lock_release (&filesys_lock);
}

unsigned
tell (int fd)
{
  struct file_info *finfo;
  unsigned ret;

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

void
close (int fd)
{
  struct file_info *finfo;

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

/*
 * Helper functions
 */

/* Check whether user-provided pointer UPTR is in the valid address space */
void
check_user_ptr (const void *uptr)
{
  if (!is_user_vaddr(uptr) || (uptr < BOTTOM_USER_SPACE))
		exit (-1);
}


void
check_user_ptr2 (const void *uptr)
{
  if (!is_user_vaddr(uptr) || (uptr < BOTTOM_USER_SPACE))
    printf("\n testing pointer after the fetching of data, NOT A UADDR!!");
else
{ printf("\n oh shit it is a uaddr");
//if (*uptr==NULL)
//printf("\n well at least is null\n");

}
}



/* Check whether user-provided buffer UPTR is in the valid address space */
void
check_user_buffer (const void *uptr, unsigned size)
{
  char *ptr = (char *)uptr;
  unsigned i;

  for (i = 0; i < size; i++) {
    check_user_ptr ((const void *)ptr);
    ptr++;
  }
}

/* Get system call arguments from caller's stack */
void
get_sysarg (struct intr_frame *f, int *sysarg, int arg_num)
{
  int i;
  int *ptr;
  for (i = 0; i < arg_num; i++) {
    ptr = (int *)f->esp + i + 1; /* +1 for syscall number */
    check_user_ptr ((const void *)ptr);
    sysarg[i] = *ptr;
 }


}

/* Get kernel virtual address from user virtual address */
int
get_kernel_vaddr (const void *uaddr)
{
  void *kaddr;
  struct thread *cur = thread_current ();

  check_user_ptr (uaddr);
  kaddr = pagedir_get_page (cur->pagedir, uaddr);
  if (kaddr == NULL) {
    /* There is no valid physical address for uaddr */
		exit (-1);
  }

  return (int)kaddr;
}

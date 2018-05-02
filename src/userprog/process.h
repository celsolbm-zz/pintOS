#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include <list.h>
#include "threads/synch.h"
#include "filesys/file.h"

#define NO_PARENT (-1)
typedef int pid_t;

bool install_page (void *upage, void *kpage, bool writable);

/* Load status */
typedef enum load_status {
  LOAD_NOT_BEGIN =  1 << 0,
  LOAD_SUCCESS =    1 << 1,
  LOAD_FAIL =       1 << 2,
} LOAD_STATUS;

/* Wait status */
typedef enum wait_status {
  NOT_WAITED =      1 << 0,
  WAITED =          1 << 1,
} WAIT_STATUS;

/* Exit status */
typedef enum exit_status {
  NOT_EXITED =      1 << 0,
  EXITED =          1 << 1,
} EXIT_STATUS;

pid_t process_execute (const char *file_name);
int process_wait (pid_t);
void process_exit (void);
void process_activate (void);

/* Process file information */
#define INITIAL_FD 2

struct file_info {
  int fd;
  struct file *file;
  struct list_elem file_elem;
};

struct file_info *get_file_info (int fd);
void destroy_file_list (void);


/* Child information */
struct child_info {
  pid_t pid;
  LOAD_STATUS load_status;
  WAIT_STATUS wait_status;
  EXIT_STATUS exit_status;
  struct semaphore exec_sema;
  struct semaphore exit_sema;;
  int exit_code;
  struct list_elem child_elem;
};

struct child_info *create_child_info (pid_t pid);
struct child_info *get_child_info (pid_t pid);
void remove_child_info (struct child_info *chinfo);
void destroy_child_list (void);
bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                   uint32_t read_bytes, uint32_t zero_bytes,
                   bool writable);
#endif /* userprog/process.h */

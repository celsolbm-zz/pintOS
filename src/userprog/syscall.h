#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <user/syscall.h>
#include "threads/synch.h"

void syscall_init (void);

/* File system lock */
struct lock filesys_lock;

#endif /* userprog/syscall.h */

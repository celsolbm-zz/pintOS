#ifndef THREADS_SYNCH_H
#define THREADS_SYNCH_H

#include <list.h>
#include <stdbool.h>

/* A counting semaphore. */
struct semaphore 
  {
    unsigned value;             /* Current value. */
    struct list waiters;        /* List of waiting threads. */
  };

void sema_init (struct semaphore *, unsigned value);
void sema_down (struct semaphore *);
bool sema_try_down (struct semaphore *);
void sema_up (struct semaphore *);
void sema_self_test (void);

/* Lock. */
struct lock 
  {
    /* Thread holding lock (for debugging). */
    struct thread *holder;

    /* List of threads which try to acquire this lock */
    struct list wait_list;

    /* List element for holding list in thread */
    struct list_elem holding_elem;

    /* Binary semaphore controlling access. */
    struct semaphore semaphore;
  };

void lock_init (struct lock *);
void lock_acquire (struct lock *);
void remove_from_wait_list (struct thread *, struct lock *);
bool lock_try_acquire (struct lock *);
void lock_release (struct lock *);
bool lock_held_by_current_thread (const struct lock *);

/* Condition variable. */
struct condition 
  {
    struct list waiters;        /* List of waiting threads. */
  };

/* One semaphore in a list */
struct semaphore_elem
{
  struct list_elem elem;      /* List elemnt */
  struct semaphore semaphore; /* This semaphore */
  int priority;               /* Associated priority */
};

void cond_init (struct condition *);
void cond_wait (struct condition *, struct lock *);
void cond_signal (struct condition *, struct lock *);
void cond_broadcast (struct condition *, struct lock *);

/* Optimization barrier.

   The compiler will not reorder operations across an
   optimization barrier.  See "Optimization Barriers" in the
   reference guide for more information.*/
#define barrier() asm volatile ("" : : : "memory")

#endif /* threads/synch.h */

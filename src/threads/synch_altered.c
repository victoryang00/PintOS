/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
*/

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
     decrement it.

   - up or "V": increment the value (and wake up one waiting
     thread, if any). */
void
sema_init (struct semaphore *sema, unsigned value) 
{
  ASSERT (sema != NULL);

  sema->value = value;
  list_init (&sema->waiters);
  /*Added by moon*/
  sema->sema_priority = PRI_MIN-1;
  /*Added by moon*/
}

/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. */
void
sema_down (struct semaphore *sema) 
{
  enum intr_level old_level;

  ASSERT (sema != NULL);
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  while (sema->value == 0) 
    {
      list_push_back (&sema->waiters, &thread_current ()->elem);
      thread_block ();
    }
  sema->value--;
  intr_set_level (old_level);
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
bool
sema_try_down (struct semaphore *sema) 
{
  enum intr_level old_level;
  bool success;

  ASSERT (sema != NULL);

  old_level = intr_disable ();
  if (sema->value > 0) 
    {
      sema->value--;
      success = true; 
    }
  else
    success = false;
  intr_set_level (old_level);

  return success;
}

/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler. */
void
sema_up (struct semaphore *sema) 
{
  enum intr_level old_level;

  ASSERT (sema != NULL);
  /*Added by moon*/
  struct thread *wake_up; /*需要唤醒的线程*/
  wake_up = NULL;
  /*Added by moon*/

  old_level = intr_disable ();
  if (!list_empty (&sema->waiters)) 
  {
     /*thread_unblock (list_entry (list_pop_front (&sema->waiters),
                                struct thread, elem));*/
     /*Added by moon*/
     /*对sema的waiters队列按照优先级进行排序*/
     list_sort (&sema->waiters, priority_higher, NULL); 
     /*唤醒队列头，也就是队列中优先级最高的线程*/
     wake_up = list_entry (list_pop_front (&sema->waiters), struct thread, elem);
     thread_unblock (wake_up);
     /*Added by moon*/
  }
  sema->value++;
  /*Added by moon*/
  /*如果当前线程的优先级比唤醒的线程的低，就要放弃CPU*/
  if(wake_up != NULL && thread_current()->priority < wake_up->priority)
     thread_yield();
  /*Added by moon*/
  intr_set_level (old_level);
}

static void sema_test_helper (void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void
sema_self_test (void) 
{
  struct semaphore sema[2];
  int i;

  printf ("Testing semaphores...");
  sema_init (&sema[0], 0);
  sema_init (&sema[1], 0);
  thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
  for (i = 0; i < 10; i++) 
    {
      sema_up (&sema[0]);
      sema_down (&sema[1]);
    }
  printf ("done.\n");
}

/* Thread function used by sema_self_test(). */
static void
sema_test_helper (void *sema_) 
{
  struct semaphore *sema = sema_;
  int i;

  for (i = 0; i < 10; i++) 
    {
      sema_down (&sema[0]);
      sema_up (&sema[1]);
    }
}

/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
void
lock_init (struct lock *lock)
{
  ASSERT (lock != NULL);

  lock->holder = NULL;
  sema_init (&lock->semaphore, 1);
  /*Added by moon*/
  lock->lock_priority = PRI_MIN-1;
  /*Added by moon*/
}

/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
lock_acquire (struct lock *lock)
{
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (!lock_held_by_current_thread (lock));

  /*Added by moon*/
  struct thread *thrd; /*持有锁但是优先级较低的线程*/
  struct thread *curr;  /*正在申请使用锁的线程*/
  struct lock *another; /*当前被thrd持有，curr正在申请的锁*/

  enum intr_level old_level;
  old_level = intr_disable();

  /*初始化声明的变量*/
  curr = thread_current();
  thrd = lock->holder;
  curr->blocked = another = lock;

  /*可以解决donate-nest的问题*/
  while(thrd != NULL && thrd->priority < curr->priority)
  {
    /*捐赠优先级*/
    thrd->donated = true;
    thread_set_other_priority (thrd, curr->priority, false);
    if(another->lock_priority < curr->priority)
      another->lock_priority = curr->priority;

    /*假如捐赠优先级的线程也因为缺锁被block,将another更新为它需要的锁，thrd更新为使它block的线程*/
    if(!thread_mlfqs && thrd->status == THREAD_BLOCKED && thrd->blocked != NULL)
    {
      another = thrd->blocked;
      thrd = thrd->blocked->holder; 
    }
    else
      break;
  }
  /*Added by moon*/
  
  sema_down (&lock->semaphore);
  lock->holder = curr;

  /*Added by moon*/
  if(!thread_mlfqs)
  {
    /*当前线程已经获得锁*/
    lock->lock_priority = curr->priority;
    curr->blocked = NULL;
    /*将锁按照优先级的顺序插入当前线程的锁队列中*/
    list_insert_ordered(&(curr->locks), &(lock->holder_elem), 
                         lock_priority_higher, NULL);
  }
  intr_set_level(old_level);
  /*Added by moon*/
}

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
bool
lock_try_acquire (struct lock *lock)
{
  bool success;

  ASSERT (lock != NULL);
  ASSERT (!lock_held_by_current_thread (lock));

  success = sema_try_down (&lock->semaphore);
  if (success)
    lock->holder = thread_current ();
  return success;
}

/* Releases LOCK, which must be owned by the current thread.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */
void
lock_release (struct lock *lock) 
{
  ASSERT (lock != NULL);
  ASSERT (lock_held_by_current_thread (lock));

  /*Added by moon*/
  struct thread *curr;
  struct list_elem *I;
  struct lock *another;
  enum intr_level old_level;

  curr = thread_current();
  old_level = intr_disable();

  /*Added by moon*/
  
  lock->holder = NULL;
  if(!thread_mlfqs)
  {
    /*将锁从相应的队列中移除*/
    list_remove(&(lock->holder_elem));
    lock->lock_priority = PRI_MIN-1;
  }
  sema_up (&lock->semaphore);
  /*Added by moon*/ 
  if(!thread_mlfqs)
  {
    /*如果当前的线程持有的锁的队列为空，就恢复到原来的优先级*/
    if(list_empty(&curr->locks))
    {
      curr->donated = false;
      thread_set_priority (curr->old_priority);
    }
    /*否则将锁队列头的优先级捐赠给它，可以解决donate-multiple的问题*/
    else
    {
      I = list_front(&curr->locks);
      another = list_entry(I, struct lock, holder_elem);
      thread_set_other_priority (thread_current(), another->lock_priority, false);
    }
  }
  intr_set_level(old_level);
  /*Added by moon*/
}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool
lock_held_by_current_thread (const struct lock *lock) 
{
  ASSERT (lock != NULL);

  return lock->holder == thread_current ();
}

/* One semaphore in a list. */
struct semaphore_elem 
  {
    struct list_elem elem;              /* List element. */
    struct semaphore semaphore;         /* This semaphore. */
  };

/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void
cond_init (struct condition *cond)
{
  ASSERT (cond != NULL);

  list_init (&cond->waiters);
}

/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.

   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
cond_wait (struct condition *cond, struct lock *lock) 
{
  struct semaphore_elem waiter;

  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));
  
  sema_init (&waiter.semaphore, 0);
  /*Added by moon*/
  /*将waiter对应的semaphore的优先级设置为当前的优先级*/
  (waiter.semaphore).sema_priority = thread_current()->priority; 
  /*将waiter按照优先级高优先的顺序插入cond的waiters队列*/
  list_insert_ordered(&cond->waiters,&(waiter.elem),sema_priority_higher,NULL);
  /*Added by moon*/

  /*list_push_back (&cond->waiters, &waiter.elem);*/
  lock_release (lock);
  sema_down (&waiter.semaphore);
  lock_acquire (lock);
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_signal (struct condition *cond, struct lock *lock UNUSED) 
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));

  if (!list_empty (&cond->waiters)) 
    sema_up (&list_entry (list_pop_front (&cond->waiters),
                          struct semaphore_elem, elem)->semaphore);
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_broadcast (struct condition *cond, struct lock *lock) 
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);

  while (!list_empty (&cond->waiters))
    cond_signal (cond, lock);
}

/*Added by moon*/
/*比较两个list_elem对应的锁的优先级*/
bool
lock_priority_higher (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED)
{
  struct lock *l1, *l2;
  l1 = list_entry (a, struct lock, holder_elem);
  l2 = list_entry (b, struct lock, holder_elem);
  return (l1->lock_priority > l2->lock_priority);
}

/*比较两个list_elem对应的semaphore_elem所对应的semaphore的优先级*/
bool
sema_priority_higher (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED)
{
  struct semaphore_elem *elem1,*elem2;
  struct semaphore *s1, *s2;
  elem1 = list_entry (a, struct semaphore_elem, elem);
  elem2 = list_entry (b, struct semaphore_elem, elem);
  s1 = &(elem1->semaphore);
  s2 = &(elem2->semaphore);
  return (s1->sema_priority > s2->sema_priority);
}
/*Added by moon*/

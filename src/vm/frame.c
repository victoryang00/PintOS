#include "vm/frame.h"
#include <stdio.h>
#include "vm/page.h"
#include "devices/timer.h"
#include "threads/init.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/thread.h"

static struct frame *frames;
static uint32_t count;
static struct lock scan_lock;
static uint32_t handle;
static struct list frame_list;

/* Initialize the frame manager. */
void
frame_init (void) 
{
  void *base;

  lock_init (&scan_lock);
  
  frames = malloc (sizeof *frames * init_ram_pages);
  if (frames == NULL)
    PANIC ("out of memory allocating page frames");

  while ((base = palloc_get_page (PAL_USER)) != NULL) 
    {
      struct frame *f = &frames[count++];
      lock_init (&f->lock);
      f->base = base;
      f->page = NULL;
    }
}

/* Tries to allocate and lock a frame for PAGE.
   Returns the frame if successful, false on failure. */
static struct frame *
try_frame_alloc_and_lock (struct spt_elem *page) 
{
  size_t i;

  lock_acquire (&scan_lock);

  /* Find a free frame. */
  for (i = 0; i < count; i++)
    {
      struct frame *f = &frames[i];
      if (!lock_try_acquire (&f->lock))
        continue;
      if (f->page == NULL) 
        {
          f->page = page;
          lock_release (&scan_lock);
          return f;
        } 
      lock_release (&f->lock);
    }

  /* No free frame.  Find a frame to evict. */
  for (i = 0; i < count * 2; i++) 
    {
      /* Get a frame. */
      struct frame *f = &frames[handle];
      if (++handle >= count)
        handle = 0;

      if (!lock_try_acquire (&f->lock))
        continue;

      if (f->page == NULL) 
        {
          f->page = page;
          lock_release (&scan_lock);
          return f;
        } 

      if (page_get_recently (f->page)) 
        {
          lock_release (&f->lock);
          continue;
        }
          
      lock_release (&scan_lock);
      
      /* Evict this frame. */
      if (!page_out (f->page))
        {
          lock_release (&f->lock);
          return NULL;
        }

      f->page = page;
      return f;
    }

  lock_release (&scan_lock);
  return NULL;
}


/* Tries really hard to allocate and lock a frame for PAGE.
   Returns the frame if successful, false on failure. */
struct frame *
frame_alloc (struct spt_elem *page) 
{
  size_t try;

  for (try = 0; try < 3; try++) 
    {
      struct frame *f = try_frame_alloc_and_lock (page);
      if (f != NULL) 
        {
          ASSERT (lock_held_by_current_thread (&f->lock));
          return f; 
        }
      timer_msleep (1000);
    }

  return NULL;
}

/* Locks P's frame into memory, if it has one.
   Upon return, p->frame will not change until P is unlocked. */
void
frame_lock (struct spt_elem *p) 
{
  /* A frame can be asynchronously removed, but never inserted. */
  struct frame *f = p->frame;
  while (f != NULL) 
    {
      lock_acquire (&f->lock);
      if (f == p->frame) {
          thread_set_nice(0);
      } else {
          (&f->lock)->holder = NULL;
          sema_up(&(&f->lock)->semaphore);
      }
      break;
    }
}

/* Releases frame F for use by another page.
   F must be locked for use by the current process.
   Any data in F is lost. */
void
frame_free (struct frame *f)
{
  f->page = NULL;
    (&f->lock)->holder = NULL;
  sema_up(&(&f->lock)->semaphore);
}

/* Unlocks frame F, allowing it to be evicted.
   F must be locked for use by the current process. */
void
frame_unlock (struct frame *f) 
{
  (&f->lock)->holder = NULL;
  sema_up(&(&f->lock)->semaphore);
}

static struct frame *get_frame_by_paddr(void *paddr) {
  struct list_elem *e;

  for (e = list_begin(&frame_list); e != list_end (&frame_list); e = list_next (e)) {
    struct frame *f = list_entry (e, struct frame, frame_elem);
    if (f->base == paddr) {
      return f;
    }
  }
  return NULL;
}

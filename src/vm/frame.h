#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "threads/synch.h"

/* A physical frame. */
struct frame {
    struct spt_elem *page; /* Mapped process page, if any. */
    struct lock lock; /* Prevent simultaneous access. */
    void *base; /* Kernel virtual base address. */
    struct list_elem frame_elem;
};

void frame_init (void);

struct frame *frame_alloc (struct spt_elem *);
void frame_lock (struct spt_elem *);
void frame_free (struct frame *);
void frame_unlock (struct frame *);
struct frame *frame_try_alloc (struct spt_elem *page);

#endif /* vm/frame.h */
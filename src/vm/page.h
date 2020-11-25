#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include "devices/block.h"
#include "filesys/off_t.h"
#include "threads/synch.h"

/* Virtual page. */
struct spt_elem 
  {
    /* Immutable members. */
    void *addr;                 /* User virtual address. */
    bool read_only;             /* Read-only page? */
    struct thread *thread;      /* Owning thread. */
    uint8_t *upage;             /* The page the descrpting. */

    /* Accessed only in owning process context. */

    /* Set only in owning process context with frame->frame_lock held.
       Cleared only with scan_lock and frame->frame_lock held. */
    struct frame *frame;        /* Page frame. */

    /* Swap information, protected by frame->frame_lock. */
    block_sector_t sector;       /* Starting sector of swap area, or -1. */
    
    /* Memory-mapped file information, protected by frame->frame_lock. */
    bool writable;               /* False to write back to file,
                                   true to write back to swap. */
    struct file *fileptr;      /* Fileptr */
    off_t ofs;                 /* Offset in file. */
    off_t bytes;               /* Bytes to read/write, 1...PGSIZE. */
    struct hash_elem hash_elem;  /* struct thread `pages' hash element. */
    struct list_elem slot_elem;     /* Swap slot */
  };



void page_exit (void);
struct spt_elem *page_allocate (void *, int read_only);
struct spt_elem *page_find (void *upage);
void page_deallocate (void *vaddr);
int page_in (void *fault_addr);
int page_get_in (struct spt_elem *p);
int page_out (struct spt_elem *);
int page_get_recently (struct spt_elem *);
int page_lock (const void *, int will_write);
void page_unlock (const void *);
void page_destroy (struct hash_elem *tmp, void *aux);
struct spt_elem *page_get_addr (const void *address);
bool page_validate (struct spt_elem *page, bool will_write);
hash_hash_func page_hash;
hash_less_func page_less;

#endif /* vm/page.h */
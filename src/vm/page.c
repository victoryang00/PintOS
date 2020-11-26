#include <stdio.h>
#include <string.h>
#include "vm/page.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include "filesys/file.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"

/* Adds a mapping for user virtual address VADDR to the page hash
   table.  Fails if VADDR is already mapped or if memory
   allocation fails. */
struct spt_elem *page_allocate (void *vaddr, int read_only){
    struct thread *t = thread_current();
    struct spt_elem *p = malloc(sizeof *p);
    if (p == NULL) {
        return p;
    } else {
        p->addr = pg_round_down(vaddr);
        p->read_only = read_only;
        p->writable = !read_only;
        p->frame = NULL;
        p->sector = (block_sector_t)-1;
        p->fileptr = NULL;
        p->ofs = 0;
        p->bytes = 0;
        p->thread = thread_current();

        return (hash_insert(t->pages, &p->hash_elem)!= NULL) ?  free(p),(void *)0:p;        
    }
};

/* Find the page containing 
   address the given page. */
struct spt_elem *
page_find (void *upage){
    struct thread *t = thread_current();
    struct spt_elem *spte;
    struct list_elem *e;
    e=list_begin(&t->stack_pointer);
    while(e!=list_end(&t->stack_pointer)){
        spte=(struct spt_elem *) list_entry (e, struct spt_elem, slot_elem);
        if(upage==spte->upage){
            return e;
            e=list_next(e);
        }
        e=list_next(e);
    }
    return 0;
};

/* Returns the page containing the given virtual ADDRESS,
   or a null pointer if no such page exists. */
struct spt_elem *
page_get_addr (const void *address){
    if(address < PHYS_BASE){
        struct spt_elem p;
        struct hash_elem *e;
        struct thread* t = thread_current();
        p.addr = (void*) ((uintptr_t) address & ~PGMASK);
        int s = hash_find (t->pages, &p.hash_elem);
        int tb = address >= PHYS_BASE - 1024 * 1024;
        int b = address >= t->stack_pointer - 32;
        int st = s * 4 + tb * 2 + b;

        switch (st) {
        case 0:
            return hash_entry(e, struct spt_elem, hash_elem);
            break;
        case 1:
            return hash_entry(e, struct spt_elem, hash_elem);
            break;
        case 2:
            return hash_entry(e, struct spt_elem, hash_elem);
            break;
        case 3:
            return hash_entry(e, struct spt_elem, hash_elem);
            break;
        case 4:
            return (void *)(0);
            break;
        case 5:
            return (void *)(0);
            break;
        case 6:
            return (void *)(0);
            break;
        case 7:
            return page_allocate((void *)address, false);
            break;
        }
    }
};

/* Evicts the page containing address VADDR
   and removes it from the page table. */
void page_deallocate (void *vaddr){
    struct spt_elem *p = page_get_addr(vaddr);
    struct frame *f = p->frame;
    struct thread *t = thread_current();

    frame_lock(p);

    int s = p->frame;
    int tb = p->fileptr;
    int b = p->writable;
    int st = s * 4 + tb * 2 + b;

    switch (st) {
    case 0:
        frame_free(f);
        hash_delete(t->pages, &p->hash_elem);
        free(p);
        break;
    case 1:
        frame_free(f);
        hash_delete(t->pages, &p->hash_elem);
        free(p);
        break;
    case 2:
        page_out(p);
        frame_free(f);

        hash_delete(t->pages, &p->hash_elem);
        free(p);
        break;
    case 3:
        frame_free(f);
        hash_delete(t->pages, &p->hash_elem);
        free(p);
        break;
    case 4:
        hash_delete(t->pages, &p->hash_elem);
        free(p);
        break;
    case 5:
        hash_delete(t->pages, &p->hash_elem);
        free(p);
        break;
    case 6:
        hash_delete(t->pages, &p->hash_elem);
        free(p);
        break;
    case 7:
        hash_delete(t->pages, &p->hash_elem);
        free(p);
        break;
    }

    if (p->frame) {
        if (p->fileptr && p->writable)
            frame_free(f);
        else {
            page_out(p);
            frame_free(f);
        }
    }
    hash_delete(t->pages, &p->hash_elem);
    free(p);
};


/* Returns the page containing the given virtual ADDRESS,
   or a null pointer if no such page exists.
   Allocates stack pages as necessary. */
static struct page *page_for_addr(const void *address) {
    if (address < PHYS_BASE) {
        struct spt_elem p;
        struct hash_elem *e;

        /* Find existing page. */
        p.addr = (void *)pg_round_down(address);
        e = hash_find(thread_current()->pages, &p.hash_elem);
        if (e != NULL)
            return hash_entry(e, struct spt_elem, hash_elem);

        if (address >= PHYS_BASE - 1024 * 1024) {
            if (address >= thread_current()->stack_pointer - 32) {
                return page_allocate((void *)address, false);
            }
        }
    }
    return NULL;
}

/* Faults in the page containing FAULT_ADDR.
   Returns true if successful, false on failure. */
int
page_in (void *fault_addr) 
{
  struct spt_elem *p;
  int success;

  /* Can't handle page faults without a hash table. */
  if (thread_current ()->pages == NULL) 
    return false;

  p = page_for_addr (fault_addr);
  if (p == NULL) 
    return false; 

  frame_lock (p);
  if (p->frame == NULL)
    {
      if (!page_get_in (p))
        return false;
    }
  ASSERT (lock_held_by_current_thread (&p->frame->lock));
    
  /* Install frame into page table. */
  success = pagedir_set_page (thread_current ()->pagedir, p->addr,
                              p->frame->base, !p->read_only);

  /* Release frame. */
  frame_unlock (p->frame);

  return success;
}

/* Evicts page P.
   P must have a locked frame.
   Return true if successful, false on failure. */
int
page_out (struct spt_elem *page){
    struct frame *frame = page->frame;
    struct thread * thread = page->thread;
    pagedir_clear_page(thread->pagedir, page->addr);
    
    int s = (page->fileptr != (void *)0);
    bool t = pagedir_is_dirty(thread->pagedir, page->addr);
    int st= s*2+(int)t;
    switch (st) {
    case 0:
        frame = (void *)0;
        return true;
        break;
    case 1:
        s = (page->writable) ? swap_out(page)
                             : file_write_at(page->fileptr, frame->base, page->bytes, page->ofs) == page->bytes;
        break;
    case 2:
        s = swap_out(page);
        break;
    case 3:
        s = swap_out(page);
        break;
    };
    if (s) {
        frame = (void *)0;
    }
    return s;
};

/* Returns true if page P's data has been accessed recently,
   false otherwise. P must have a frame locked into memory. */
int page_get_recently(struct spt_elem *page) {
    if (pagedir_is_accessed(page->thread->pagedir, page->addr)) {
        pagedir_set_accessed(page->thread->pagedir, page->addr, false);
        return 1;
    } else {
        return 0;
    }
};

/* Destroys the current process's page table. */
void 
page_exit (void){
    struct thread* t = thread_current();
    struct hash *h =t->pages;
    if (h){
        struct thread* tmp = thread_current();
        tmp->nice = 0;
    }else
        hash_destroy(h, page_destroy);
};



/* To destroy a page*/
void
page_destroy (struct hash_elem *tmp, void *aux){
  struct spt_elem *p = hash_entry (tmp, struct spt_elem, hash_elem);
  frame_lock (p);
  if (p->frame) {
      frame_free(p->frame);
      free(p);
  } else
      free(p);
};
/* Returns true if page A precedes page B. */
bool 
page_less (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
    return (const struct spt_elem *)hash_entry(a, struct spt_elem, hash_elem)->addr <
           (const struct spt_elem *)hash_entry(b, struct spt_elem, hash_elem)->addr;
}

/* Returns a hash value for the page that E refers to. */
unsigned
page_hash (const struct hash_elem *e, void *aux UNUSED) 
{
    return ((uintptr_t)(const struct spt_elem *)hash_entry(e, struct spt_elem, hash_elem)->addr) >> PGBITS;
}

/* Tries to lock the page containing ADDR into physical memory.
   If WILL_WRITE is true, the page must be writeable;
   otherwise it may be read-only.
   Returns true if successful, false on failure. */
int
page_lock (const void *addr, int will_write) 
{
  struct spt_elem *p = page_for_addr (addr);
  if (p == NULL || (p->read_only && will_write))
    return false;
  
  frame_lock (p);
  if (p->frame == NULL)
    return (page_get_in (p)
            && pagedir_set_page (thread_current ()->pagedir, p->addr,
                                 p->frame->base, !p->read_only)); 
  else
    return true;
}

/* Unlocks a page locked with page_lock(). */
void
page_unlock (const void *addr) 
{
  struct spt_elem *p = page_for_addr (addr);
  ASSERT (p != NULL);
  frame_unlock (p->frame);
}


/* Locks a frame for page P and pages it in.
   Returns true if successful, false on failure. */
int
page_get_in (struct spt_elem *p)
{
  /* Get a frame for the page. */
  p->frame = frame_alloc(p);

  if (p->frame == NULL)
      return 0;
  int st = (int)(p->sector != (block_sector_t)-1)*2 + (int) ((p->fileptr != NULL));
  
  switch(st){
      case 0:
        memset(p->frame->base, 0, PGSIZE);
        break;
      case 1:
        /* Get data from file. */
        memset(p->frame->base + (off_t)file_read_at(p->fileptr, p->frame->base, p->bytes, p->ofs), 0,
             (off_t)(PGSIZE - file_read_at(p->fileptr, p->frame->base, p->bytes, p->ofs)));
        break;
      case 2:
        swap_in(p);
        break;
      case 3:
        swap_in(p);
        break;
  }

  return 1;
}



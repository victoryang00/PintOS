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

        return (hash_insert(t->pages, &p->hash_elem)) ? p : (void *)0;
    }
};

/* Find the page containing 
   address the given page. */
struct spt_elem *
page_find (void *upage){
    struct thread *t = thread_current();
    struct spt_elem *spte;
    struct list_elem *e;

    for(e=list_begin(&t->stack_pointer);e!=list_end(&t->stack_pointer);e=list_next(e)){
        spte=(struct spt_elem *) list_entry (e, struct spt_elem, slot_elem);
        if(upage==spte->upage){
            return e;
        }
    }
    return 0;
};

/* Evicts the page containing address VADDR
   and removes it from the page table. */
void page_deallocate (void *vaddr){
    struct spt_elem *p = page_get_addr(vaddr);
    struct frame *f = p->frame;
    struct thread *t = thread_current();

    frame_lock(p);
    if (p->frame) {
        if (!p->fileptr || p->writable)
            frame_free(f);
        else {
            page_out(p);
            frame_free(f);
        }
    }
    hash_delete(t->pages, &p->hash_elem);
    free(p);
};

/* Faults in the page containing FAULT_ADDR.
   Returns true if successful, false on failure. */
int 
page_in (void *fault_addr){
    struct spt_elem *p;
    struct thread *t = thread_current();
    int result=1;
    /* Can't handle page faults without a hash table. */
    if (t->pages == NULL)
        return 0;
    p = page_get_addr(fault_addr);
    if (p == NULL)
        return 0;
    frame_lock(p);
    if (p->frame == NULL && !(bool)page_get_in(p))
        return 0;
    /* Install frame into page table. */
    result = (int)pagedir_set_page(t->pagedir, p->addr, p->frame->base, !p->read_only);
    /* Release frame. */
    frame_unlock(p->frame);

    return result;
};

/* Evicts page P. P must have a locked frame.
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

/* Locks a page unlocked. */
/* Tries to lock the page containing ADDR into physical memory.
   If WILL_WRITE is true, the page must be writeable;
   otherwise it may be read-only.
   Returns true if successful, false on failure. */
int 
page_lock (const void *address, int will_write){
    struct spt_elem *p = page_get_addr(address);
    struct thread* t = thread_current();
    if(page_validate(p,will_write)){
        frame_lock(p);
        return (p->frame == NULL)?(page_get_in(p) && pagedir_set_page(t->pagedir, p->addr, p->frame->base, !p->read_only)):true;
    }else
        return false;
};


/* Unlocks a page locked. */
void 
page_unlock (const void * address){
    struct spt_elem *p = page_get_addr(address);
    ASSERT(p != NULL);
    frame_unlock(p->frame);
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


/* Returns the page containing the given virtual ADDRESS,
   or a null pointer if no such page exists. */
struct spt_elem *
page_get_addr (const void *address){
    if(address < PHYS_BASE){
        struct spt_elem p;
        struct hash_elem *e;
        struct thread* t = thread_current();
        /* Get the existing page. */
        p.addr = (void*) ((uintptr_t) address & ~PGMASK);
        e = hash_find (t->pages, &p.hash_elem);
        if (e) {
            if (address >= PHYS_BASE - 1024 * 1024) {
                if (address >= t->stack_pointer - 32) {
                    /* If the page fault area in the user addr space, just expand our stack.*/
                    return page_allocate((void *)address, false);
                }
            }
            return (void *)(0);
        } else
            return hash_entry(e, struct spt_elem, hash_elem);
    }
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

/* To validate a page*/
bool 
page_validate (struct spt_elem *page, bool will_write){
    return (page != NULL && (!page->read_only || !will_write)) ? true : false;
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



#ifndef VM_SWAP_H
#define VM_SWAP_H 1

#include "vm/page.h"
struct page;
void swap_init (void);
void swap_in (struct spt_elem *);
int swap_out (struct spt_elem *);

#endif /* vm/swap.h */
#ifndef VM_SWAP_H
#define VM_SWAP_H 1

struct page;
void swap_init (void);
void swap_in (struct page *);
int swap_out (struct page *);

#endif /* vm/swap.h */
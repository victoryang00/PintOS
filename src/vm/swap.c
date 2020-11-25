#include <bitmap.h>
#include <debug.h>
#include <stdio.h>
#include "vm/swap.h"
#include "vm/frame.h"
#include "vm/page.h"
#include "threads/synch.h"
#include "threads/vaddr.h"

static struct block *device;
static struct bitmap *swapped_location;
static struct lock swap_lock;

/* Sets up swap. */
void
swap_init (void) 
{
  device = block_get_role (BLOCK_SWAP);
  if (device == NULL) {
      printf ("no swap DEVICEs\n");
      swapped_location = bitmap_create (0);
    }
  else
    swapped_location = bitmap_create (block_size (device)
                                 / PGSIZE * BLOCK_SECTOR_SIZE);
  if (swapped_location == NULL)
    PANIC ("SB swap bitmap");
  lock_init (&swap_lock);
}

/* Swaps in page P, which must have a locked frame
   (and be swapped out). */
void
swap_in (struct spt_elem *p) 
{
    uint32_t i = 0;

    while (i < PGSIZE / BLOCK_SECTOR_SIZE) {
        block_read(device, p->sector + i, p->frame->base + i * BLOCK_SECTOR_SIZE);
        i++;
    }
    bitmap_reset(swapped_location, p->sector / PGSIZE * BLOCK_SECTOR_SIZE);
    p->sector = (block_sector_t)-1;
}

/* Swaps out page P, which must have a locked frame. */
int
swap_out (struct spt_elem *p) 
{
  uint32_t slot;
  uint32_t i = 0;

  lock_acquire (&swap_lock);
  slot = bitmap_scan_and_flip (swapped_location, 0, 1, false);
  lock_release (&swap_lock);
  if (slot == BITMAP_ERROR) 
    return 0; 

  p->sector = slot * PGSIZE / BLOCK_SECTOR_SIZE;

  while(i < PGSIZE / BLOCK_SECTOR_SIZE){
     block_write (device, p->sector + i, p->frame->base + i * BLOCK_SECTOR_SIZE);
     i++;
  } 
  p->writable = false;
  p->fileptr = NULL;
  p->ofs = 0;
  p->bytes = 0;

  return 1;
}
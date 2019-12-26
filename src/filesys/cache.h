#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include <stdbool.h>
#include "devices/block.h"
#include "threads/synch.h"
#include "filesys/filesys.h"

struct cache_entry
{
  bool valid;                   /* if the entry is valid */
  bool dirty;                   /* if this entry has been modified */
  int reference;                /* for clock */
  block_sector_t sector;        /* sector number that this entry holds */
  struct semaphore sector_lock; /* lock for the sector that it holds */
  void *block;                  /* the pointer to the block that it holds */
};

int clock_hand;
struct cache_entry *cache[64];
struct lock cache_lock;
void flush_clear_cache (bool clear);
int get_next_cache_block_to_evict (void);
void 

#endif 
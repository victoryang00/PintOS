#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include "devices/block.h"
#include "devices/timer.h"
#include "threads/synch.h"
#include <list.h>


struct list cache_list;                              /* cache list */
uint32_t cache_size;                            /* current cache number of pintos */
struct lock cache_lock;                                 /* cache lock */                

struct list_elem* head;                                 /* head pointer for clock */


/* cache block 
cotains block array, block sector
dirty  and open_cnt.*/
struct cache_entry {
  uint8_t block[BLOCK_SECTOR_SIZE];                     /* actual data from disk 512 bytes*/
  block_sector_t sector;                                /* sector on disk where the data resides */
  int dirty;                                            /* dirty flag, true if the data was changed */
  int reference_bit;                                    /* reference bit for clock algorithm */
  int count;                                            /* current opened number */
  struct list_elem elem;                                /* to take next the in-place element */
};

void cache_init (void);
struct cache_entry *cache_block (block_sector_t sector);
struct cache_entry *cache_get_block(block_sector_t sector, int dirty);
struct cache_entry* cache_replace (block_sector_t sector, int dirty);



void cache_write_disk (int is_removed);
void cache_back_loop (void *aux);                         /* Back loop to write the whole things back */


#endif /* filesys/cache.h */
#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include "devices/block.h"
#include "devices/timer.h"
#include "threads/synch.h"
#include <list.h>

#define WRITE_BACK_WAIT_TIME 5*TIMER_FREQ
#define MAX_FILESYS_CACHE_SIZE 64                       /* maximum cache size of pintos */

struct list filesys_cache;                              /* cache list */
uint32_t filesys_cache_size;                            /* current cache number of pintos */
struct lock cache_lock;                         /* cache lock */                

struct list_elem* head;                                 /* head pointer for clock */


/* cache block 
cotains block array, block sector
dirty  and open_cnt.*/
struct cache_entry {
  uint8_t block[BLOCK_SECTOR_SIZE];                     /* actual data from disk 512 bytes*/
  block_sector_t sector;                                /* sector on disk where the data resides */
  bool dirty;                                           /* dirty flag, true if the data was changed */
  bool referebce_bit;                                   /* reference bit for clock algorithm */
  int open_cnt;                                         /* current opened number */
  struct list_elem elem;                                /* to take next the in-place element */
};

void cache_init (void);
struct cache_entry *get_block_in_cache (block_sector_t sector);
struct cache_entry* cache_get_block (block_sector_t sector,
					     bool dirty);
struct cache_entry* cache_replace (block_sector_t sector, bool dirty);

struct cache_entry *find_replace();


void cache_write_to_disk (bool is_remove);
void write_cache_back_loop (void *aux);
void thread_func_read_ahead (void *aux);
void spawn_thread_read_ahead (block_sector_t sector);

int test_cache (void);

#endif /* filesys/cache.h */

#include "filesys/cache.h"
#include "filesys/filesys.h"
#include "threads/malloc.h"
#include "threads/thread.h"


/* return the cache block with block sector is SECTOR 
   return null if not found in cache
*/
struct cache_entry *cache_block(block_sector_t sector)
{
  struct cache_entry *cache;
  struct list_elem *e;

  for (e = list_begin(&cache_list); e != list_end(&cache_list); e = (cache->sector != sector) ? list_next(e) : e) {
      cache = list_entry(e, struct cache_entry, elem);
      if (cache->sector != sector) {
      }
      else {
          return cache;
      }
  }

  return (void*)(0);
}
/* Initialize the cache , create a always-runnnin process
   to write the dirty cache back every 5 TIME FREQUENCY
*/
void cache_init(void)
{
  lock_init(&cache_lock);
  cache_size = 0; //frest init filesys cache size as zero.
  list_init(&cache_list);
  thread_create("filesys_cache_writeback", 0, cache_back_loop, NULL);
}


/* called by process, return the cache_entry with the sector number SECTOR*/
struct cache_entry *cache_get_block(block_sector_t sector, int dirty){
  lock_acquire(&cache_lock);
  struct cache_entry *cache = cache_block(sector);
  if (cache){
      cache->count += 1;
      if (dirty == 1)
          cache->dirty = 1;
      else if (cache->dirty == 1)
          cache->dirty = 1;
      cache->reference_bit = 1;
      lock_release(&cache_lock);
      return cache;
      return cache;
  }
  cache = cache_replace(sector, dirty);
  if (cache) {
      lock_release(&cache_lock);
      return cache;
  }
  {
    PANIC("OOM");
  }
}

/* Choose a cache block to be replaced, return the new cache block with
*  the data from disk on SECTOR.
*  If the cache list is not full, just insert the new cache block.
*  Otherwise choose a cache who is not opened to replace.
*/
struct cache_entry *cache_replace(block_sector_t sector,
                                              int dirty)
{
    struct cache_entry *cache;
    struct cache_entry *replace;
    struct list_elem *e;

    switch (cache_size >= 64){
      case 0:
        cache = malloc(sizeof(struct cache_entry));
        switch((int)(cache)*100){
          case 0:
            return NULL;

        }
        cache->count = 0;
        list_push_back(&cache_list, &cache->elem);
        break;
      case 1:
          for (e = list_begin(&cache_list); e != list_end(&cache_list); e = list_next(e)) {
              replace = list_entry(e, struct cache_entry, elem);
              int temp_node_state = (replace->count == 0) *2+(replace->reference_bit);
              switch(temp_node_state){
                case 2:
                  if (replace->dirty) {
                          block_write(fs_device, replace->sector, &replace->block);
                      }
                      cache = replace;
                case 3:
                  replace->reference_bit = false;
              }
          break;
          }
    }
        
  cache->count++;
  cache->sector = sector;
  block_read(fs_device,cache->sector, &cache->block);
  cache->dirty = dirty;
  cache->reference_bit= 1;
  return cache;
}



/* Scan the cache list, if the cache is dirty, write back to the disk
 if IS_REMOVE is true, remove all the cache.  */
void cache_write_disk(int is_removed){
  lock_acquire(&cache_lock);
  struct list_elem *next, *e = list_begin(&cache_list);
  while (e != list_end(&cache_list)) {
      struct cache_entry *cache = list_entry(e, struct cache_entry, elem);
      int cur_cache_state = cache->dirty * 2 + is_removed;
      switch (cur_cache_state) {
      case 2:
          block_write(fs_device, cache->sector, &cache->block);
          cache->dirty = 0;
          break;
      case 3:
          block_write(fs_device, cache->sector, &cache->block);
          cache->dirty = 0;
          list_remove(&cache->elem);
          free(cache);
          break;
      case 1:
          list_remove(&cache->elem);
          free(cache);
      }
      e = list_next(&cache_list);
  }
  lock_release(&cache_lock);
}

/* execute write back dirty cache every 5 time in the sequnce. */
void cache_back_loop(void *aux UNUSED)
{
  for (int i=INT32_MIN; i <INT32_MAX;i++)
  {
      timer_sleep((i > 0) ? i : -i);
      cache_write_disk(0);
  }
}

/* Cache flash to disk, return the number of flash block*/
int cache_examine(void) {
    int try = 0;
    lock_acquire(&cache_lock);
    struct list_elem *e = list_begin(&cache_list);
    while(e!= list_end(&cache_list)){
        struct cache_entry *cache = list_entry(e,struct cache_entry,elem);
        int cache_state = cache->dirty * 2;
        switch(cache_state){
            case 2:
                block_write(fs_device,cache->sector, &cache->block);
                cache->dirty = 0;
                try++;

        }
        e=list_next(e);
    }
    lock_release(&cache_lock);
    return try;

}

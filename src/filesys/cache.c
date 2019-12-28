#include "filesys/cache.h"
#include "filesys/filesys.h"
#include "threads/malloc.h"
#include "threads/thread.h"


/* return the cache block with block sector is SECTOR 
   return null if not found in cache
*/
struct cache_entry *get_block_in_cache(block_sector_t sector)
{
  struct cache_entry *cache;
  struct list_elem *e;
  e = list_begin(&filesys_cache);
  while( e != list_end(&filesys_cache)) {
    cache= list_entry(e, struct cache_entry, elem);
    if (cache->sector == sector){
      return cache;
    }
    e = list_next(e);
  }
  return NULL;
}
/* Initialize the cache , create a always-runnnin process
   to write the dirty cache back every 5 TIME FREQUENCY
*/
void cache_init(void)
{
  lock_init(&cache_lock);
  filesys_cache_size = 0; //frest init filesys cache size as zero.
  list_init(&filesys_cache);
  thread_create("filesys_cache_writeback", 0, write_cache_back_loop, NULL);
}


/* called by process, return the cache_entry with the sector number SECTOR*/
struct cache_entry *cache_get_block(block_sector_t sector, bool dirty){
  lock_acquire(&cache_lock);
  struct cache_entry *cache = get_block_in_cache(sector);
  if (cache){
    cache->open_cnt++;
    cache->dirty |= dirty;
    cache->referebce_bit = true;
    lock_release(&cache_lock);
    return cache;
  }
  cache = cache_replace(sector, dirty);
  if (!cache)
  {
    PANIC("Not enough memory for buffer cache.");
  }
  lock_release(&cache_lock);
  return cache;
}

/* Choose a cache block to be replaced, return the new cache block with
*  the data from disk on SECTOR.
*  If the cache list is not full, just insert the new cache block.
*  Otherwise choose a cache who is not opened to replace.
*/
struct cache_entry *cache_replace(block_sector_t sector,
                                              bool dirty)
{
  struct cache_entry *c;
  if (filesys_cache_size < MAX_FILESYS_CACHE_SIZE)
  {
    /* create a new cache */
    filesys_cache_size++;
    c = malloc(sizeof(struct cache_entry));
    if (!c)
    {
      return NULL;
    }
    c->open_cnt = 0;
    list_push_back(&filesys_cache, &c->elem);
  }
  else // find a cache to replace
  {
    c = find_replace();
  }
  c->open_cnt++;
  c->sector = sector;
  block_read(fs_device, c->sector, &c->block);
  c->dirty = dirty;
  c->referebce_bit= true;
  return c;
}

/* find cache in the list to be replaced 
   use clock algorithm */
struct cache_entry *find_replace()
{
  struct cache_entry *replace;
  while (true)
  {
    struct list_elem *e;
    for (e = list_begin(&filesys_cache); e != list_end(&filesys_cache);
         e = list_next(e))
    {
      replace = list_entry(e, struct cache_entry, elem);
      if (replace->open_cnt == 0)
      {
        if (replace->referebce_bit)
        {
          replace->referebce_bit= false;
        }
        else
        {
          if (replace->dirty)
          {
            block_write(fs_device, replace->sector, &replace->block);
          }
          return replace;
        }
      }
    }
  }
}

/*
scan the cache list, if the cache is dirty, write back to the disk
if IS_REMOVE is true, remove all the cache.  */
void cache_write_to_disk(bool is_remove){
  // ASSERT(is_remove);
  lock_acquire(&cache_lock);
  struct list_elem *next, *e = list_begin(&filesys_cache);
  while (e != list_end(&filesys_cache))
  {
    next = list_next(e);
    struct cache_entry *c = list_entry(e, struct cache_entry, elem);
    if (c->dirty)
    {
      block_write(fs_device, c->sector, &c->block);
      c->dirty = false;
    }
    if (is_remove)
    {
      list_remove(&c->elem);
      free(c);
    }
    e = next;
  }
  lock_release(&cache_lock);
}

/* execute write back dirty cache every 5 time in the sequnce. */
void write_cache_back_loop(void *aux UNUSED)
{
  while (true)
  {
    timer_sleep(WRITE_BACK_WAIT_TIME);
    cache_write_to_disk(false);
  }
}

/* Cache flash to disk, return the number of flash block*/
int test_cache(void) {
  int count= 0;
  // count.
  lock_acquire(&cache_lock);
  //in the cache flash, it's producer and consumer problem
  for (struct list_elem *next, *e = list_begin(&filesys_cache);e != list_end(&filesys_cache);e=list_next(e))
  {
    struct cache_entry *c = list_entry(e, struct cache_entry, elem);
    if (c->dirty)
    {
      block_write(fs_device, c->sector, &c->block);
      c->dirty = false;
      count++;
    }
  }
  lock_release(&cache_lock);
  //make this not bothered by other operations.
  return count;
}

#include "filesys/cache.h"
#include "filesys/filesys.h"
#include "threads/malloc.h"
#include "threads/thread.h"

/* Initialize the cache , create a always-runnnin process
   to write the dirty cache back every 5 TIME FREQUENCY
*/
void filesys_cache_init(void)
{
  list_init(&filesys_cache);
  lock_init(&filesys_cache_lock);
  filesys_cache_size = 0;
  thread_create("filesys_cache_writeback", 0, write_cache_back_loop, NULL);
}

/**  
 * Flushes the cache to disk.
 * */
void filesys_cache_flush(void) 
{
  filesys_cache_write_to_disk(true);
}

/* return the cache block with block sector is SECTOR 
   return null if not found in cache
*/
struct cache_entry *get_block_in_cache(block_sector_t sector)
{
  struct cache_entry *c;
  struct list_elem *e;
  for (e = list_begin(&filesys_cache); e != list_end(&filesys_cache);
       e = list_next(e))
  {
    c = list_entry(e, struct cache_entry, elem);
    if (c->sector == sector)
    {
      return c;
    }
  }
  return NULL;
}

/* called by process, return the cache_entry with the sector number SECTOR*/
struct cache_entry *filesys_cache_get_block(block_sector_t sector,
                                            bool dirty)
{
  lock_acquire(&filesys_cache_lock);
  struct cache_entry *c = get_block_in_cache(sector);
  if (c)
  {
    c->open_cnt++;
    c->dirty |= dirty;
    c->ref_bit = true;
    lock_release(&filesys_cache_lock);
    return c;
  }
  c = cache_replace(sector, dirty);
  if (!c)
  {
    PANIC("Not enough memory for buffer cache.");
  }
  lock_release(&filesys_cache_lock);
  return c;
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
  c->ref_bit = true;
  return c;
}

/* find cache in the list to be replaced 
   use clock algorithm */
struct cache_entry *find_replace()
{
  struct cache_entry *replace;
  while (1)
  {
    struct list_elem *e;
    for (e = list_begin(&filesys_cache); e != list_end(&filesys_cache);
         e = list_next(e))
    {
      replace = list_entry(e, struct cache_entry, elem);
      if (replace->open_cnt == 0)
      {
        if (replace->ref_bit)
        {
          replace->ref_bit = false;
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

/**
 * scan the cache list, if the cache is dirty, write back to the disk
 * if IS_REMOVE is true, remove all the cache. 
 * */
void filesys_cache_write_to_disk(bool is_remove)
{
  lock_acquire(&filesys_cache_lock);
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
  lock_release(&filesys_cache_lock);
}

/* execute write back dirty cache every 5 time frequence*/
void write_cache_back_loop(void *aux UNUSED)
{
  while (true)
  {
    timer_sleep(WRITE_BACK_WAIT_TIME);
    filesys_cache_write_to_disk(false);
  }
}

/* Cache flash to disk, return the number of flash block*/
int test_cache_flash(void) {
  int write_num = 0;

  lock_acquire(&filesys_cache_lock);
  
  struct list_elem *next, *e = list_begin(&filesys_cache);
  while (e != list_end(&filesys_cache))
  {
    next = list_next(e);
    struct cache_entry *c = list_entry(e, struct cache_entry, elem);
    if (c->dirty)
    {
      block_write(fs_device, c->sector, &c->block);
      c->dirty = false;
      write_num ++;
    }
    e = next;
  }
  lock_release(&filesys_cache_lock);
  return write_num;
}

/* Return the number of dirty cache */
int test_dirty_cache (void) {
    int write_num = 0;

  lock_acquire(&filesys_cache_lock);
  
  struct list_elem *next, *e = list_begin(&filesys_cache);
  while (e != list_end(&filesys_cache))
  {
    next = list_next(e);
    struct cache_entry *c = list_entry(e, struct cache_entry, elem);
    if (c->dirty)
    {
      write_num ++;
    }
    e = next;
  }
  lock_release(&filesys_cache_lock);
  return write_num;
}


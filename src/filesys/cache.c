#include "filesys/cache.h"
#include "filesys/filesys.h"
#include "threads/malloc.h"
#include "threads/thread.h"

/* get the cache block with the block sector and return NULL if not found in cache. */
struct cache_entry *cache_block(block_sector_t sector){
    struct cache_entry *cache;
    struct list_elem *e;

    e = list_begin(&cache_list);
    while(e != list_end(&cache_list)){
        cache =list_entry(e,struct cache_entry,elem);
        if (cache->sector != sector){
            e = list_next(e);
        } else{
            return cache;
        }
    }
    return (void*)(0);
}

/* Initiate the cache and create a watchdog process to check whether the cahce is written on a basis of 5 time frequency.*/
void cache_init(void){
    lock_init(&cache_lock);
    cache_size=0;
    list_init(&cache_list);
    thread_create("filesys_cache_writeback",0,write_cache_back_loop,(void*)(0));

}

/* Called by the cache_entry*/
struct cache_entry *cache_get_block(block_sector_t sector, int dirty){
    lock_acquire(&cache_lock);
    struct cache_entry *cache = cache_block(sector);
    if (cache){
        // cache->dirty = cache->dirty | dirty;
        cache->count++;
        if (dirty == 1)
            cache->dirty = 1;
        else if (cache->dirty == 1) 
            cache->dirty = 1;
        cache->reference_bit = 1;
        lock_release(&cache_block);
        return cache;

    }
    cache = cache_replace(sector, dirty);
    if (!cache){
        PANIC("OOM");
    }
    lock_release(&cache_lock);
    return cache;
}

/* Choose the cache block to be replcace by the needed sector number*/
struct cache_entry *cache_replace(block_sector_t sector, int dirty){
    struct cache_entry *cache;
    switch (cache_size >= 64){
        case 0:
            cache = malloc(sizeof(struct cache_entry));
            int state = cache;
            cache_size++;
            switch (state) {
            case 0:
                return NULL;
            case 1:
                cache->count = 0;
                list_push_back(&cache_list, &cache->elem);
            }
            break;
        case 1:
            struct cache_entry *replace;
            while (true) {
                struct list_elem *e = list_begin(&cache_list) ;
                while (e != list_end(&cache_list) ) {
                    replace = list_entry(e, struct cache_entry, elem);
                    if (replace->count == 0) {
                        if (replace->reference_bit) {
                            replace->reference_bit = false;
                        } else {
                            if (replace->dirty) {
                                block_write(fs_device, replace->sector, &replace->block);
                            }
                            cache = replace;
                        }
                    }
                    e = list_next(e);
                }
            }
        }
    cache->count++;
    cache->sector = sector;
    block_read(fs_device,cache->sector, &cache->block);
    cache->dirty = dirty;
    cache->reference_bit = 1;
    return cache;
}

void cache_write_disk(int is_removed){
    lock_acquire(&cache_lock);
    struct list_elem *e = list_begin(&cache_list);
    while (e != list_end(&cache_list)){
        struct cache_entry *cache = list_entry(e, struct cache_entry, elem);
        int cur_cache_state = cache->dirty*2+is_removed;
        switch (cur_cache_state){
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

void cache_back_loop (void *aux){
    aux=(int *)aux;
    while (1){
        timer_sleep(5*TIMER_FREQ);
        cache_write_disk(false);
    }
}

int cache_examine(void) {
    int count = 0;
    lock_acquire(&cache_lock);
    struct list_elem *e = list_begin(&cache_list);
    while(e!= list_end(&cache_list)){
        struct cache_entry *cache = list_entry(e,struct cache_entry,elem);
        int cache_state = cache->dirty * 2;
        switch(cache_state){
            case 2:
                block_write(fs_device,cache->sector, &cache->block);
                cache->dirty = 0;
                count++;

        }
        e=list_next(e);
    }
    lock_release(&cache_lock);
    return count;

}

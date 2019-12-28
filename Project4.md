# Project 4

## Group

- Yiwei Yang <yangyw@shanghaitech.edu.cn>
- Yuqing Yao <yaoyq@shanghaitech.edu.cn>

## Indexed and Extensible Files

### Data Structures
- `inode.c`
```cpp
#define MAX_FILE_SIZE 8460288 // in bytes
#define PTRS_PER_SECTOR 128 // how many sectors a block can point: 512 byte / 4 byte

struct bitmap;

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    // block_sector_t start;               /* First data sector. */
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
     
    block_sector_t pointers[TOTAL_POINTER_NUM];
    uint32_t level0_ptr_index;                  /* index of the pointer list */
    uint32_t level1_ptr_index;               /* index of the level 1 pointer table */
    uint32_t level2_ptr_index;               /* index of the level 2 pointer table */

    uint32_t is_file;                    /* 1 for file, 0 for dir */
    uint32_t not_used[122 - TOTAL_POINTER_NUM];
  };
  ```

### Algorithms

- `directory.c`

```cpp
  if (inode != NULL) 
    {
      struct dir_entry entries[2];

      memset (entries, 0, sizeof entries);

      /* "." entry. */
      entries[0].inode_sector = sector;
      strlcpy (entries[0].name, ".", sizeof entries[0].name);
      entries[0].in_use = true;

      /* ".." entry. */
      entries[1].inode_sector = parent_sector;
      strlcpy (entries[1].name, "..", sizeof entries[1].name);
      entries[1].in_use = true;
      
      if (inode_write_at (inode, entries, sizeof entries, 0) != sizeof entries)
        {
          inode_remove (inode);
          inode_close (inode); 
          inode = NULL;
        } 
    }
  ```
- many other related funtions are modified for subdirectories in `directory.c`
#### What is the maximum size of a file supported by your inode structure?  Show your work.

- The inode_disk holds 12 sector IDs.
- The indirect node holds 128 IDs.

```cpp
  #define PTRS_PER_SECTOR 128 
  ```
- The double indirect node holds 128 indirect nodes, which each hold
```cpp
  #define PTRS_PER_SECTOR 128 // how many sectors a block can point: 512 byte / 4 byte
  ```
- 128 sector IDs for a total of 128 * 128 = 16384. Overall this is (12 + 128 + 16384) * 512 = 8460288 Bytes.

```cpp
  #define MAX_FILE_SIZE 8460288 // in bytes
  ```

### Synchronization

#### Explain how your code avoids a race if two processes attempt to extend a file at the same time.

The query for sectors with specific offsets is atomic, and if no sectors exist, sectors are allocated. This means that if two threads try to write the same offset beyond the end of the file, they will be assigned a sector. We only need to lock the entire byte to sector function based on inode lock to achieve this. If the logical length is less than the offset of the last byte written, we also lock the statement that updates the inode length after the end of the file. Therefore, under competitive conditions, the larger length will remain unchanged after all threads are completed.

#### Suppose processes A and B both have file F open, both positioned at end-of-file.  If A reads and B writes F at the same time, A may read all, part, or none of what B writes.  However, A may not read data other than what B writes, e.g. if B writes nonzero data, A is not allowed to see all zeros.  Explain how your code avoids this race.

Because the length field is updated only after the entire extension is written, if A attempts to read while B is writing, it will not see any data in our implementation. If A tries to read after B has finished writing, it will see the entire extension.

#### Explain how your synchronization design provides "fairness". File access is "fair" if readers cannot indefinitely block writers or vice versa.  That is, many processes reading from a file cannot prevent forever another process from writing the file, and many processes writing to a file cannot prevent another process forever from reading the file.

At every `file_write` or `file_read`, the porcess is always requring the lock. And because the lock may only take one block each, and no other process will control the lock longer than one block. When the lock is released, the turn continues. As it's like the Reader and Writer problem, and no additional condition are added, so it's fair.
### Rationale

#### Is your inode structure a multilevel index?  If so, why did you choose this particular combination of direct, indirect, and doubly indirect blocks?  If not, why did you choose an alternative inode structure, and what advantages and disadvantages does your structure have, compared to a multilevel index?

This is a multilevel index with 12 single level blocks, one indirect block, and one double indirect block. We chose a combination because it is the same as FFS, and it works well for the required file size. For most tests that do not use double intirect blocks, using indirect blocks is slightly faster.

## Subdirectories

### Data Structures

- `struct inode()`

  In-memory inode.

  - `struct list_elem elem`         

       Element in inode list. 

  - `block_sector_t sector`              

       Sector number of disk location. 

  - `int open_cnt`

       Number of openers.

  - `bool removed`

       True if deleted, false otherwise.

  - `int deny_write_cnt`

       0: writes ok, >0: deny writes. 

  
  - `struct inode_disk data`

      Inode content.  
        
  -  `struct lock extend_lock`

      To implement the extendability lock
  
  -  `off_t length`
      
       File size in bytes.

  -  `off_t length_for_read`             
        
      Calculate the File size in bytes. 
  

### Algorithms

- `struct inode * inode_open (block_sector_t sector)`

    Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails.

- `off_t inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset)`

    Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. 

- `off_t inode_write_at (struct inode *inode, const void *buffer_, off_t size, off_t offset) `

    Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.

- `void inode_deny_write (struct inode *inode) && void inode_allow_write (struct inode *inode) `

    Disable & Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode.

- `static size_t bytes_to_indirect_sectors(off_t size) ...`

    The implementation of the indirected sectors.
### Synchronization

#### How do you prevent races on directory entries?  For example, only one of two simultaneous attempts to remove a single file should succeed, as should only one of two simultaneous attempts to create a file with the same name, and so on.

Each directory has a lock. Any changes made to the directory will be first acquired by a lock. This implementation make the simultaneously modifying a file through creating or deleting files impossible.

#### Does your implementation allow a directory to be removed if it is open by a process or if it is in use as a process's current working directory?  If so, what happens to that process's future file system operations?  If not, how do you prevent it?

No. The simple assert is that if a number that specifies how many programs have the directory open is greater than 0, it's obviously invalid deletion. so just deny it.

### Rationale

#### Explain why you chose to represent the current directory of a process the way you did.

We proposed another number to record the current directory of process as a char array. That makes the hanling complexity in the directory part is rather easier. And after that, we do the parsing part.

## Buffer Cache

### Data Structures
```cpp
MAX_FILESYS_CACHE_SIZE 64   
```
- `cache.c`
```cpp
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
```


### Algorithms
- `void cache_init (void);`

Initialize the cache , create a always-runnnin process to write the dirty cache back every 5 TIME FREQUENCY
```cpp
void cache_init(void)
{
  lock_init(&cache_lock);
  filesys_cache_size = 0; //frest init filesys cache size as zero.
  list_init(&filesys_cache);
  thread_create("filesys_cache_writeback", 0, write_cache_back_loop, NULL);
}
```
- `struct cache_entry *cache_get_block(block_sector_t sector, bool dirty)`

called by process, return the cache_entry with the sector number SECTOR

- `struct cache_entry *cache_replace(block_sector_t sector, bool dirty)`

Choose a cache block to be replaced, return the new cache block with the data from disk on SECTOR. If the cache list is not full, just insert the new cache block. Otherwise choose a cache who is not opened to replace.

- `struct cache_entry *find_replace() && void cache_write_to_disk(bool is_remove) && void write_cache_back_loop(void *aux UNUSED) && int test_cache(void)`

Some tools to execute write dealt with unconditional situations



#### Describe how your cache replacement algorithm chooses a cache block to evict.

I modified the clock algorithm with 1 hand to decide how to replace cache blocks. The cache is only 64 entries large, which is small enough that there is no real performance advantage to having a second hand; this also simplifies the code.

It is unsafe to evict a block while other processes are reading from it or writing to it. So the buffer cache entry has a count of "users" (readers/writers). If this is not zero, my algorithm skips over this entry (leaving it marked as accessed). If it is zero, then it is marked as not accessed. Once the clock hand sees a block that is marked not accessed, that block will be immediately evicted and its index returned to the caller.

If all blocks are currently being accessed, then no block can be evicted. In this case, the evicting process will sleep and try again. 

####  Describe your implementation of write-behind.

I spawn a new thread that runs the auto_save function. This calls timer
sleep and cache_flush for its entire life.

####  Describe your implementation of read-ahead.

Any time I do a read or write, I spawn a new thread that gets the next
sector of the sector that was just accessed. 

### Synchronization

#### When one process is actively reading or writing data in a buffer cache block, how are other processes prevented from evicting that block?


Each cache entry has a number that specifies how many threads are 
currently using it. If this number > 0 then the entry will not be
evicted.



#### During the eviction of a block from the cache, how are other processes prevented from attempting to access the block?

As soon as the entry is chosen for eviction it is marked as not valid,
thus, not other threads will be able to access it.


### Rationale

#### Describe a file workload likely to benefit from buffer caching, and workloads likely to benefit from read-ahead and write-behind.
A workload likely to benefit from buffer caching would be reading to writing from one or a few blocks frequently. For example, if it is writing/reading often
from the same parts of a file. Read-ahead will benefit those which
read through an entire file sequentially. Write-behind will cut down on
writes for programs which write to the same sector often.

# Project 4

## Group

- Yiwei Yang <yangyw@shanghaitech.edu.cn>
- Yuqing Yao <yaoyq@shanghaitech.edu.cn>

## Indexed and Extensible Files

### Data Structures




#### What is the maximum size of a file supported by your inode structure?  Show your work.

The inode_disk holds 12 sector IDs.
The indirect node holds 128 IDs.
The double indirect node holds 128 indirect nodes, which each hold
128 sector IDs for a total of 128 * 128 = 16384. 
In total this is (12 + 128 + 16384) * 512 = 8460288 Bytes.
This is just over 8 MB.

### Synchronization

#### A3: Explain how your code avoids a race if two processes attempt to extend a file at the same time.

#### A4: Suppose processes A and B both have file F open, both positioned at end-of-file.  If A reads and B writes F at the same time, A may read all, part, or none of what B writes.  However, A may not read data other than what B writes, e.g. if B writes nonzero data, A is not allowed to see all zeros.  Explain how your code avoids this race.

#### A5: Explain how your synchronization design provides "fairness". File access is "fair" if readers cannot indefinitely block writers or vice versa.  That is, many processes reading from a file cannot prevent forever another process from writing the file, and many processes writing to a file cannot prevent another process forever from reading the file.

### Rationale

#### A6: Is your inode structure a multilevel index?  If so, why did you choose this particular combination of direct, indirect, and doubly indirect blocks?  If not, why did you choose an alternative inode structure, and what advantages and disadvantages does your structure have, compared to a multilevel index?

1 double indirect block is enough to address 8mb (because 128 references * 128 references = 16k = 8mb / 512b). 80-20 rule says 100 direct, 25 indirect, and the 1 double indirect.

## Subdirectories

### Data Structures

#### B1: Copy here the declaration of each new or changed `struct' or `struct' member, global or static variable, `typedef', or enumeration.  Identify the purpose of each in 25 words or less.

### Algorithms

#### B2: Describe your code for traversing a user-specified path.  How do traversals of absolute and relative paths differ?

### Synchronization

#### B4: How do you prevent races on directory entries?  For example, only one of two simultaneous attempts to remove a single file should succeed, as should only one of two simultaneous attempts to create a file with the same name, and so on.

#### B5: Does your implementation allow a directory to be removed if it is open by a process or if it is in use as a process's current working directory?  If so, what happens to that process's future file system operations?  If not, how do you prevent it?

### Rationale

#### B6: Explain why you chose to represent the current directory of a process the way you did.

## Buffer Cache

### Data Structures

- MAX_FILESYS_CACHE_SIZE 64   


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
};```




### Algorithms

#### C2: Describe how your cache replacement algorithm chooses a cache block to evict.

I modified the clock algorithm with 1 hand to decide how to replace cache blocks. The cache is only 64 entries large, which is small enough that there is no real performance advantage to having a second hand; this also simplifies the code.

It is unsafe to evict a block while other processes are reading from it or writing to it. So the buffer cache entry has a count of "users" (readers/writers). If this is not zero, my algorithm skips over this entry (leaving it marked as accessed). If it is zero, then it is marked as not accessed. Once the clock hand sees a block that is marked not accessed, that block will be immediately evicted and its index returned to the caller.

If all blocks are currently being accessed, then no block can be evicted. In this case, the evicting process will sleep and try again. (It will take 2 full clock hand sweeps to know whether this is the case.)

#### C3: Describe your implementation of write-behind.

We use the alarm clock algorithm.

#### C4: Describe your implementation of read-ahead.

### Synchronization

#### C5: When one process is actively reading or writing data in a buffer cache block, how are other processes prevented from evicting that block?


For every reader or writer, the "user" count is incremented by one. As long as the user count is positive, my modified clock algorithm prevents that block from being evicted.


#### C6: During the eviction of a block from the cache, how are other processes prevented from attempting to access the block?


Each buffer cache entry also has a "valid" flag, which indicates whether the contents of that buffer cache entry are valid or not. As the first step of evicting a block, the valid flag is set to false, and the cache_lookup algorithm treats this as a sector not existing there. When a block is read into a cache entry, the valid flag is the last thing set; as long as the valid flag is true, the contents of the buffer cache entry are consistent. This will prevent processes from trying to access the block while it is being evicted.


### Rationale

#### C7: Describe a file workload likely to benefit from buffer caching, and workloads likely to benefit from read-ahead and write-behind.
A workload likely to benefit from buffer caching would be reading to writing from one or a few blocks frequently. An example workload is breaking up a data file into several files, one for each of several repeated keys. (For example, given a list of hospitals in the US and their location, you want to break the list into a list of hospitals for each state in the US.) Then for each record in the data file, you would need to decide which of several blocks to write to, then go back to the data file. In this case, it would be better to have the blocks available in the buffer cache than to frequently read and write to disk directly.

A workload that would likely benefit from read-ahead and write-behind is a merge of two columns of a very long CSV file. Since the CSV is very long, it spans multiple disk sectors. Since the merges happen sequentially on rows of the CSV, it would save time read-ahead rather than look up the next block at the time it was needed, which will definitely happen. At the same time, because previous blocks of the CSV are not being picked up immediately by a subsequent process, it is okay if there is a slight delay between writing to the buffer cache and writing to the disk, and write-behind saves time by not blocking execution.

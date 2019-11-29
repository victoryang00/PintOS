# Project 3: Virtual Memory

## Group 20

- Yiwei Yang <yangyw@shanghaitech.edu.cn>
- Yuqing Yao <yaoyq@shanghaitech.edu.cn>

## Page Table Management

### Data Structures

### Algorithms

#### In a few paragraphs, describe your code for accessing the data stored in the SPT about a given page.

Each page struct has a number of associated members, including the frame struct that contains its physical data. The frame struct contains a pointer to the kernel virtual address holding its data, and a reference to the page that owns it. When the page is initially created, its frame is set to NULL -- it doesn't receive a frame until allocated one via the `frame_alloc_and_lock()` function in 'frame.c' (called by the `do_page_in()` function).

The process of finding a free frame in memory is conducted by `frame_alloc_and_lock()`. It makes multiple attempts to secure a free region of memory in which to allocate the new frame. If no frame sized piece of memory exists, then an existing frame must be evicted to make room for the new one. Upon finding/creating a new frame, the frame is returned and associated with the page that requested it (`p->frame = frame` and `f->page = page`). If for some reason `frame_alloc_and_lock()` is unable to find an existing frame to evict, `NULL` is returned and no frame is allocated.

#### How does your code coordinate accessed and dirty bits between kernel and user virtual addresses that alias a single frame, or alternatively how do you avoid the issue?

We avoid this issue by only accessing the virtual address.

### Synchronization

#### When two user processes both need a new frame at the same time, how are races avoided?

Searching into the frame table (usually to find a free frame) is limited to a single process at a time via a lock called `scan_lock`. No two processes can secure the same frame at once, and race conditions are avoided. Additionally, each individual frame contains its own lock (`f->lock`) denoting whether or not it is occupied.

### Rationale

#### Why did you choose the data structure(s) that you did for representing virtual-to-physical mappings?

We use a hash map because it allows an $O(1)$ and space-efficient method for managing the mapped pages of each process. We need to support fast lookups in the mapping, so an $O(1)$ algorithm is necessary and satisfying.

## Paging to and from Disk

### Data Structures

### Algorithms

#### When a frame is required but none is free, some frame must be evicted.  Describe your code for choosing a frame to evict.

The least recently used one. Algorithm implemented in `try_frame_alloc_and_lock()` in `frame.c`.

If the frame being searched for has no page associated with it then we immediately acquire that frame. Otherwise, we acquire the first frame that has not been accessed recently. If all of the frame have been accessed recently, then we iterate over each of the frames again. At this time, it is very likely that a valid frame will be acquired because the `page_accessed_recently()` function changes the access status of a frame upon being called. If for whatwver reason the second iteration yields no valid frames, the `NULL` is returned and no frame is evicted.

#### When a process P obtains a frame that was previously used by a process Q, how do you adjust the page table (and any other data structures) to reflect the frame Q no longer has?

When P obtains a frame that was used by Q, we first pin the frame, acquire the lock for the supplemental page table entry associated with that page, and then remove it from process Q's page table. This means that process Q will fault upon any success to this frame from now, nut it will have to block on acquiring the supplemental page table entry lock before unevicting its frame.

Depending on the property of Q, it will be written to disk or swap.

#### Explain your heuristic for deciding whether a page fault for an invalid virtual address should cause the stack to be extended into the page that faulted.

There are two important checks that must be made before a page is allocated.

1. the address of the page (rounded down to the nearest page boundary) must be within the allocated stack space (which is by default 1 MB).
2. The page address (unrounded) must be within 32 Bytes of the threads' `user_esp`. We do this to account for commands that manage stack memory, including the PUSH and PUSHA commnds that will access at most 32 bytes beyond the stack pointer.

### Synchronization

#### Explain the basics of your VM synchronization design.  In particular, explain how it prevents deadlock.  (Refer to the textbook for an explanation of the necessary conditions for deadlock.)

There is an internal lock for frame table and swap table. For supplemental page table, it might be used by other process during eviction, so to avoid confusion and allow synchronization, we add a lock to each supplemental page table entry.

These three parts 

#### A page fault in process P can cause another process Q's frame to be evicted.  How do you ensure that Q cannot access or modify the page during the eviction process?  How do you avoid a race between P evicting Q's frame and Q faulting the page back in?

#### Suppose a page fault in process P causes a page to be read from the file system or swap.  How do you ensure that a second process Q cannot interfere by e.g. attempting to evict the frame while it is still being read in?

#### Explain how you handle access to paged-out pages that occur during system calls.  Do you use page faults to bring in pages (as in user programs), or do you have a mechanism for "locking" frames into physical memory, or do you use some other design?  How do you gracefully handle attempted accesses to invalid virtual addresses?

### Rationale

#### A single lock for the whole VM system would make synchronization easy, but limit parallelism.  On the other hand, using many locks complicates synchronization and raises the possibility for deadlock but allows for high parallelism.  Explain where your design falls along this continuum and why you chose to design it this way.

## Memory Mapped Files

### Data Structures

### Algorithms

#### Describe how memory mapped files integrate into your virtual memory subsystem.  Explain how the page fault and eviction processes differ between swap pages and other pages.

Memory mapped files are encapsulated in a struct called `mapping` in `syscall.c`. Each thread contains a list of all of the files mapped to that thread, which can be used to manage which files are present directly in memory. Otherwise, the pages containing memory mapped file information are managed just the same as any other page.

The page fault and eviction process differs slightly for pages belonging to memory mapped files. Non-file related pagesare moved to a swap partition upon eviction, regardless of whether or not the page is dirty. When evicted, memory mapped file pages must only be written back to the file if modified. Otherwise, no writing is necessary -- the swap partition is avoided all together for memory mapped files.

#### Explain how you determine whether a new file mapping overlaps another segment, either at the time the mapping is created or later.

Pages for a new file mapping are only allocated if pages are found that are free and unmapped. The `page_allocated()` function has access to existing file mappings, and will refuse to allocate any space that is already occupied. If a new file attemps to infringe upon already mapped space, it is immediately unmapped and the process fails.

### Rationale

#### Mappings created with "mmap" have similar semantics to those of data demand-paged from executables, except that "mmap" mappings are written back to their original files, not to swap.  This implies that much of their implementation can be shared.  Explain why your implementation either does or does not share much of the code for the two situations.

The code is largely shared between processes. Any page, regardless of origin, will ultimately be pages out via the same `page_out()` function in `page.c`. The only difference is a check to see whether or not the page should be written back out of disk. If the page is marked as private then it should be swapped to the swap partition, otherwise it should be written out to the file on the disk. This makes it easier than writing separately for different page types.

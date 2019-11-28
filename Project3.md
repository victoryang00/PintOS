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



### Synchronization

#### When two user processes both need a new frame at the same time, how are races avoided?

Searching into the frame table (usually to find a free frame) is limited to a single process at a time via a lock called `scan_lock`. No two processes can secure the same frame at once, and race conditions are avoided. Additionally, each individual frame contains its own lock (`f->lock`) denoting whether or not it is occupied.

### Rationale

#### Why did you choose the data structure(s) that you did for representing virtual-to-physical mappings?

## Paging to and from Disk

### Data Structures

### Algorithms

#### When a frame is required but none is free, some frame must be evicted.  Describe your code for choosing a frame to evict.

The least recently used one. Algorithm implemented in `try_frame_alloc_and_lock()` in `frame.c`.

If the frame being searched for has no page associated with it then we immediately acquire that frame. Otherwise, we acquire the first frame that has not been accessed recently. If all of the frame have been accessed recently, then we iterate over each of the frames again. At this time, it is very likely that a valid frame will be acquired because the `page_accessed_recently()` function changes the access status of a frame upon being called. If for whatwver reason the second iteration yields no valid frames, the `NULL` is returned and no frame is evicted.

#### When a process P obtains a frame that was previously used by a process Q, how do you adjust the page table (and any other data structures) to reflect the frame Q no longer has?

#### Explain your heuristic for deciding whether a page fault for an invalid virtual address should cause the stack to be extended into the page that faulted.

There are two important checks that must be made before a page is allocated.

1. the address of the page (rounded down to the nearest page boundary) must be within the allocated stack space (which is by default 1 MB).
2. The page address (unrounded) must be within 32 Bytes of the threads' `user_esp`. We do this to account for commands that manage stack memory, including the PUSH and PUSHA commnds that will access at most 32 bytes beyond the stack pointer.

### Synchronization

#### Explain the basics of your VM synchronization design.  In particular, explain how it prevents deadlock.  (Refer to the textbook for an explanation of the necessary conditions for deadlock.)

#### A page fault in process P can cause another process Q's frame to be evicted.  How do you ensure that Q cannot access or modify the page during the eviction process?  How do you avoid a race between P evicting Q's frame and Q faulting the page back in?

#### Suppose a page fault in process P causes a page to be read from the file system or swap.  How do you ensure that a second process Q cannot interfere by e.g. attempting to evict the frame while it is still being read in?

#### Explain how you handle access to paged-out pages that occur during system calls.  Do you use page faults to bring in pages (as in user programs), or do you have a mechanism for "locking" frames into physical memory, or do you use some other design?  How do you gracefully handle attempted accesses to invalid virtual addresses?

### Rationale

#### A single lock for the whole VM system would make synchronization easy, but limit parallelism.  On the other hand, using many locks complicates synchronization and raises the possibility for deadlock but allows for high parallelism.  Explain where your design falls along this continuum and why you chose to design it this way.

## Memory Mapped Files

### Data Structures

### Algorithms

#### Describe how memory mapped files integrate into your virtual memory subsystem.  Explain how the page fault and eviction processes differ between swap pages and other pages.

#### Explain how you determine whether a new file mapping overlaps another segment, either at the time the mapping is created or later.

### Rationale

#### Mappings created with "mmap" have similar semantics to those of data demand-paged from executables, except that "mmap" mappings are written back to their original files, not to swap.  This implies that much of their implementation can be shared.  Explain why your implementation either does or does not share much of the code for the two situations.


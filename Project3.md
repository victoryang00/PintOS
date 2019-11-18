# Project 3: Virtual Memory

## Group 20

- Yiwei Yang <yangyw@shanghaitech.edu.cn>
- Yuqing Yao <yaoyq@shanghaitech.edu.cn>

## Page Table Management

### Data Structures

### Algorithms

#### In a few paragraphs, describe your code for accessing the data stored in the SPT about a given page.

#### How does your code coordinate accessed and dirty bits between kernel and user virtual addresses that alias a single frame, or alternatively how do you avoid the issue?

### Synchronization

#### When two user processes both need a new frame at the same time, how are races avoided?

### Rationale

#### Why did you choose the data structure(s) that you did for representing virtual-to-physical mappings?

## Paging to and from Disk

### Data Structures

### Algorithms

#### When a frame is required but none is free, some frame must be evicted.  Describe your code for choosing a frame to evict.

#### When a process P obtains a frame that was previously used by a process Q, how do you adjust the page table (and any other data structures) to reflect the frame Q no longer has?

#### Explain your heuristic for deciding whether a page fault for an invalid virtual address should cause the stack to be extended into the page that faulted.

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


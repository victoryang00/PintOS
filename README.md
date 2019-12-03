# Introduction
Pintos is a simple operating system framework for the 80x86 architecture. It supports kernel threads, loading and running user programs, and a file system, but it implements all of these in a very simple way. In the Pintos projects, you and your project team will strengthen its support in all three of these areas. You will also add a virtual memory implementation.
# Composition
1. Thread
2. User Programs
3. Virtual Memory
4. File System

# CS130 Project 1: Threads

## Group 20 Members

- Yuqing Yao yaoyq@shanghaitech.edu.cn
- Yiwei Yang yangyw@shanghaitech.edu.cn

## Task 1: Alarm Clock

### Data Structure

#### Edited Method

##### `timer_sleep()`

First, it is necessary to check the legality of the input parameters. The input parameter ticks must be greater than 0 to make sense. The second is to change it to non-busy waiting. After reading the above content, the idea is already clear, just call thread_sleep(). Throw the current thread into the sleep_list queue.

##### `timer_interrupt()`

When each ticks interrupt arrives, the thread in sleep_list needs to be updated, otherwise the thread in sleep will never be woken up.

##### `thread_init()`

Initialize sleep_list.

##### `next_thread_to_run()`

The function originally selected the first thread in the ready queue to execute. Now we use the list_max() provided in the library function to select the thread with the highest priority.

#### Edited structs

##### `struct thread`

- `struct list_elem slpelem`
The element in the `sleep_list`.
- `int64_t sleep_ticks`
The time to wait.

#### New global variables

##### `static int load_avg`

Set average parameter for loading.

##### `static struct list sleep_list`

Add a statistical variable of thread to manage the sleeping threads.

#### New functions

##### `void thread_sleep(int64_t ticks)`

`ticks` is from `lib/kernel/timer.c` function `timer_sleep` which requires the ticks(the edge between start and now).

##### `void thread_foreach_sleep (void)`

A sleep version of foreach is need to be defined differently from the general one.

##### `bool thread_less_priority (const struct list_elem *compare1,const struct list_elem *compare2,void *aux UNUSED)`

If `compare2` is greater, then return true. `UNUSED` is a state to see whether is used.

### Algorithm

#### Briefly describe what happens in a call to `timer_sleep()`, including the effects of the timer interrupt handler.

Every time a thread needs to go to sleep, the `timer_sleep()` is called. It is put to the list of sleeping threads ordered by wakeup time. Basicly, the timer interrupt is known as tick. It means that we should check the list every 'tick' to update the list of sleeping threads and check whether to wake them up.

#### What steps are taken to minimize the amount of time spent in the timer interrupt handler?

Check every tick:

- Check the first element in `sleep_list`.
- If `sleep_ticks == 0`, pop the element out and push into the `ready_list`.
- Repeat for every element in the `sleep_list`.

### Synchronization

#### How are race conditions avoided when multiple threads call `timer_sleep()` simultaneously?

When we deal with threads that are going to sleep, the operations are all atomic because we disable the interrupts. Therefore, there are no race conditions between threads.

#### How are race conditions avoided when a timer interrupt occurs during a call to `timer_sleep()`?

The `sleep_list` is manipulated by the time interrupt handler. Since the interrupts are disabled during the call to `timer_sleep()`, so the race conditions are avoided.

### Rationale

#### Why did you choose this design? In what ways is it superior to another design you considered?

We first looked up the ***Modern Operating System***, there's a implementation of Blocking Thread.(A thread pauses during execution to wait for a condition to fire.) We also got a lot from the ***thread graph*** int the ppt. Our concept is very similar to the Blocking Thread.

 As for the superirity:
1. In the implementation part, we first add `sleep_list` queue using list with some necessary variables. Because in Pintos, there's already a usable list struct. For precisely control the thread in the thread list, we just implement a check (constant time) every tick. 
2. Insdead of single-functional `timer_sleep`, we rewrote thread_sleep, a function to put the curr thread into the sleep_list queue, and update the thread information, and finally call schedule() for new thread scheduling.
3. The multiple calls of `thread_less_priority` function fully applied the essense of ***High cohesion low coupling*** in C.

## Task 2: Priority Scheduling

### Data Structures

#### Edited Method(Locks Part)

##### `lock_init()`

Initialize the lock.

##### `init_thread()`

Initialize the member variables just defined in the struct.

##### `lock_acquire()`

Implement priority donation: first determine whether the lock is occupied. If it is occupied, it will make a priority donation and throw the current thread into the blocking queue; if it is not occupied, it will acquire the lock and update the relevant status information.

##### `lock_try_acquire()`

Achieve priority donation: The idea is the same as above, except that when the lock is occupied, it will not enter the blocking state, but return false.

##### `lock_release()`

The priority operation is restored when the lock is released.

##### `thread_create()`

In thread scheduling, there is a thread that requires the current execution to have the highest priority among all threads, so priority scheduling needs to be performed where the thread priority changes.

##### `thread_set_priority()`

Because this function will also modify the priority of the thread, but need to pay attention to the modification of the thread's basic priority, so you need to add additional judgment for priority update.

#### Edited Method(Semaphore Part)

##### `sema_up()`

Ensure that each time the semaphore increases, the thread with the highest priority in the queue is selected.

##### `cond_init()` & `cond_wait()` & `cond_signal()` & `cond_broadcast()`

Use semaphore to implement cond.

#### Edited struct

##### `struct thread`

- `struct lock *lock_waiting`
  A struct lists the locks that are waiting.
- `struct list locks`
  Locks owned by the thread.
- `int locks_priority`
  the top priority in the thread.
- `int base_priority`
  the priority right now.

#### New functions

##### `void thread_priority_donate_nest(struct thread *t)`

To update the priority of a thread with dependencies in a thread tree.

##### `void thread_priority(struct thread *t)`

To update the priority of a thread in a thread tree.

##### `void lock_priority_update(struct lock *l)`

To update the priority of a lock.

#### Explain the data structure used to track priority donation. Use ASCII art to diagram a nested donation.  (Alternately, submit a .png file.)

The data structure used to track priority donation is `struct lock`.

Firstly, get the curr thread's lock to `l`, which should be a list of locks. Then, traverse through the curr thread's lock to see whether to donate.

```
    +--------+   lock    +------+
    | thread |  ------>  |   l  |
    +--------+           +------+
        |                    |
        |                    |
     priority      >     priority   =>  donate from thread to l.
```

During the loop, then, get the lock thread to `l`.

```
    +--------+           +------+
    | thread |           |   l  |
    +--------+           +------+
        |                    |
        |                    |
  locks_priority   <     priority   =>  donate from l to thread.
```

In case the loop is applied just after its defination and its original priority is very high, we need a special judge after above two operations.

```
    +--------+           +------+
    | thread |  ------>  |   l  |
    +--------+           +------+
        |                    |
        |                    |
  locks_priority   >     priority   =>  donate from thread to l.
```
If none of above meet the requirement, just set state wait.

### Algorithm

#### How do you ensure that the highest priority thread waiting for a lock, semaphore, or condition variable wakes up first?

It's schedule() in the function next_thread_to_run() maintain the highest priority. Other than choosing the first element in the ready queue we check all of them and keep track of the lowest priority. We call our method thread_less_priority() to get the effective priority, because this method takes priority donation into consideration.

#### Describe the sequence of events when a call to `lock_acquire()` causes a priority donation.  How is nested donation handled?

First, we must check if priority donation is enabled (by checking the bool for mlfqs). When a call causes the priority donation, it first look at the curr lock. If the curr lock holder has a lower priority, update lock holder's locks to match the waiting thread's(in `thread_priority_donate_nest` also delt with the edge condition), and keep doing this up chain of locks while the holder pointer is not NULL. (recursive donation)

#### Describe the sequence of events when is called on a lock that a higher-priority thread is waiting for.

When a thread releases a lock, we take the lock off of holder. We then loop through the remaining holder and get the maximum priority element and set that to donated_priority. If no locks held, set to min. Check if our curr effective priority is still highest in ready queue and yield if not.

### Synchronization

#### Describe a potential race in thread_set_priority() and explain how your implementation avoids it.  Can you use a lock to avoid this race?

In `thread_set_priority(new_priority)`, we check the base_priority and immediately yield if there is a higher priority thread. Otherwise, two or more waiting threads will get the wrong value from it, and thus disturb the `thread_get_priority()`. In theory, it can be avoided by lock. In implementation, you should introduce the lock around all code that interacts with variables used
in `thread_set_priority`. However, that locks modifies the list of locks that a thread holds. This may result in deadlock because the lock code would call itself in some cases. 

### Rationale

#### Why did you choose this design? In what ways is it superior to another design you considered?

Because the original implementation is to treat lock as a semaphore with a value of 1, in order to achieve priority donation, you need to use the waiting queue, but it is very troublesome to wrap a layer of structure, so the lock is done with large changes.

As for the surperity:

1. We record the current thread's base priority so that the original priority can be restored when the donation ends; we record the locks that the thread already holds so that when needed (when a lock is acquired or released), To determine the priority of the new donation; you also need to record the lock that the thread is waiting for, so that when a nested donation occurs, we can continue to find the next donated object based on this variable.
2. It is necessary to record which thread the lock holder is, so that when a nested donation occurs, the next donated object can be continuously found according to this variable; the thread waiting for the lock needs to be recorded, so that when the lock is released, The holder of the next lock is selected from it; the priority of the current lock needs to be recorded to facilitate the implementation of the priority donation.

## Task 3: Advanced Scheduler

### Data Structures

#### Edited Methods

##### `lock_acquire()` & `lock_try_acquire()` & `lock_release()`

It is prohibited to modify the thread priority when performing the mlfqs test.

#### Edited Structs

##### `int nice`

The parameter in the cpu equation.

##### `int recent_cpu`

The float emulated by integet.

#### New Functions

##### `void thread_increase_recent_cpu(void)`

Update the `recent_cpu` every tick.

##### `void thread_increase_recent_cpu(void)`

Update the global variable `load_avg` every second.

##### `void thread_recalculate_recent_cpu(struct thread *t,void *)`

Recalculate the `recent_cpu` of every thread.

##### `void thread_recalculate_priority(struct thread *t,void *)`

Recalculate the priority of every thread every 4 ticks.

### Algorithm

#### Suppose threads A, B, and C have nice values 0, 1, and 2. Each has a recent_cpu value of 0.  Fill in the table below showing the scheduling decision and the priority and recent_cpu values for each thread after each given number of timer ticks:


timer ticks | R(A) | R(B) | R(C) | P(A) | P(B) | P(C) | thread to run
------------|------|------|------|------|------|------|--------------
0 |   0.0 |   0.0 |   0.0 |  63.0 | 61.0 |  59.0 | A
4 |   4.0 |   1.0 |   2.0 |  62.0 | 60.75 |  58.5 | A
8 |   8.0 |   1.0 |   2.0 |  61.0 | 60.75 |  58.5 | A
12 |  12.0 |   1.0 |   2.0 |  60.0 | 60.75 |  58.5 | B
16 |  12.0 |   5.0 |   2.0 |  60.0 | 59.75 |  58.5 | A
20 | 2.363 | 1.454 | 2.181 | 62.40 | 60.63 | 58.45 | A
24 | 6.363 | 1.454 | 2.181 | 61.40 | 60.63 | 58.45 | A
28 | 10.36 | 1.454 | 2.181 | 60.40 | 60.63 | 58.45 | B
32 | 10.36 | 5.454 | 2.181 | 60.40 | 59.63 | 58.45 | A
36 | 14.36 | 5.454 | 2.181 | 59.40 | 59.63 | 58.45 | B


#### Did any ambiguities in the scheduler specification make values in the table uncertain?  If so, what rule did you use to resolve them?  Does this match the behavior of your scheduler?

1. The specification is not clear about whether we should yield when the current thread's priority becomes equal to the priority of another thread. In both the table and our implementation, we do not yield in this case.
2. The specification did not specify whether recent_cpu is to be updated before or after updating the priorities. In both the table and our implementation, we update recent_cpu before updating the priority.

#### How is the way you divided the cost of scheduling between code inside and outside interrupt context likely to affect performance?

The only code that potentially runs outside an interrupt context involving selecting the next thread to run, which is at most a small constant number of operations. This means that a thread can not lose time due To scheduler bookkeeping. This prevents the scheduler from stealing time from a thread.

### Rationale

#### Briefly critique your design, pointing out advantages and disadvantages in your design choices.  If you were to have extra time to work on this part of the project, how might you choose to refine or improve your design?

This test set requires multi-level queue feedback scheduling. The specific calculation formulas have been given in great detail. How to use integer arithmetic to simulate floating-point operations is also described in the appendix.

Advantages: We use queue to select thread. Although it is called a multi-level queue, it is still necessary to select the thread with the highest priority every time, so as long as the thread's priority range is correct, it is enough to use a queue.

Disadvantages: Although the guide says that the priority test set does not conflict with the implementation of the mlfqs test set, in fact there is a call to lock in this part, so in order to be able to pass all, you need to modify the original lock operation.

If we have enough time, we might write a queue struct together with list to raise the system performance rather than the general extendibility. Also, to make the code more elegant, we can just write multiple-used module into function.

# CS130 Project 2: User Program

## Group 20 Members

- Yuqing Yao yaoyq@shanghaitech.edu.cn
- Yiwei Yang yangyw@shanghaitech.edu.cn

## Task 1: Argument Passing

### Data Structure 

#### `thread.h`

- struct file *code_file : Use for store the executable code file.

#### `process.c`

- process_execute (const char *file_name)

- load (const char *file_name, void (**eip) (void), void **esp)

- setup_stack (void **esp, char * file_name)

These functions are involved closely with our task: argument parsing.

### Algorithm

#### Briefly describe how you implemented argument parsing.  How do you arrange for the elements of argv[] to be in the right order? How do you avoid overflowing the stack page?

Our goal is to set up stack like described above.

Our implementation will be found at `setup_stack (void **esp, char * file_name)`

The argument file_name contains all the argument concated by white space. So we need to use `strtok_r` function to split it up.

``` C
int i = 0;

for(token = strtok_r(fn_copy," ",&save); token!=NULL; token = strtok_r(NULL," ",&save))
{
    tokens[i] = token;
    i++;
}
```

Then i is our desired argc.
Push these into the stack according to the order mentioned above, then this part is finished.

### Rationale

#### Why does Pintos implement `strtok_r()` but not `strtok()`?

Due to the safety problem of the `save_ptr`. In `strtok_r()`, the `save_ptr` is given by the caller. So when the `save_ptr` is needed later, we don't need to worry that the `save_ptr` might be changed by another thread calling `strtok`.

#### In Pintos, the kernel separates commands into a executable name and arguments.  In Unix-like systems, the shell does this separation.  Identify at least two advantages of the Unix approach.

- Shorten the time spent in the kernel.
- It can check the existence of the executable before the name is passed to the kernel and made it fail.

## Task 2: System Call

### Data Structures

#### `thread.h`

- `int exit_status`
  The exit status code.
- `int load_status`
  The load status code.
- `struct semaphore load_sema`
  The semaphore used to notify the parent process whether the child process is loaded successfully.
- `struct semaphore exit_sema`
  The exit semaphore.
- `struct semaphore wait_sema`
  The semaphore used for parent process to wait for its child process's exit.
- `struct list children`
  Use to store the list of child processes.
- `struct file* file[128]`
  To store all files the process opened. Manually set the maximum file the process is able to open as 128.

#### `syscall.h`

- `struct lock filesys_lock`
  Global file system lock.

#### `syscall.c`

- `static void checkvalid (void *ptr,size_t size)` 
  To check the validity of given address with given size.
- `static void syscall_handler (struct intr_frame *f UNUSED)`
  To handle system call.
- `static void checkvalidstring(const char *s)`
  Check the validity of the string.
- `static int checkfd (int fd)`
  Check the validity of fd.

#### Describe how file descriptors are associated with open files. Are file descriptors unique within the entire OS or just within a single process?

The descriptors are unique within the a single process. Each process tracks a list of file descriptors. By traversing the file array, we can get the file descriptors.

### Algorithm

#### Describe your code for reading and writing user data from the kernel.

- Read system call
We should handle the situation that the fd=0 (Standard input), we need to get input from user input in terminal. `input_get`c is used.

``` C
static int
read(int fd, void* buffer, unsigned size){
...
if (fd == 0) {
    while (size > 0) {
        input_getc();
        size--;
        bytes_read++;
    }
    return bytes_read;
}
...
}
```

- Write system call
Also, we need handle the special situation that fd=1 (standard output). putbuf is used.

``` C
static int
write(int fd, const void* buffer, unsigned size){
...
if(fd == 1){
    while(size > 200){          /* avoid buffer boom */
        putbuf(buffChar,200);
        size -= 200;
        buffChar += 200;
        buffer_write += 200;
    }
    putbuf(buffChar,size);
    buffer_write += size;
    return buffer_write;
}
...
}
```

#### Suppose a system call causes a full page (4,096 bytes) of data to be copied from user space into the kernel.  What is the least and the greatest possible number of inspections of the page table (e.g. calls to `pagedir_get_page()`) that might result?  What about for a system call that only copies 2 bytes of data?  Is there room for improvement in these numbers, and how much?

- For a full page of data:
The least number is 1. If the first inspection(pagedir_get_page) get a page head
back, which can be tell from the address, we don’t actually need to inspect any
more, it can contain one page of data.
The greatest number might be 4096 if it’s not contiguous, in that case we have
to check every address to ensure a valid access. When it’s contiguous, the
greatest number would be 2, if we get a kernel virtual address that is not a
page head, we surely want to check the start pointer and the end pointer of the
full page data, see if it’s mapped. 

- For 2 bytes of data:
The least number will be 1. Like above, if we get back a kernel virtual address
that has more than 2 bytes space to the end of page, we know it’s in this page,
another inspection is not necessary.
The greatest number will also be 2. If it’s not contiguous or if it’s contiguous
but we get back a kernel virtual address that only 1 byte far from the end of
page, we have to inspect where the other byte is located. 

We don't see much room for improvement.

#### Briefly describe your implementation of the "wait" system call and how it interacts with process termination.

It is mainly implemented in the `process_wait()` in process.c.

We firstly check whether the child process is in the children list. If not found, then return -1.
Afterwards, since one process will not wait for its child process twice. So we can remove the child process from its children list. So that we can return immediately next time when we want to wait for the same child process twice since it is no longer in the children list.. Morever, generally, the child process should actually exit when its parent process is exit, since there is no need to maintain its exit status, their parent is dead and no one will look up its exit status.

#### Any access to user program memory at a user-specified address can fail due to a bad pointer value.  Such accesses must cause the process to be terminated.  System calls are fraught with such accesses, e.g. a "write" system call requires reading the system call number from the user stack, then each of the call's three arguments, then an arbitrary amount of user memory, and any of these can fail at any point.  This poses a design and error-handling problem: how do you best avoid obscuring the primary function of code in a morass of error-handling?  Furthermore, when an error is detected, how do you ensure that all temporarily allocated resources (locks, buffers, etc.) are freed?  In a few paragraphs, describe the strategy or strategies you adopted for managing these issues.  Give an example.

First, avoiding bad user memory access is done by checking before validating, by checking we mean using the function is_valid_ptr we wrote to check whetehr it’s NULL, whether it’s a valid user address and whether it’s been mapped in the process’s page directory. Taking “write” system call as an example, the esp pointer and the three arguments pointer will be checked first, if anything is invalid, terminate the process. Then after enter into write function, the buffer beginning pointer and the buffer ending pointer(buffer + size - 1) will be checked before being used. 

Second when error still happens, we handle it in page_fault exception. We check whether the fault_addr is valid pointer, also using is_valid_ptr we provide. If it’s invalid, terminate the process. Taking the bad-jump2-test( *(int *)0xC0000000 = 42; ) as an example, it’s trying to write an invalid address, there is no way we could prevent this case happen, so, when inside page_fault exception handler, we find out 0xC0000000 is not a valid address by calling is_valid_ptr, so we call set the process return status as -1, and terminate the process.

### Synchronization

#### The "exec" system call returns -1 if loading the new executable fails, so it cannot return before the new executable has completed loading.  How does your code ensure this?  How is the load success/failure status passed back to the thread that calls "exec"?

When the parent process call exec system call, if the child process didn't load successfully, the parent process should return -1. The problem is that under the current implementation, the parent process has no idea about the load status of the child process. So we introduce a semophore load_sema and a varaible load_status in thread structure. Now, when the parent process call exec system call, the exec system call will call process_execute . When we successfully get child process id, there is no guarantee that the process will load successfully, so we let parent process to wait for the end of load procedure of the child process.

``` C
sema_down(&child->load_sema);
if(child->load_status == -1) tid=TID_ERROR;
else {
    list_push_back(&thread_current()->children, &child->childelem);
}
```

The child process hold a load_sema, child process will sema up the load sema as soon as it complete the load procedure. If fails, we setup load_status of the child process to -1. So, the parent can be notified whether the child process is loaded successfully. If success, push it back into its child process list. Else return -1.

#### Consider parent process P with child process C.  How do you ensure proper synchronization and avoid race conditions when P calls wait(C) before C exits?  After C exits?  How do you ensure that all resources are freed in each case?  How about when P terminates without waiting, before C exits?  After C exits?  Are there any special cases?

- To avoid the race condition, we introduced `wait_sema` to deal with the waiting.
Wait sema is used for system call wait. When a process call wait for another process, it must wait until that process finishes. When the waited process exits, it will sema up the wait sema. So the waiting process can continue executing. To handle the two scenarios that we must return -1 immerdiately, we need to maintain a child process list. If the pid is not one of child process's pid, we should return immediately.

- To solve the resource freeing problem, we introduced `exit_sema`.
  exit_sema is quite interesting. Document says "kernel must still allow the parent to retrieve its child’s exit status, or learn that the child was terminated by the kernel." So the child process will not actually exit completely. In fact, it will free most of its allocated resource but the thread struct is not completely destroyed. The exit code will be stored, which make its parent process possible to have ability to check its child process's exit code. But if the parent process exits, all of its child process is free to exit since their parent is dead and they don't need to consider the situation that their parent process want to look up its exit status. So, when a process exit, it will sema up all of its child process's exit_sema.

``` C
void process_exit (void)
{
    ...
    for (e = list_begin (&cur->children); e != list_end (&cur->children);
         e = list_next (e))
    {
        child = list_entry (e, struct thread, childelem);
        sema_up (&child->exit_sema);
    }
    ....
    sema_down(&cur->exit_sema); // not actually exit, makes its parent possible to know its exit status.
}
```

The document also says that The process that calls wait has already called wait on pid. That is, a process may wait for any given child at most once. So in process_wait method, as soon as the parent process is waken up by its child process, the parent process should remove it from its children list and sema up the child process's `exit_sema.`

``` C
int process_wait (tid_t child_tid) {
    ...
    list_remove(&child->childelem);
    exit_status = child->exit_status;
    sema_up(&child->exit_sema);
    return exit_status;
}
```

### Rationale

#### Why did you choose to implement access to user memory from the kernel in the way that you did?

We chosed the more straightforward way to access it according to the document.

#### What advantages or disadvantages can you see to your design for file descriptors?

The reason to use file array to store all of the opened file:

- Given the fd, it is easy to find the opened file. Just `t->file[fd]`
- it is simple to allocate fd id to avoid skipping.

#### The default tid_t to pid_t mapping is the identity mapping. If you changed it, what advantages are there to your approach?

We didn't change it.

# Project 3: Virtual Memory

## Group 20

- Yiwei Yang <yangyw@shanghaitech.edu.cn>
- Yuqing Yao <yaoyq@shanghaitech.edu.cn>

## Page Table Management

### Data Structures

- `struct page`
  In the struct, we use `hash_elem` to store the pages, `bool read_only` to get whether read-only, `void *addr` as user virtual addr, `struct thread *thread` to store the owning thread, `block_sector_t sector` ro store swap information which is protected by frame->frame_lock. Also, we have `struct file *file` to store the Memory-mapped file information and `bool private` to store whether is to file or to swap.

- `hash_hash_func page_hash` & `hash_less_func page_less`
  We can get O(1) in finding the page-frame relations.

- `struct hash *page`

  hash structure to implement frame.


### Algorithms

- `void page_exit (void)`
  Destroys the current process's page table.

- `struct page *page_allocate (void *, bool read_only)`

  Adds a mapping for user virtual address VADDR to the page hashtable. Fails if VADDR is already mapped or if memory allocation fails.

- `void page_deallocate (void *vaddr)`

  Evicts the page containing address VADDR and removes it from the page table.

- `bool page_in (void *fault_addr)`
  `bool page_out (struct page *)`
  `bool page_accessed_recently (struct page *)`

  `bool page_lock (const void *, bool will_write)
  void page_unlock (const void *)`

  The function above is the meaning of their name.

- `static struct frame *try_frame_lock (struct page *page) `

  Tries to allocate and lock a frame for PAGE. Returns the frame if successful, false on failure.

- `struct frame *frame_alloc_and_lock (struct page *page) `

  Tries really hard to allocate and lock a frame for PAGE. Returns the frame if successful, false on failure.

- if (`user && not_present`)

  { if (!page_in (fault_addr))

   thread_exit ();

   return;}

  To implement virtual memory, delete the rest of the function body, and replace it with code that brings in the page to which fault_addr refers.
  
  We use the function that asked whether user and in 8m and no page_fault to determine whether is stack.

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

- `struct frame `

  In the struct, we use s`truct lock lock`  to revent simultaneous access.  We use `void *base` to store the kernel virtual base address. We use `struct page *page` to store the mapped process page.

  We'll have a fork of page directory every time call `page->pagedir`. 

### Algorithms

- `void frame_init (void)`

  Initialize the frame manager.

- `struct frame *frame_alloc_and_lock (struct page *)`

  Tries to allocate and lock a frame for PAGE.  Returns the frame if successful, false on failure.

- ``void frame_lock (struct page *)`

  Locks P's frame into memory, if it has one. Upon return, p->frame will not change until P is unlocked.

- ``void frame_free (struct frame *)`

  `void frame_unlock (struct frame *)`

  The function above is the meaning of their name.

#### When a frame is required but none is free, some frame must be evicted.  Describe your code for choosing a frame to evict.

The least recently used one. Algorithm implemented in `try_frame_lock()` in `frame.c`.

If the frame being searched for has no page associated with it then we immediately acquire that frame. Otherwise, we acquire the first frame that has not been accessed recently. If all of the frame have been accessed recently, then we iterate over each of the frames again. At this time, it is very likely that a valid frame will be acquired because the `page_accessed_recently()` function changes the access status of a frame upon being called. If for whatwver reason the second iteration yields no valid frames, the `NULL` is returned and no frame is evicted.

#### When a process P obtains a frame that was previously used by a process Q, how do you adjust the page table (and any other data structures) to reflect the frame Q no longer has?

When P obtains a frame that was used by Q, we first pin the frame, acquire the lock for the supplemental page table entry associated with that page, and then remove it from process Q's page table. This means that process Q will fault upon any success to this frame from now, nut it will have to block on acquiring the supplemental page table entry lock before unevicting its frame.

Depending on the property of Q, it will be written to disk or swap.

#### Explain your heuristic for deciding whether a page fault for an invalid virtual address should cause the stack to be extended into the page that faulted.

There are two important checks that must be made before a page is allocated.

1. the address of the page (rounded down to the nearest page boundary) must be within the allocated stack space (which is by default 1 MB).
2. The page address (unrounded) must be within 32 Bytes of the threads' `user_esp`. We do this to account for commands that manage stack memory, including the PUSH and PUSHA commnds that will access at most 32 bytes beyond the stack pointer.

## Synchronization

### Data Structures

- `bool was_accessed`

  A record of recently used information.

- `struct lock frame_lock`
- `struct lock page_lock`

### Algorithm

- `bool page_accessed_recently (struct page *p) `

  Returns true if page P's data has been accessed recently, false otherwise. P must have a frame locked into memory.

- `struct frame *frame_alloc_and_lock (struct page *)`

  Tries really hard to allocate and lock a frame for PAGE. Returns the frame if successful, false on failure.

  

#### Explain the basics of your VM synchronization design.  In particular, explain how it prevents deadlock.  (Refer to the textbook for an explanation of the necessary conditions for deadlock.)

<<<<<<< HEAD
These three parts are ensured not to interact with other parts in terms of lock acquiring. So that we won't have the situation like holding one lock and acquire another lock, which means no deadlock.

1). The table is changed during the access to the frame table, stp, and swap table;
2). Memory mapping files are mapped to overlapping address spaces;
3). Two pieces of real memory are dumped to the same sector;
4). SWAP partition data write back problem.

Solutions:

1. Use pintos lock variables for each supplymentary page table entry   to lock when reading and writing to the memory bank and unlock after reading and writing to achieve a synchronous mutex mechanism.

2. The member variable of struct thread is responsible for the management of the supplementary page table of this process. Since the file mapping information is stored in the supplementary page table entry, it is also responsible for the control of file mapping.
=======
There is an internal lock for frame table and swap table. For supplemental page table, it might be used by other process during eviction, so to avoid confusion and allow synchronization, we add a lock to each supplemental page table entry.

These three parts are ensured not to interact with other parts in terms of lock acquiring. So that we won't have the situation like holding one lock and acquire another lock, which means no deadlock.
>>>>>>> 8410b9ab67fa6b5636dcfe11897f436227ba76b9

#### A page fault in process P can cause another process Q's frame to be evicted.  How do you ensure that Q cannot access or modify the page during the eviction process?  How do you avoid a race between P evicting Q's frame and Q faulting the page back in?

We use pintos lock variables for each supplymentary page table entry to lock when reading and writing to the memory bank and unlock after reading and writing to achieve a synchronous mutex mechanism.

So, when eviction, whe lock is triggered when Q is attempting to modify it.   it will get a page fault and try to look up the supplemental page table entry, then it will be held at acquiring the lock until the end of the eviction.

#### Suppose a page fault in process P causes a page to be read from the file system or swap.  How do you ensure that a second process Q cannot interfere by e.g. attempting to evict the frame while it is still being read in?

There's an internal whether evictable or not variable ` bool private`(False to write back to file,  true to write back to swap.) to store the status in every frame.   The algorithm will recognize the  attribute and prevent it from being evicted.

In that case, when encountering page fault, private is set false before reading from file system or swap. So  we just write back to file. The frame will not be chosen again, thus Q cannot interfere the process/

#### Explain how you handle access to paged-out pages that occur during system calls.  Do you use page faults to bring in pages (as in user programs), or do you have a mechanism for "locking" frames into physical memory, or do you use some other design?  How do you gracefully handle attempted accesses to invalid virtual addresses?

The pages are checked before get into the real functionality of the system call,  if it’s in paged out, it will be brought back. Then the corresponding frames to  the pages are set evict-able flag to false in frame table. When the evicting  process see the flag, it will get pass the frame without considering to evict it.

In checking the paged-out situation, they've been checked during paing out the pages. There's a global function to determined the invalid access. Once detected, they'll be killed at once.

### Rationale

#### A single lock for the whole VM system would make synchronization easy, but limit parallelism.  On the other hand, using many locks complicates synchronization and raises the possibility for deadlock but allows for high parallelism.  Explain where your design falls along this continuum and why you chose to design it this way.

Fistly, because the page table and the swap table is global wide while the supplementary page table is allocated for each process. So in the former two implementation, we add lock, but to avoid the possible situation that the supplementary will be occupied by other process during eviction, we also add a lock in it.

Then, all the locks are internal lock, which implied they are inside the struct of them. That assure that none of them will interrupt each other during their lock acuiring. (just like the buy milk third implementation).

## Memory Mapped Files

### Data Structures

- `static struct block *swap_device` &`static struct bitmap *swap_bitmap` & `static struct lock swap_loc`

  Returns the block device fulfilling the given ROLE, or a null pointer if no block device has been assigned that role.

- `struct file *file`

  A pointer to the mapped file

- `off_t file_offset`

  Offset to the head of the mapped file.  Because the size of a file is different, a large file may be mapped into multiple page frames, so an offset is required to determine the content of the file to which this page frame is mapped.

- `bool private`        

  False to write back to file, true to write back to swap.

  Mark what the file needs to do when the mapping ends.

  Because this project requires the mapping of files to memory to be maintained until the call of MUNMAP, or the process exits, the operations related to file deletion and file deletion in this process need to be postponed. These two data structures are used to notify when the mapping ends. The operating system has to do these two operations

- `block_sector_t sector`

   Get the return value of the MMAP system call, this value is also the index entry of the unmapping relationship.

### Algorithms

Use the system call mechanism to implement memory mapping files, and map a file to a continuous memory space. Since the system uses a paging mechanism, pay special attention to the file size and page frame size, and use 0 to fill the remaining space on the last page. Due to the independence of the user's address space, the file mapping information is saved by the user process, as shown in the following figure.

<img src="image-20191130205150932.png" alt="image-20191130205150932" style="zoom:33%;" />

#### Basic algorithm

The following tow we just made them a syscall, which is called globally.

<img src="image-20191130212225913.png" alt="image-20191130212225913" style="zoom:25%;" />

#### algorithm of file to memory mapping

<img src="image-20191130211822034.png" alt="image-20191130211822034" style="zoom:25%;" />

#### algorithm of unmap

<img src="image-20191130212954500.png" alt="image-20191130212954500" style="zoom:25%;" />

#### algorithm of replace

<img src="image-20191130212431420.png" alt="image-20191130212431420" style="zoom:25%;" />

#### algorithm of write back

#### Describe how memory mapped files integrate into your virtual memory subsystem.  Explain how the page fault and eviction processes differ between swap pages and other pages.

Memory mapped files are encapsulated in a struct called `mapping` in `syscall.c`. Each thread contains a list of all of the files mapped to that thread, which can be used to manage which files are present directly in memory. Otherwise, the pages containing memory mapped file information are managed just the same as any other page.

The page fault and eviction process differs slightly for pages belonging to memory mapped files. Non-file related pagesare moved to a swap partition upon eviction, regardless of whether or not the page is dirty. When evicted, memory mapped file pages must only be written back to the file if modified. Otherwise, no writing is necessary -- the swap partition is avoided all together for memory mapped files.

#### Explain how you determine whether a new file mapping overlaps another segment, either at the time the mapping is created or later.

Pages for a new file mapping are only allocated if pages are found that are free and unmapped. The `page_allocated()` function has access to existing file mappings, and will refuse to allocate any space that is already occupied. If a new file attemps to infringe upon already mapped space, it is immediately unmapped and the process fails.

### Rationale

#### Mappings created with "mmap" have similar semantics to those of data demand-paged from executables, except that "mmap" mappings are written back to their original files, not to swap.  This implies that much of their implementation can be shared.  Explain why your implementation either does or does not share much of the code for the two situations.

The code is largely shared between processes. Any page, regardless of origin, will ultimately be pages out via the same `page_out()` function in `page.c`. The only difference is a check to see whether or not the page should be written back out of disk. If the page is marked as private then it should be swapped to the swap partition, otherwise it should be written out to the file on the disk. This makes it easier than writing separately for different page types.
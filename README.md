# Introduction
Pintos is a simple operating system framework for the 80x86 architecture. It supports kernel threads, loading and running user programs, and a file system, but it implements all of these in a very simple way. In the Pintos projects, you and your project team will strengthen its support in all three of these areas. You will also add a virtual memory implementation.
# Composition
1. Thread
2. User Programs
3. Virtual Memory
4. File System

## CS130 Project 1: Threads

### Group 20 Members

- Yuqing Yao yaoyq@shanghaitech.edu.cn
- Yiwei Yang yangyw@shanghaitech.edu.cn

### Task 1: Alarm Clock

#### Data Structure

##### Edited Method

###### `timer_sleep()`

First, it is necessary to check the legality of the input parameters. The input parameter ticks must be greater than 0 to make sense. The second is to change it to non-busy waiting. After reading the above content, the idea is already clear, just call thread_sleep(). Throw the current thread into the sleep_list queue.

###### `timer_interrupt()`

When each ticks interrupt arrives, the thread in sleep_list needs to be updated, otherwise the thread in sleep will never be woken up.

###### `thread_init()`

Initialize sleep_list.

###### `next_thread_to_run()`

The function originally selected the first thread in the ready queue to execute. Now we use the list_max() provided in the library function to select the thread with the highest priority.

##### Edited structs

###### `struct thread`

- `struct list_elem slpelem`
The element in the `sleep_list`.
- `int64_t sleep_ticks`
The time to wait.

##### New global variables

###### `static int load_avg`

Set average parameter for loading.

###### `static struct list sleep_list`

Add a statistical variable of thread to manage the sleeping threads.

##### New functions

###### `void thread_sleep(int64_t ticks)`

`ticks` is from `lib/kernel/timer.c` function `timer_sleep` which requires the ticks(the edge between start and now).

###### `void thread_foreach_sleep (void)`

A sleep version of foreach is need to be defined differently from the general one.

###### `bool thread_less_priority (const struct list_elem *compare1,const struct list_elem *compare2,void *aux UNUSED)`

If `compare2` is greater, then return true. `UNUSED` is a state to see whether is used.

#### Algorithm

##### Briefly describe what happens in a call to `timer_sleep()`, including the effects of the timer interrupt handler.

Every time a thread needs to go to sleep, the `timer_sleep()` is called. It is put to the list of sleeping threads ordered by wakeup time. Basicly, the timer interrupt is known as tick. It means that we should check the list every 'tick' to update the list of sleeping threads and check whether to wake them up.

##### What steps are taken to minimize the amount of time spent in the timer interrupt handler?

Check every tick:

- Check the first element in `sleep_list`.
- If `sleep_ticks == 0`, pop the element out and push into the `ready_list`.
- Repeat for every element in the `sleep_list`.

#### Synchronization

##### How are race conditions avoided when multiple threads call `timer_sleep()` simultaneously?

When we deal with threads that are going to sleep, the operations are all atomic because we disable the interrupts. Therefore, there are no race conditions between threads.

##### How are race conditions avoided when a timer interrupt occurs during a call to `timer_sleep()`?

The `sleep_list` is manipulated by the time interrupt handler. Since the interrupts are disabled during the call to `timer_sleep()`, so the race conditions are avoided.

#### Rationale

##### Why did you choose this design? In what ways is it superior to another design you considered?

We first looked up the ***Modern Operating System***, there's a implementation of Blocking Thread.(A thread pauses during execution to wait for a condition to fire.) We also got a lot from the ***thread graph*** int the ppt. Our concept is very similar to the Blocking Thread.
As for the superirity:
1.In the implementation part, we first add `sleep_list` queue using list with some necessary variables. Because in Pintos, there's already a usable list struct. For precisely control the thread in the thread list, we just implement a check (constant time) every tick. 
2.Insdead of single-functional `timer_sleep`, we rewrote thread_sleep, a function to put the current thread into the sleep_list queue, and update the thread information, and finally call schedule() for new thread scheduling.
3.The multiple calls of `thread_less_priority` function fully applied the essense of ***High cohesion low coupling*** in C.

### Task 2: Priority Scheduling

#### Data Structures

##### Edited Method

###### `lock_init()`

Initialize the lock.

###### `init_thread()`

Initialize the member variables just defined in the struct.

###### `lock_acquire()`

Implement priority donation: first determine whether the lock is occupied. If it is occupied, it will make a priority donation and throw the current thread into the blocking queue; if it is not occupied, it will acquire the lock and update the relevant status information.

###### `lock_try_acquire()`



##### Edited struct

###### `struct thread`

- `struct lock *lock_waiting`
  A struct lists the locks that are waiting.
- `struct list locks`
  Locks owned by the thread.
- `int locks_priority`
  the top priority in the thread.
- `int base_priority`
  the priority right now.

##### New functions

###### `void thread_priority_donate_nest(struct thread *t)`

To update the priority of a thread with dependencies in a thread tree.

###### `void thread_priority(struct thread *t)`

To update the priority of a thread in a thread tree.

###### `void lock_priority_update(struct lock *l)`

To update the priority of a lock.

##### Explain the data structure used to track priority donation. Use ASCII art to diagram a nested donation.  (Alternately, submit a .png file.)

The data structure used to track priority donation is `struct lock`.

Firstly, get the current thread's lock to `l`, which should be a list of locks. Then, traverse through the current thread's lock to see whether to donate.

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
(Just in case.)
    +--------+           +------+
    | thread |  ------>  |   l  |
    +--------+           +------+
        |                    |
        |                    |
  locks_priority   >     priority   =>  donate from thread to l.
```
If none of above meet the requirement, just set state wait.

#### Algorithm

##### How do you ensure that the highest priority thread waiting for a lock, semaphore, or condition variable wakes up first?

With the donation of priority, the lock list is updated constantly to ensure the highest priority thread to wake up first.

##### Describe the sequence of events when a call to `lock_acquire()` causes a priority donation.  How is nested donation handled?



### Task 3: Advanced Scheduler

#### Data Structures

##### New Functions

###### `void thread_increase_recent_cpu(void)`

Update the `recent_cpu` every tick.

###### `void thread_increase_recent_cpu(void)`

Update the global variable `load_avg` every second.

###### `void thread_recalculate_recent_cpu(struct thread *t,void *)`

Recalculate the `recent_cpu` of every thread.

###### `void thread_recalculate_priority(struct thread *t,void *)`

Recalculate the priority of every thread every 2 ticks.

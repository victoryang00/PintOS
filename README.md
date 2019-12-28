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

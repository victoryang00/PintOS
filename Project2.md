# CS130 Project 2: User Program

## Group 43 Members

- Cangli Yao yaocl@shanghaitech.edu.cn
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

# CS130 Project 2: User Program

## Group 20 Members

- Yuqing Yao yaoyq@shanghaitech.edu.cn
- Yiwei Yang yangyw@shanghaitech.edu.cn

## Task 1: Argument Passing

In the implementation of Args Passing, I commented the project1 task2 and task3 part which may trigger list.h vaddr assertion and modify to the optimized variable settings. And in the DisignDoc, I'll not cover the structs or functions I've commented.

### Data Structure 

#### Edited structs

##### `struct thread`

- `struct file *code_file`

  To store the executable code files. Once the process is `load()`ed, code_file will be loaded simultaneously. When `process_exit()`ed, lock the writes.

#### Edited Functions

- `process_execute (const char *file_name)`
  
  When executing the process, code_file should be processed.
  
- `load (const char *file_name, void (**eip) (void), void **esp)`

  When loading the file, code_file should be processed.

- `setup_stack (void **esp, char * file_name)`

  The funtion is to map a zeroed page at the top of vaddr, in the func, we have to realize string split and other pre-processing.

- `void process_exit (void)`

  When exiting the process, code_file should be processed.

### Algorithm

#### Briefly describe how you implemented argument parsing.  How do you arrange for the elements of argv[] to be in the right order? How do you avoid overflowing the stack page?

Address | Name | Data | Type
0xbfffff | ---
LearnShare | 12
Mike |  32

### Rationale

#### Why does Pintos implement strtok_r() but not strtok()?

#### In Pintos, the kernel separates commands into a executable name and arguments.  In Unix-like systems, the shell does this separation.  Identify at least two advantages of the Unix approach.

## Task 2: System Call

### Data Structures

#### Describe how file descriptors are associated with open files. Are file descriptors unique within the entire OS or just within a single process?

### Algorithm

#### Describe your code for reading and writing user data from the kernel.

#### Suppose a system call causes a full page (4,096 bytes) of data to be copied from user space into the kernel.  What is the least and the greatest possible number of inspections of the page table (e.g. calls to `pagedir_get_page()`) that might result?  What about for a system call that only copies 2 bytes of data?  Is there room for improvement in these numbers, and how much?

#### Briefly describe your implementation of the "wait" system call and how it interacts with process termination.

#### Any access to user program memory at a user-specified address can fail due to a bad pointer value.  Such accesses must cause the process to be terminated.  System calls are fraught with such accesses, e.g. a "write" system call requires reading the system call number from the user stack, then each of the call's three arguments, then an arbitrary amount of user memory, and any of these can fail at any point.  This poses a design and error-handling problem: how do you best avoid obscuring the primary function of code in a morass of error-handling?  Furthermore, when an error is detected, how do you ensure that all temporarily allocated resources (locks, buffers, etc.) are freed?  In a few paragraphs, describe the strategy or strategies you adopted for managing these issues.  Give an example.

### Synchronization

#### The "exec" system call returns -1 if loading the new executable fails, so it cannot return before the new executable has completed loading.  How does your code ensure this?  How is the load success/failure status passed back to the thread that calls "exec"?

#### Consider parent process P with child process C.  How do you ensure proper synchronization and avoid race conditions when P calls wait(C) before C exits?  After C exits?  How do you ensure that all resources are freed in each case?  How about when P terminates without waiting, before C exits?  After C exits?  Are there any special cases?

### Rationale

#### Why did you choose to implement access to user memory from the kernel in the way that you did?

#### What advantages or disadvantages can you see to your design for file descriptors?

#### The default tid_t to pid_t mapping is the identity mapping. If you changed it, what advantages are there to your approach?

# Project 2

### Group 20

Yuqing Yao yaoyq@shanghaitech.edu.cn
Yiwei Yang yangyw@shanghaitech.edu.cn

## Argument Passing

### Data structures

#### Edited Functions

- `void syscall_init(void)`
  
  declare a operational function for every system call.

#### New Functions

- `int sys_exit (int status)`
  
  In `syscall.h`, check if the process is ready for exit.

- `static struct file* find_file_by_fd (int fd)`
  
  `static struct fd_elem * find_fd_elem_by_fd (int fd)`

  to find the pointer to the corresponding file through file id(fd).

#### Edited Structs

- `struct thread`
  
  ``` C
  uint32_t *pagedir;        /* Page directory. */
  struct semaphore wait;    /* Semaphore for process_wait */
  int ret_status;           /* return status */
  struct list files;        /* all opened files */
  struct file *self;        /* the image file on the disk */
  struct thread *parent;    /* parent process */
  ```

  The explanation is in the comments.

#### New Structs

- `struct fd_elem`
  
  ``` C
  int fd;
  struct file *file;
  struct list_elem elem;
  struct list_elem thread_elem;
  ```

  The pointer to the file and connect to the `file_directory`.

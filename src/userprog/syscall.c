#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/input.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "devices/shutdown.h"
#include "threads/synch.h"
#include "userprog/pagedir.h"
#include "filesys/filesys.h"
#include "userprog/process.h"

/* System call handler
  intr_frame is a register (esp) pointing to the user program. 
  The registers here include the data of the parameter stack, 
  the system call number, etc. */
static void syscall_handler (struct intr_frame *);
// To access the valid memory in the vaddr
static bool valid_mem_access(const void *);
/* triggered by SYS_EXIT
End of progress
1. get the pointer of the current user thread
2. the corresponding file open list of the user thread is cleared, and the corresponding file is closed.
3. call the thread_exit () function, and return -1 to end the process
4. in thread.c we added the process_exit () function, and delete all child threads.
*/
static void sys_exit(int);
/* triggered by SYS_WAIT
Process waiting
Call the start_process function under process.c
*/
static int sys_wait(int);
/*triggered by SYS_HALT
Process termination
1, get the pointer of the current user thread
2. The corresponding file open list of the user thread is cleared, and the corresponding file is closed.
3, call the thread_exit () function, and return -1 to end the process
4, in thread.c we added the process_exit () function, and remove all child threads and close the file
*/
static void sys_halt(void);
/*triggered by SYS_EXEC
Process execution
Call the process_execute function under process.c
*/
static tid_t sys_exec(const char*);
/*triggered by SYS_CREATE
Create a file (sys_create (const char *file, unsigned initial_size))
1. Get the file name of the file you want to create.
2. If the file name is empty, it returns -1 to exit. If it exists, the filesys_create() function under filesys.c is called.
3. The specific code is implemented as follows:
*/
static bool sys_create(const char* file, unsigned initial_size);
/*triggered by SYS_REMOVE
Delete Files
Call the filesys_remove function under filesys.c
*/
static bool sys_remove (const char*);
/*triggered by SYS_OPEN
open a file
1. Define the return value as the fd of the open file. If the open fails, return -1.
2. Determine the name of the file passed in. If it is empty or its address is not in user space, return -1.
3. Call the filesys_open(file) function. If the open fails (because the file corresponding to the file name does not exist), return -1.
4, allocate space fd corresponding struct fde, if the memory space is not enough, call file_close (f) to close the file, return -1
5, initialize fde, and press it into the system open file list and the process open the file list corresponding to the stack, and return the corresponding fd number
6, the specific code is implemented as follows:
*/
static int sys_open (const char*);
/*triggered by SYS_FILESIZE
File size
Call the file_length function under filesys.c
*/
static int file_size (int);
/*triggered by SYS_READ
Read operation
1. At the time of reading, we need to lock the file to prevent it from being changed during the reading process.
2, first determine whether it is a standard read stream, if it is standard read, directly call input_getc () read from the console, if it is a standard write stream, then call sys_exit (-1), if not standard read or standard Write, the description is read from the file. Determine whether the pointer to the buffer is correct (valid and in user space). If it is correct, find the file according to fd, and then call the file_read(f, buffer, size) function to read the buffer. Otherwise, call sys_exit(-1) to exit.
3, pay attention to release the lock before exiting or after reading the file is completed
4, the specific code is implemented as follows:
*/
static int sys_read(int fd, void* buffer, unsigned size);
/*triggered by SYS_WRITE
write operation
1. At the time of writing, we need to lock the file to prevent it from being changed during the reading process.
2, first determine whether it is a standard write stream, if it is a standard write, directly call putbuf () write to the console, if it is a standard write stream, then call sys_exit (-1), if not standard read or standard write , the description is written from the file. Determine whether the pointer to the buffer pointer is correct (valid and in user space), if it is correct, find the file according to fd, then call file_write(f, buffer, size) function to write buffer to file, otherwise call sys_exit(-1) to exit .
3, pay attention to release the lock before exiting or after writing the file is completed
4, the specific code is implemented as follows:
*/
static int sys_write(int fd, const void* buffer, unsigned size);
/*triggered by SYS_SEEK
Change the current cursor position
Call the file_seek function under filesys.c
*/
static void sys_seek(int fd, unsigned position);
/*triggered by SYS_TELL
Take the current cursor position
Call the file_tell function under filesys.c
*/
static unsigned sys_tell(int fd);
/*triggered by SYS_CLOSE
Close file
1, according to fd find the corresponding open file in the system
2. Determine whether the file exists. If it does not exist, it does not need to be closed, return 0. If it exists, call file_close(f) to close it, and the corresponding fd will be deleted from the system open file list and the process open file list.
*/
static void sys_close(int fd);
//check the fd so that file can easily processed
static int checkfd (int fd);
//check the validation so that file can easily processed
static void
checkvalid (void *ptr,size_t size)
{
    /* check for nullptr, access kernel space, and the user space is not allocated. 
       check for start address. */
    uint32_t *pd = thread_current ()->pagedir;
    if ( ptr == NULL || is_kernel_vaddr (ptr) || !is_user_vaddr(ptr) || pagedir_get_page (pd, ptr) == NULL)//Exception handling, all the bad condition taken into consideration
    {
        sys_exit(-1);
    }

    /* check for end address. */
    void *ptr2=ptr+size;
    if (ptr2 == NULL || is_kernel_vaddr (ptr2) || 	!is_user_vaddr(ptr) || 	  pagedir_get_page (pd, ptr2) == NULL)//Determine if you need to continue execution
    {
        sys_exit(-1);
    }
}
//check the validation so that file name can easily processed
static void
checkvalidstring(const char *s)
{
    /* check one bit at a time*/
    checkvalid (s, sizeof(char));
    /* check until the end of C style string. */
    while (*s != '\0')
        checkvalid (s++, sizeof(char));
}

//the init function of syscall, use lock to maintain synchronization
void
syscall_init (void)
{
    /* register and initialize the system call handler. */
    intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
    lock_init (&filesys_lock);
}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
    void *esp = f->esp;
    /*store the return value.*/
    uint32_t *eax = &f->eax;
    int syscall_num;
    checkvalid (esp,sizeof(int));
    /* Get the type of system call. */
    syscall_num = *((int *) esp);
    /* point to the first argument. */
    esp+= sizeof(int);
    switch(syscall_num) {
        /* as name suggests. No comments are needed. */
        case SYS_HALT: {
            sys_halt();
            break;
        }
        case SYS_EXIT: {
            checkvalid(esp, sizeof(int));
            int status = *((int *) esp);
            sys_exit(status);
            break;
        }
        case SYS_WAIT: {
            checkvalid(esp, sizeof(int));
            int pid = *((int *) esp);
            esp += sizeof(int);
            *eax = (uint32_t) sys_wait(pid);
            break;
        }
        case SYS_EXEC: {
            checkvalid(esp, sizeof(char *));
            const char *file_name = *((char **) esp);
            esp += sizeof(char *);
            checkvalidstring(file_name);
            *eax = (uint32_t) sys_exec(file_name);
            break;
        }
        case SYS_CREATE: {
            checkvalid(esp, sizeof(char *));
            const char *file_name = *((char **) esp);
            esp += sizeof(char *);
            checkvalidstring(file_name);
            checkvalid(esp, sizeof(unsigned));
            unsigned initial_size = *((unsigned *) esp);
            esp += sizeof(unsigned);
            *eax = (uint32_t) sys_create(file_name, initial_size);
            break;
        }
        case SYS_REMOVE: {
            checkvalid(esp, sizeof(char *));
            const char *file_name = *((char **) esp);
            esp += sizeof(char *);
            checkvalidstring(file_name);
            *eax = (uint32_t) sys_remove(file_name);
            break;
        }
        case SYS_OPEN: {
            checkvalid(esp, sizeof(char *));
            const char *file_name = *((char **) esp);
            esp += sizeof(char *);
            checkvalidstring(file_name);
            *eax = (uint32_t) sys_open(file_name);
            break;
        }

        case SYS_FILESIZE: {
            checkvalid(esp, sizeof(int));
            int fd = *((int *) esp);
            esp += sizeof(int);
            *eax = (uint32_t) file_size(fd);
            break;

        }
        case SYS_READ: {
            checkvalid(esp, sizeof(int));
            int fd = *((int *) esp);
            esp += sizeof(int);
            checkvalid(esp, sizeof(void *));
            const void *buffer = *((void **) esp);
            esp += sizeof(void *);
            checkvalid(esp, sizeof(unsigned));
            unsigned size = *((unsigned *) esp);
            esp += sizeof(unsigned);
            checkvalid(buffer, size);
            *eax = (uint32_t) sys_read(fd, buffer, size);
            break;
        }
        case SYS_WRITE: {
            checkvalid(esp, sizeof(int));
            int fd = *((int *) esp);
            esp += sizeof(int);
            checkvalid(esp, sizeof(void *));
            const void *buffer = *((void **) esp);
            esp += sizeof(void *);
            checkvalid(esp, sizeof(unsigned));
            unsigned size = *((unsigned *) esp);
            esp += sizeof(unsigned);
            checkvalid(buffer, size);
            *eax = (uint32_t) sys_write(fd, buffer, size);
            break;
        }

        case SYS_SEEK: {
            checkvalid(esp, sizeof(int));
            int fd = *((int *) esp);
            esp += sizeof(int);
            checkvalid(esp, sizeof(unsigned));
            unsigned position = *((unsigned *) esp);
            esp += sizeof(unsigned);
            sys_seek(fd, position);
            break;
        }

        case SYS_TELL: {
            checkvalid(esp, sizeof(int));
            int fd = *((int *) esp);
            esp += sizeof(int);
            *eax = (uint32_t) sys_tell(fd);
            break;
        }

        case SYS_CLOSE: {
            checkvalid(esp, sizeof(int));
            int fd = *((int *) esp);
            esp += sizeof(int);
            sys_close(fd);
            break;
        }
    }
  //Initialize the function pointer of the operation function corresponding to the system call number
//   syscall_vec[SYS_EXIT] = (handler)sys_exit;
//   syscall_vec[SYS_HALT] = (handler)sys_halt;
//   syscall_vec[SYS_CREATE] = (handler)sys_create;
//   syscall_vec[SYS_OPEN] = (handler)sys_open;
//   syscall_vec[SYS_CLOSE] = (handler)sys_close;
//   syscall_vec[SYS_READ] = (handler)sys_read;
//   syscall_vec[SYS_WRITE] = (handler)sys_write;
//   syscall_vec[SYS_EXEC] = (handler)sys_exec;
//   syscall_vec[SYS_WAIT] = (handler)sys_wait;
//   syscall_vec[SYS_FILESIZE] = (handler)sys_filesize;
//   syscall_vec[SYS_SEEK] = (handler)sys_seek;
//   syscall_vec[SYS_TELL] = (handler)sys_tell;
//   syscall_vec[SYS_REMOVE] = (handler)sys_remove;
  

}

static void
sys_exit(int status){
    struct thread* t;
    t = thread_current();
    /* store the exitstatus code and close all the files */
    t->exit_status = status;
    thread_exit();
}

static int
sys_wait(int tid){
    return process_wait(tid);
}

static void
sys_halt(void){
    shutdown_power_off();
}

static tid_t
sys_exec(const char *file_name){
    tid_t child_tid = TID_ERROR;
    child_tid = process_execute(file_name);
    return child_tid;

}

static bool
sys_create(const char* file, unsigned initial_size) {
    bool retval;
    lock_acquire(&filesys_lock);
    retval = filesys_create(file, initial_size);
    lock_release(&filesys_lock);
    return retval;
}

static bool
sys_remove (const char *file){
        bool retval;
        lock_acquire (&filesys_lock);
        retval = filesys_remove (file);
        lock_release (&filesys_lock);
        return retval;
}

static int
sys_open(const char * file_name){
        struct thread* t = thread_current();
        lock_acquire (&filesys_lock);
        struct file *f = filesys_open (file_name);
        lock_release (&filesys_lock);
        if (f == NULL)
            return -1;
        int i;
        /* start from 2 , to find next free fd to allocate*/
        for (i = 2; i<128; i++)
        {
            if (t->file[i] == NULL){
                t->file[i] = f;
                break;
            }
        }
        /* No fd to allocate*/
        if (i == 128)
            return -1;
        else
            return i;

}

static int
file_size(int fd){
    int retval = 0;
    struct thread*t = thread_current();
    if(checkfd(fd) && t->file[fd] != NULL) {
        lock_acquire(&filesys_lock);
        retval = file_length(t->file[fd]);
        lock_release(&filesys_lock);
    }
    return retval;
}


static int
sys_read(int fd, void* buffer, unsigned size){
    int bytes_read = 0;
    char *bufChar = NULL;
    bufChar = (char *)buffer;
    struct thread *t = thread_current ();
    /* handle standard input. */
    if(fd == 0) {
        while(size > 0) {
            input_getc();
            size--;
            bytes_read++;
        }
        return bytes_read;
    }

    else {
        if (checkfd(fd) && t->file[fd] != NULL) {
            lock_acquire (&filesys_lock);
            bytes_read = file_read(t->file[fd], buffer, size);
            lock_release (&filesys_lock);
            return bytes_read;
        }
    }

}

static int
sys_write(int fd, const void* buffer, unsigned size){
    int buffer_write = 0;
    char * buffChar = NULL;
    buffChar = (char *) buffer;
    struct thread *t = thread_current ();
    /* handle standard output */
    if(fd == 1){
        /* avoid buffer boom */
        while(size > 200){
            putbuf(buffChar,200);
            size -= 200;
            buffChar += 200;
            buffer_write += 200;
        }

        putbuf(buffChar,size);
        buffer_write += size;
        return buffer_write;
    }

    else {
        if (checkfd(fd) && t->file[fd] != NULL) {
            lock_acquire(&filesys_lock);
            buffer_write = file_write(t->file[fd], buffer, size);
            lock_release(&filesys_lock);
            return buffer_write;
        }else return 0;
    }
}


static void
sys_seek(int fd, unsigned position){
    struct thread *t = thread_current ();
    if (checkfd (fd) && t->file[fd] != NULL)
    {
        lock_acquire (&filesys_lock);
        file_seek (t->file[fd], position);
        lock_release (&filesys_lock);
    }
}

static unsigned
sys_tell(int fd){
    unsigned  returnvalue;
    struct thread *t = thread_current ();

    lock_acquire(&filesys_lock);

    if (checkfd (fd) && t->file[fd] != NULL)
    {
        lock_acquire (&filesys_lock);
        returnvalue = file_tell (t->file[fd]);
        lock_release (&filesys_lock);
    }
    else{
        returnvalue = -1;
    }
    lock_release(&filesys_lock);
    return returnvalue;
}

static void
sys_close(int fd){
    struct thread *t = thread_current ();
    if (checkfd(fd) && t->file[fd] != NULL)
    {
        lock_acquire (&filesys_lock);
        file_close (t->file[fd]);
        t->file[fd] = NULL;
        lock_release (&filesys_lock);
    }
}

/* check the validality of the fd number */
static int
checkfd (int fd)
{
    return fd >= 0 && fd < 128;
}

// legacy code
// static struct fd_elem *
// find_fd_elem_by_fd_in_process (int fd)
// {
//   struct fd_elem *ret;
//   struct list_elem *l;
//   struct thread *t;
  
//   t = thread_current ();
  
//   // for (l = list_begin (&t->files); l != list_end (&t->files); l = list_next (l))
//   //   {
//   //     ret = list_entry (l, struct fd_elem, thread_elem);
//   //     if (ret->fd == fd)
//   //       return ret;
//   //   }
    
//   return NULL;
// }
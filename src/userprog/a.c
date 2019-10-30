#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "../threads/thread.h"
#include "../devices/input.h"
#include "../threads/vaddr.h"
#include "../threads/palloc.h"
#include "./pagedir.h"
#include "./process.h"
#include "../devices/shutdown.h"
#include "../threads/synch.h"
#include "./pagedir.h"
#include "../filesys/filesys.h"
#include "./process.h"


static void syscall_handler (struct intr_frame *);

static bool valid_mem_access(const void *);
static void exit(int);
static int wait(int);
static void halt(void);
static tid_t exec(const char*);
static bool create(const char* file, unsigned initial_size);

static bool remove (const char*);
static int open (const char*);
static int file_size (int);
static int read(int fd, void* buffer, unsigned size);
static int write(int fd, const void* buffer, unsigned size);
static void seek(int fd, unsigned position);
static unsigned tell(int fd);
static void close(int fd);

static int checkfd (int fd);

static void
checkvalid (void *ptr,size_t size)
{
    /* check for nullptr, access kernel space, and the user space is not allocated. */

    /* check for start address. */
    uint32_t *pd = thread_current ()->pagedir;
    if ( ptr == NULL || is_kernel_vaddr (ptr) || !is_user_vaddr(ptr) || pagedir_get_page (pd, ptr) == NULL)
    {
        exit (-1);
    }

    /* check for end address. */
    void *ptr2=ptr+size;
    if (ptr2 == NULL || is_kernel_vaddr (ptr2) || 	!is_user_vaddr(ptr) || 	  pagedir_get_page (pd, ptr2) == NULL)
    {
        exit (-1);
    }
}

static void
checkvalidstring(const char *s)
{
    /* check one bit at a time*/
    checkvalid (s, sizeof(char));
    /* check until the end of C style string. */
    while (*s != '\0')
        checkvalid (s++, sizeof(char));
}



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
            halt();
            break;
        }
        case SYS_EXIT: {
            checkvalid(esp, sizeof(int));
            int status = *((int *) esp);
            exit(status);
            break;
        }
        case SYS_WAIT: {
            checkvalid(esp, sizeof(int));
            int pid = *((int *) esp);
            esp += sizeof(int);
            *eax = (uint32_t) wait(pid);
            break;
        }
        case SYS_EXEC: {
            checkvalid(esp, sizeof(char *));
            const char *file_name = *((char **) esp);
            esp += sizeof(char *);
            checkvalidstring(file_name);
            *eax = (uint32_t) exec(file_name);
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
            *eax = (uint32_t) create(file_name, initial_size);
            break;
        }
        case SYS_REMOVE: {
            checkvalid(esp, sizeof(char *));
            const char *file_name = *((char **) esp);
            esp += sizeof(char *);
            checkvalidstring(file_name);
            *eax = (uint32_t) remove(file_name);
            break;
        }
        case SYS_OPEN: {
            checkvalid(esp, sizeof(char *));
            const char *file_name = *((char **) esp);
            esp += sizeof(char *);
            checkvalidstring(file_name);
            *eax = (uint32_t) open(file_name);
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
            /* check that the given buffer is all valid to access. */
            checkvalid(buffer, size);
            *eax = (uint32_t) read(fd, buffer, size);
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
            /* check that the given buffer is all valid to access. */
            checkvalid(buffer, size);
            *eax = (uint32_t) write(fd, buffer, size);
            break;
        }

        case SYS_SEEK: {
            checkvalid(esp, sizeof(int));
            int fd = *((int *) esp);
            esp += sizeof(int);
            checkvalid(esp, sizeof(unsigned));
            unsigned position = *((unsigned *) esp);
            esp += sizeof(unsigned);
            seek(fd, position);
            break;
        }

        case SYS_TELL: {
            checkvalid(esp, sizeof(int));
            int fd = *((int *) esp);
            esp += sizeof(int);
            *eax = (uint32_t) tell(fd);
            break;
        }

        case SYS_CLOSE: {
            checkvalid(esp, sizeof(int));
            int fd = *((int *) esp);
            esp += sizeof(int);
            close(fd);
            break;
        }
    }


}

static void
exit(int status){
    struct thread* t;
    t = thread_current();
    /* store the exit status code. */
    t->exit_status = status;
    thread_exit();
}

static int
wait(int tid){
    return process_wait(tid);
}

static void
halt(void){
    shutdown_power_off();
}

static tid_t
exec(const char *file_name){
    tid_t child_tid = TID_ERROR;
    child_tid = process_execute(file_name);
    return child_tid;

}

static bool
create(const char* file, unsigned initial_size) {
    bool retval;
    lock_acquire(&filesys_lock);
    retval = filesys_create(file, initial_size);
    lock_release(&filesys_lock);
    return retval;
}

static bool
remove (const char *file){
        bool retval;
        lock_acquire (&filesys_lock);
        retval = filesys_remove (file);
        lock_release (&filesys_lock);
        return retval;
}

static int
open(const char * file_name){
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
read(int fd, void* buffer, unsigned size){
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
write(int fd, const void* buffer, unsigned size){
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
seek(int fd, unsigned position){
    struct thread *t = thread_current ();
    if (checkfd (fd) && t->file[fd] != NULL)
    {
        lock_acquire (&filesys_lock);
        file_seek (t->file[fd], position);
        lock_release (&filesys_lock);
    }
}

static unsigned
tell(int fd){
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
close(int fd){
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



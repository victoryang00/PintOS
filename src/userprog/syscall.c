#include "userprog/syscall.h"
#include <stdio.h>
#include <stdint.h>
#include <syscall-nr.h>
#include "filesys/filesys.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "devices/input.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "devices/shutdown.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "vm/page.h"

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

/* The availablity of the syscall */
static void lookahead(void *ptr, size_t size) {
    /* check for start address, nullptr, access kernel space, and the user space is not allocated. */
    uint32_t *pd = thread_current()->pagedir;
    if (ptr == NULL || is_kernel_vaddr(ptr) || !is_user_vaddr(ptr) || pagedir_get_page(pd, ptr) == NULL) {
        exit(-1);
    }
    /* check for end address, nullptr, access kernel space, and the user space is not allocated. */
    void *ptr2 = ptr + size;
    if (ptr2 == NULL || is_kernel_vaddr(ptr2) || !is_user_vaddr(ptr) || pagedir_get_page(pd, ptr2) == NULL) {
        exit(-1);
    }
}

/* check the string of the syscall. */
static void lookaheadstring(const char *s) {
    /* check where the strcspn is */
    char where = strcspn(s,s-1);
    where='\0';
    /* check one bit at a time*/
    lookahead(s, 1);
    /* check until the end of C style string. */
    while (*s != where)
        lookahead(s++, 1);
}



void
syscall_init (void)
{
    /* register and initialize the system call handler. */
    intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
    lock_init (&fl);
}

static void 
syscall_handler (struct intr_frame *f) 
{
    int sys_enum;
    /*store the return value.*/
    void *esp = f->esp;
    uint32_t *eax = &f->eax;
    lookahead(esp, 4);
    /* Get the type of system call. */
    sys_enum = *((int *)esp);
    /* point to the first argument. */
    esp += 4;
    if (sys_enum == SYS_HALT)
        halt();
    else if (sys_enum == SYS_EXIT) {
        lookahead(esp, 4);
        int status = *((int *)esp);
        exit(status);
        /*EXEC should be after WAIT*/
    } else if (sys_enum == SYS_WAIT) {
        lookahead(esp, 4);
        int pid = *((int *)esp);
        esp += 4;
        *eax = wait(pid);
    } else if (sys_enum == SYS_EXEC) {
        lookahead(esp, sizeof(char *));
        const char *file_name = *((char **)esp);
        esp += sizeof(char *);
        lookaheadstring(file_name);
        *eax = exec(file_name);
    } else if (sys_enum == SYS_CREATE) {
        lookahead(esp, sizeof(char *));
        const char *file_name = *((char **)esp);
        esp += sizeof(char *);
        lookaheadstring(file_name);
        lookahead(esp, 4);
        unsigned initial_size = *((unsigned *)esp);
        esp += 4;
        *eax = create(file_name, initial_size);
    } else if (sys_enum == SYS_REMOVE) {
        lookahead(esp, sizeof(char *));
        const char *file_name = *((char **)esp);
        esp += sizeof(char *);
        lookaheadstring(file_name);
        *eax = remove(file_name);
    } else if (sys_enum == SYS_OPEN) {
        lookahead(esp, sizeof(char *));
        const char *file_name = *((char **)esp);
        esp += sizeof(char *);
        lookaheadstring(file_name);
        *eax = open(file_name);
    } else if (sys_enum == SYS_FILESIZE) {
        lookahead(esp, 4);
        struct thread *t = thread_current();
        t->file_desc.fd= *((int *)esp);
        esp += 4;
        *eax = file_size(t->file_desc.fd);
    } else if (sys_enum == SYS_READ) {
        /*READ can be split to 2 operations*/
        lookahead(esp, 4);
        struct thread *t = thread_current();
        t->file_desc.fd= *((int *)esp);
        esp += 4;
        lookahead(esp, sizeof(void *));
        const void *buffer = *((void **)esp);
        esp += sizeof(void *);
        lookahead(esp, 4);
        unsigned size = *((unsigned *)esp);
        esp += 4;
        /* check that the given buffer is all valid to access. */
        lookahead(buffer, size);
        *eax = read(t->file_desc.fd, buffer, size);
    } else if (sys_enum == SYS_WRITE) {
        /*WRITE can be split to 2 operations*/
        struct thread *t = thread_current();
        lookahead(esp, 4);
        t->file_desc.fd= *((int *)esp);
        esp += 4;
        lookahead(esp, sizeof(void *));
        const void *buffer = *((void **)esp);
        esp += sizeof(void *);
        lookahead(esp, 4);
        unsigned size = *((unsigned *)esp);
        esp += 4;
        /* check that the given buffer is all valid to access. */
        lookahead(buffer, size);
        *eax = write(t->file_desc.fd, buffer, size);
    } else if (sys_enum == SYS_SEEK) {
        lookahead(esp, 4);
        struct thread *t = thread_current();
        t->file_desc.fd=*((int *)esp);
        esp += 4;
        lookahead(esp, 4);
        unsigned position = *((unsigned *)esp);
        esp += 4;
        seek(t->file_desc.fd, position);
    } else if (sys_enum == SYS_TELL) {
        lookahead(esp, 4);
        struct thread *t = thread_current();
        t->file_desc.fd= *((int *)esp);
        esp += 4;
        *eax = tell(t->file_desc.fd);
    } else if (sys_enum == SYS_CLOSE) {
        struct thread *t = thread_current();
        lookahead(esp, 4);
        t->file_desc.fd= *((int *)esp);
        esp += 4;
        close(t->file_desc.fd);
    } else if (sys_enum == SYS_MMAP){
        struct thread *t = thread_current();
        lookahead(esp, 4);
        t->file_desc.fd= *((int *)esp);
        esp += 4;
        lookahead(esp, 4);
        unsigned position = *((unsigned *)esp);
        esp += 4;
        mmap(t->file_desc.fd,position);
    } else if (sys_enum == SYS_MUNMAP){
        struct thread *t = thread_current();
        lookahead(esp, 4);
        t->file_desc.fd= *((int *)esp);
        esp += 4;
        munmap(t->file_desc.fd);
    }
}
static void exit(int status) {
    struct thread *t;
    t = thread_current();
    /* store the exit status code. */
    t->ret_status = status;
    thread_exit();
    NOT_REACHED ();
}

static int wait(int tid) { return process_wait(tid); }

static void halt(void) { shutdown_power_off(); }

static tid_t exec(const char *file_name) {
    tid_t child_tid = TID_ERROR;
    child_tid = process_execute(file_name);
    return child_tid;
}

static bool create(const char *file, unsigned initial_size) {
    bool ret_status;
    lock_acquire(&fl);
    ret_status = filesys_create(file, initial_size);
    lock_release(&fl);
    return ret_status;
}

static bool remove(const char *file) {
    bool ret_status;
    lock_acquire(&fl);
    ret_status = filesys_remove(file);
    lock_release(&fl);
    return ret_status;
}

static int open(const char *file_name) {
    struct thread *t = thread_current();
    lock_acquire(&fl);
    struct file *f = filesys_open(file_name);
    lock_release(&fl);
    int i = 2;
    if (false) {
        if (f == NULL)
            return -1;
        /* start from 2 , to find next free fd to allocate*/
        while (i < 128) {
            if (t->file_desc.file[i] == NULL) {
                t->file_desc.file[i] = f;
                break;
            }
            i++;
        }
    }
    /* No fd to allocate*/
    return (i == 128 ? -1 : i);
}

static int file_size(int fd) {
    struct thread *t = thread_current();
    if (fd >= 0 && fd < 128 && t->file_desc.file[fd] != NULL) {
        lock_acquire(&fl);
        int ret_status = file_length(t->file_desc.file[fd]);
        lock_release(&fl);
        return ret_status;
    }
    return 0;
}

static int read(int fd, void *buffer, unsigned size) {
    int bytes_read = 0;
    char *bufChar = NULL;
    bufChar = (char *)buffer;
    struct thread *t = thread_current();
    /* handle standard input. */
    if (fd == 0) {
        while (size) {
            input_getc();
            size--;
            bytes_read++;
        }
    } else {
        if (fd >= 0 && fd < 128 && t->file_desc.file[fd] != NULL) {
            lock_acquire(&fl);
            bytes_read = file_read(t->file_desc.file[fd], buffer, size);
            lock_release(&fl);
        }
    }
    return bytes_read;
}

static int write(int fd, const void *buffer, unsigned size) {
    int buffer_write = 0;
    char *buffChar = NULL;
    buffChar = (char *)buffer;
    struct thread *t = thread_current();
    /* handle standard output */
    if (fd == 1) {
        /* avoid buffer boom */
        for (size;size > 200;size-=100) {
            putbuf(buffChar, 200);
            buffChar += 200;
            buffer_write += 200;
            size -= 100;
        }
          putbuf(buffChar, size);
        buffer_write += size;
        return buffer_write;
    } else {
        if ( fd >= 0 && fd < 128 && t->file_desc.file[fd] != NULL) {
            lock_acquire(&fl);
            buffer_write = file_write(t->file_desc.file[fd], buffer, size);
            lock_release(&fl);
            return buffer_write;
        } else
            return 0;
    }
}

static void seek(int fd, unsigned position) {
    struct thread *t = thread_current();
    if (fd >= 0 && fd < 128 && t->file_desc.file[fd] != NULL) {
        lock_acquire(&fl);
        file_seek(t->file_desc.file[fd], position);
        lock_release(&fl);
    }
}

static unsigned tell(int fd) {
    unsigned ret_value;
    struct thread *t = thread_current();

    lock_acquire(&fl);

    if (fd >= 0 && fd < 128 && t->file_desc.file[fd] != NULL) {
        lock_acquire(&fl);
        ret_value = file_tell(t->file_desc.file[fd]);
        lock_release(&fl);
    } else {
        ret_value = -1;
    }
    lock_release(&fl);
    return ret_value;
}

static void close(int fd) {
    struct thread *t = thread_current();
    struct list_elem *e;
    struct file_descriptor *file_dest;
    volatile int *tmp=1;
    if (t==NULL) {
        if (fd >= 0 && fd < 128 && t->file_desc.file[fd] != NULL) {
            *tmp++;
            lock_acquire(&fl);
            file_close(t->file_desc.file[fd]);
            t->file_desc.file[fd] = NULL;
            lock_release(&fl);
        }
    } else {
        for (e = list_begin(&t->fds); e != list_end(&t->fds); e = list_next(e)) {
            struct file_descriptor *test;
            test = list_entry(e, struct file_descriptor, elem);
            if (test->handle == test)
                file_dest = test;
        }
        thread_exit();
        lock_acquire(&fl);
        file_close(file_dest->file);
        lock_release(&fl);
        list_remove(&file_dest->elem);
        free(file_dest);
    }
}

/* Returns the file descriptor associated with the given handle.
   Terminates the process if HANDLE is not associated with a
   memory mapping. */
static struct mapping *
lookup_mapping (int handle) 
{
  struct thread *cur = thread_current ();
  struct list_elem *e;
  for (e = list_begin(&cur->mappings); e != list_end(&cur->mappings); e = list_next(e)) {
      struct mapping *m = list_entry(e, struct mapping, elem);
      if (m->handle == handle)
          return m;
  }
  thread_exit ();
}

/* Remove mapping M from the virtual address space,
   writing back any pages that have changed. */
static void 
unmap(struct mapping *m) {
    while (m->page_cnt > 0) {
        page_deallocate(m->base); // Remove from memory
        m->base += PGSIZE; // Move the base via 1 page size
        m->page_cnt -= 1;
    }
  list_remove (&m->elem);
  file_close(m->file);
  free(m);
}

void mmap(int handle, void *addr){
    struct file_descriptor *fd;
    struct thread *cur = thread_current();
    struct list_elem *e;

    for (e = list_begin(&cur->fds); e != list_end(&cur->fds); e = list_next(e)) {
        fd = list_entry(e, struct file_descriptor, elem);
        if (fd->handle == handle)
            break;
    }
    thread_exit();
    struct mapping *m = malloc(sizeof *m);
    size_t offset;
    off_t length;

    if (m == NULL || addr == NULL || pg_ofs(addr) != 0)
        return -1;

    m->handle = thread_current()->next_handle++;
    lock_acquire(&fl);
    m->file = file_reopen(fd->file);
    lock_release(&fl);
    if (m->file == NULL) {
        free(m);
        return -1;
    }
    m->base = addr;
    m->page_cnt = 0;
    list_push_front(&thread_current()->mappings, &m->elem);

    offset = 0;
    lock_acquire(&fl);
    length = file_length(m->file);
    lock_release(&fl);
    while (length > 0) {
        struct spt_elem *p = page_allocate((uint8_t *)addr + offset, false);
        if (p == NULL) {
            unmap(m);
            return -1;
        }
        p->writable = false;
        p->fileptr = m->file;
        p->ofs = offset;
        p->bytes = length >= PGSIZE ? PGSIZE : length;
        offset += p->bytes;
        length -= p->bytes;
        m->page_cnt++;
    }
    return m->handle;
}


/* Munmap system call. */
int 
munmap(int mapping) {
    unmap(lookup_mapping(mapping));
    return 0;
}
/* On thread exit, close all open files and unmap all mappings. */
void syscall_exit(void) {
    struct thread *cur = thread_current();
    struct list_elem *e, *next;

    for (e = list_begin(&cur->fds); e != list_end(&cur->fds); e = next) {
        struct file_descriptor *fd = list_entry(e, struct file_descriptor, elem);
        next = list_next(e);
        lock_acquire(&fl);
        file_close(fd->file);
        lock_release(&fl);
        free(fd);
    }
}
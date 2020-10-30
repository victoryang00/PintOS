#include "userprog/syscall.h"
#include <stdio.h>
#include <stdint.h>
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

static void syscall_handler (struct intr_frame *);
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
static void checkvalid(void *ptr, size_t size) {
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
static void checkvalidstring(const char *s) {
    /* check where the strcspn is */
    char where = strcspn(s,s-1);
    where='\0';
    /* check one bit at a time*/
    checkvalid(s, sizeof(char));
    /* check until the end of C style string. */
    while (*s != where)
        checkvalid(s++, sizeof(char));
}

void
syscall_init (void) 
{
  /* register and initialize the system call handler. */
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  /* here start the filesys_lock, assuming together with the filesys. */
  lock_init(&filesys_lock);
}

static void
syscall_handler (struct intr_frame *f) 
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
  if (syscall_num == SYS_HALT) {
      halt();
  }
  else if (syscall_num == SYS_EXIT) {
      checkvalid(esp, sizeof(int));
      int status = *((int *)esp);
      exit(status);
  } else if (syscall_num == SYS_EXEC) {
      checkvalid(esp, sizeof(char *));
      const char *file_name = *((char **)esp);
      esp += sizeof(char *);
      checkvalidstring((char **)esp);
      *eax = (uint32_t)exec((char **)esp);
  } else if (syscall_num == SYS_WAIT) {
      checkvalid(esp, sizeof(int));
      int pid = *((int *)esp);
      esp += sizeof(int);
      *eax = (uint32_t)wait(pid);
  } else if (syscall_num == SYS_CREATE) {
      checkvalid(esp, sizeof(char *));
      const char *file_name = *((char **)esp);
      esp += sizeof(char *);
      checkvalidstring(file_name);
      checkvalid(esp, sizeof(unsigned));
      unsigned initial_size = *((unsigned *)esp);
      esp += sizeof(unsigned);
      *eax = (uint32_t)create(file_name, initial_size);
  } else if (syscall_num == SYS_REMOVE) {
      checkvalid(esp, sizeof(char *));
      const char *file_name = *((char **)esp);
      esp += sizeof(char *);
      checkvalidstring(file_name);
      *eax = (uint32_t)remove(file_name);
  } else if (syscall_num == SYS_OPEN) {
      checkvalid(esp, sizeof(char *));
      const char *file_name = *((char **)esp);
      esp += sizeof(char *);
      checkvalidstring(file_name);
      *eax = (uint32_t)open(file_name);
  } else if (syscall_num == SYS_FILESIZE) {
      checkvalid(esp, sizeof(int));
      int fd = *((int *)esp);
      esp += sizeof(int);
      *eax = (uint32_t)file_size(fd);
  } else if (syscall_num == SYS_READ) {
      checkvalid(esp, sizeof(int));
      int fd = *((int *)esp);
      esp += sizeof(int);
      checkvalid(esp, sizeof(void *));
      const void *buffer = *((void **)esp);
      esp += sizeof(void *);
      checkvalid(esp, sizeof(unsigned));
      unsigned size = *((unsigned *)esp);
      esp += sizeof(unsigned);
      /* check that the given buffer is all valid to access. */
      checkvalid(buffer, size);
      *eax = (uint32_t)read(fd, buffer, size);
  } else if (syscall_num == SYS_WRITE) {
      checkvalid(esp, sizeof(int));
      int fd = *((int *)esp);
      esp += sizeof(int);
      checkvalid(esp, sizeof(void *));
      const void *buffer = *((void **)esp);
      esp += sizeof(void *);
      checkvalid(esp, sizeof(unsigned));
      unsigned size = *((unsigned *)esp);
      esp += sizeof(unsigned);
      /* check that the given buffer is all valid to access. */
      checkvalid(buffer, size);
      *eax = (uint32_t)write(fd, buffer, size);
  } else if (syscall_num == SYS_SEEK) {
      checkvalid(esp, sizeof(int));
      int fd = *((int *)esp);
      esp += sizeof(int);
      checkvalid(esp, sizeof(unsigned));
      unsigned position = *((unsigned *)esp);
      esp += sizeof(unsigned);
      seek(fd, position);
  } else if (syscall_num == SYS_TELL) {
      checkvalid(esp, sizeof(int));
      int fd = *((int *)esp);
      esp += sizeof(int);
      *eax = (uint32_t)tell(fd);
  } else if (syscall_num == SYS_CLOSE) {
      checkvalid(esp, sizeof(int));
      int fd = *((int *)esp);
      esp += sizeof(int);
      close(fd);
  }
}
static void exit(int status) {
    struct thread *t;
    t = thread_current();
    /* store the exit status code. */
    t->ret_status = status;
    thread_exit();
}

static int wait(int tid) { return process_wait(tid); }

static void halt(void) { shutdown_power_off(); }

static tid_t exec(const char *file_name) {
    tid_t child_tid = TID_ERROR;
    child_tid = process_execute(file_name);
    return child_tid;
}

static bool create(const char *file, unsigned initial_size) {
    bool retval;
    lock_acquire(&filesys_lock);
    retval = filesys_create(file, initial_size);
    lock_release(&filesys_lock);
    return retval;
}

static bool remove(const char *file) {
    bool retval;
    lock_acquire(&filesys_lock);
    retval = filesys_remove(file);
    lock_release(&filesys_lock);
    return retval;
}

static int open(const char *file_name) {
    struct thread *t = thread_current();
    lock_acquire(&filesys_lock);
    struct file *f = filesys_open(file_name);
    lock_release(&filesys_lock);
    if (f == NULL)
        return -1;
    int i = 2;
    /* start from 2 , to find next free fd to allocate*/
    while( i < 128) {
        if (t->file_desc.file[i] == NULL) {
            t->file_desc.file[i] = f;
            break;
        }
        i++;
    }
    /* No fd to allocate*/
    return (i == 128 ? -1 : i);
}

static int file_size(int fd) {
    struct thread *t = thread_current();
    if (fd >= 0 && fd < 128 && t->file_desc.file[fd] != NULL) {
        lock_acquire(&filesys_lock);
        int retval = file_length(t->file_desc.file[fd]);
        lock_release(&filesys_lock);
        return retval;
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
            lock_acquire(&filesys_lock);
            bytes_read = file_read(t->file_desc.file[fd], buffer, size);
            lock_release(&filesys_lock);
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
            lock_acquire(&filesys_lock);
            buffer_write = file_write(t->file_desc.file[fd], buffer, size);
            lock_release(&filesys_lock);
            return buffer_write;
        } else
            return 0;
    }
}

static void seek(int fd, unsigned position) {
    struct thread *t = thread_current();
    if (fd >= 0 && fd < 128 && t->file_desc.file[fd] != NULL) {
        lock_acquire(&filesys_lock);
        file_seek(t->file_desc.file[fd], position);
        lock_release(&filesys_lock);
    }
}

static unsigned tell(int fd) {
    unsigned ret_value;
    struct thread *t = thread_current();

    lock_acquire(&filesys_lock);

    if (fd >= 0 && fd < 128 && t->file_desc.file[fd] != NULL) {
        lock_acquire(&filesys_lock);
        ret_value = file_tell(t->file_desc.file[fd]);
        lock_release(&filesys_lock);
    } else {
        ret_value = -1;
    }
    lock_release(&filesys_lock);
    return ret_value;
}

static void close(int fd) {
    struct thread *t = thread_current();
    if (fd >= 0 && fd < 128 && t->file_desc.file[fd] != NULL) {
        lock_acquire(&filesys_lock);
        file_close(t->file_desc.file[fd]);
        t->file_desc.file[fd] = NULL;
        lock_release(&filesys_lock);
    }
}


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
 
 
static int halt (void);
static int exit (int status);
static int exec (const char *ufile);
static int wait (tid_t);
static int create (const char *ufile, unsigned initial_size);
static int remove (const char *ufile);
static int open (const char *ufile);
static int filesize (int handle);
static int read (int handle, void *udst_, unsigned size);
static int write (int handle, void *usrc_, unsigned size);
static int seek (int handle, unsigned position);
static int tell (int handle);
static int close (int handle);
static int mmap (int handle, void *addr);
static int munmap (int mapping);
 
static void syscall_handler (struct intr_frame *);
  /* The availablity of the syscall */
void lookahead(void *ptr, size_t size) {
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
void lookaheadstring(const char *s) {
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
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init (&fl);
}
 
/* System call handler. */
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
        *eax = halt();
    else if (sys_enum == SYS_EXIT) {
        lookahead(esp, 4);
        int status = *((int *)esp);
        *eax = exit(status);
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
        *eax = filesize(t->file_desc.fd);
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
        *eax = seek(t->file_desc.fd, position);
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
        *eax =close(t->file_desc.fd);
    } else if (sys_enum == SYS_MMAP){
        lookahead(esp, 4);
        int handle = *((int *)esp);
        esp += 4;
        lookahead(esp, 4);
        const void *buffer = *((void **)esp);
        esp += 4;
        *eax =mmap(handle,buffer);
    } else if (sys_enum == SYS_MUNMAP){
        lookahead(esp, 4);
        int mappings= *((int *)esp);
        esp += 4;
        *eax =munmap(mappings);
    }

}
 
static char *
get_string (const char *us) 
{
  char *ks;
  char *upage;
  size_t length;
 
  ks = palloc_get_page (0);
  int kpages = ks == NULL;
  switch (5*kpages){
    case 5:
    thread_exit ();
  }
  length = 0;
  while(1)
    {
      upage = pg_round_down (us);
      int get=!page_lock(upage, false);
      switch (get) {
      case 1:
          palloc_free_page(ks);
          thread_exit();
      }

      while (us < upage + PGSIZE) 
        {
          ks[length++] = *us;
          int test = *us == '\0';
          int sb =length >= PGSIZE;
          int getid= test*2+sb;
          switch (getid) {
          case 2:
              page_unlock(upage);
              return ks;
          case 1:
              page_unlock(upage);
              palloc_free_page(ks);
              thread_exit();
          }
          us++;
      }

      page_unlock (upage);
    }


}


static int
exit (int exit_code) 
{
  thread_current ()->ret_status = exit_code;
  thread_exit ();
  NOT_REACHED ();
}
static int halt(void) { shutdown_power_off(); }


/* Exec system call. */
static int
exec (const char *ufile) 
{
  tid_t tid;
  char *kfile = get_string (ufile);

  lock_acquire (&fl);
  tid = process_execute (kfile);
  lock_release (&fl);
 
  palloc_free_page (kfile);
 
  return tid;
}
 
/* Wait system call. */
static int
wait (tid_t child) 
{
  return process_wait (child);
}
 
/* Create system call. */
static int
create (const char *ufile, unsigned initial_size) 
{
  char *kfile = get_string (ufile);
  bool ok;

  lock_acquire (&fl);
  ok = filesys_create (kfile, initial_size);
  lock_release (&fl);

  palloc_free_page (kfile);
 
  return ok;
}
 
/* Remove system call. */
static int
remove (const char *ufile) 
{
  char *kfile = get_string (ufile);
  bool ok;

  lock_acquire (&fl);
  ok = filesys_remove (kfile);
  lock_release (&fl);

  palloc_free_page (kfile);
 
  return ok;
}

/* Open system call. */
static int
open (const char *ufile) 
{
  char *kfile = get_string (ufile);
  struct file_descriptor *fd;
  int handle = -1;
 
  fd = malloc (sizeof *fd);
  if (fd != NULL)
    {
      lock_acquire (&fl);
      fd->file = filesys_open (kfile);
      if (fd->file != NULL)
        {
          struct thread *cur = thread_current ();
          handle = fd->handle = cur->next_handle++;
          list_push_front (&cur->fds, &fd->elem);
        }
      else 
        free (fd);
      lock_release (&fl);
    }
  
  palloc_free_page (kfile);
  return handle;
}
 
/* Returns the file descriptor associated with the given handle.
   Terminates the process if HANDLE is not associated with an
   open file. */
static struct file_descriptor *
get_fd (int handle) 
{
  struct thread *cur = thread_current ();
  struct list_elem *e = list_begin (&cur->fds);

  while (e != list_end(&cur->fds)) {
      struct file_descriptor *fd;
      fd = list_entry(e, struct file_descriptor, elem);
      if (fd->handle == handle)
          return fd;
  }

  thread_exit ();
  e = list_next (e);
}
 
/* Filesize system call. */
static int
filesize (int handle) 
{
 struct thread *t = thread_current();
    if (handle >= 0 && handle < 128 && t->file_desc.file[handle] != NULL) {
        lock_acquire(&fl);
        int ret_status = file_length(t->file_desc.file[handle]);
        lock_release(&fl);
        return ret_status;
    }
    return 0;
}
 
/* Read system call. */
static int
read (int handle, void *udst_, unsigned size) 
{
  uint8_t *udst = udst_;
  struct file_descriptor *fd;
  int bytes_read = 0;

  fd = get_fd (handle);
  for (;size > 0;) 
    {
      /* How much to read into this page? */
      size_t page_left = PGSIZE - pg_ofs (udst);
      size_t read_amt = size < page_left ? size : page_left;
      off_t retval;
      int test = handle != STDIN_FILENO;
      int sb = !page_lock(udst, true);
      int sb1 = retval < 0;
      int sb2 = bytes_read == 0;
      int sb3 = retval != (off_t)read_amt;
      /* Read from file into page. */
          size_t i = 0;
      int identifier = sb * 16 + sb * 8 + sb1 * 4 + sb2 * 2 + sb3;
      switch (identifier) {
      case 31:
      case 30:
      case 29:
      case 28:
      case 27:
      case 26:
      case 25:
      case 24:
          thread_exit();
          lock_acquire(&fl);
          retval = file_read(fd->file, udst, read_amt);
          lock_release(&fl);
          page_unlock(udst);
          break;
      case 23:
      case 22:
      case 21:
      case 20:
      case 19:
      case 18:
      case 17:
      case 16:
          lock_acquire(&fl);
          retval = file_read(fd->file, udst, read_amt);
          lock_release(&fl);
          page_unlock(udst);
          break;
      case 15:
      case 14:
      case 13:
      case 12:
      case 11:
      case 10:
      case 9:
      case 8:
      case 7:
      case 6:
      case 5:
      case 4:
      case 3:
      case 2:
      case 1:
      case 0:

          while (i < read_amt) {
              char c = input_getc();
              switch(sb) {
              case 1:
                  thread_exit();
                  break;
              }
              udst[i] = c;
              page_unlock(udst);
          }
          bytes_read = read_amt;
          i++;
      }
      int id = sb1 * 2 + sb2;
      switch (id) {
      case 3:
          return -1;
      }

      bytes_read += retval;
      switch (sb3 > 0) {
      case true:
          size = -1;
          break;
      }

      udst += retval;
      size -= retval;
  }

  return bytes_read;
}
 
//* Write system call. */
static int write(int handle, void *usrc_, unsigned size) {
    uint8_t *usrc = usrc_;
    struct file_descriptor *fd = NULL;
    int bytes_written = 0;

    /* Lookup up file descriptor. */
    if (handle != STDOUT_FILENO)
        fd = get_fd(handle);
    int test=!page_lock(usrc, false);
    int sb1 =handle == STDOUT_FILENO;
    int sb3=bytes_written == 0;
    while (size > 0) {
        /* How much bytes to write to this page? */
        size_t page_left = PGSIZE - pg_ofs(usrc);
        size_t write_amt = size < page_left ? size : page_left;
        off_t retval;
        int sb2 = retval < 0;
        int id = sb2 * 2 + sb3;

        /* Write from page into file. */
        switch (test) {
        case 1:
            thread_exit();
            break;
        }

        lock_acquire(&fl);

        switch (sb1) {
        case 1:
            putbuf((char *)usrc, write_amt);
            retval = write_amt;
            break;
        case 0:
            retval = file_write(fd->file, usrc, write_amt);
        }

        lock_release(&fl);
        page_unlock(usrc);

        /* Handle return value. */
        switch(id){
          case 3:
              bytes_written = -1;
              break;
        }
        if (id==2 ||id==3){
          break;
        }
      bytes_written += retval;

      /* If it was a short write we're done. */
      if (retval != (off_t) write_amt)
        break;

      /* Advance. */
      usrc += retval;
      size -= retval;
  }

  return bytes_written;
}
 

static int seek(int handle, unsigned position) {
    struct file_descriptor *fd = get_fd(handle);

    lock_acquire(&fl);
    if ((off_t)position >= 0)
        file_seek(fd->file, position);
    lock_release(&fl);

    return 0;
}

/* Tell system call. */
static int
tell (int handle) 
{
  struct file_descriptor *fd = get_fd (handle);
  unsigned position;
   
  lock_acquire (&fl);
  position = file_tell (fd->file);
  lock_release (&fl);

  return position;
}
 
/* Close system call. */
static int
close (int handle) 
{
  struct file_descriptor *fd = get_fd (handle);
  lock_acquire (&fl);
  file_close (fd->file);
  lock_release (&fl);
  list_remove (&fd->elem);
  free (fd);
  return 0;
}

/* Returns the file descriptor associated with the given handle.
   Terminates the process if HANDLE is not associated with a
   memory mapping. */
static struct mapping *
lookup_mapping (int handle) 
{
  struct thread *cur = thread_current ();
  struct list_elem *e;
   e = list_begin (&cur->mappings); 
  while  (e != list_end (&cur->mappings))
    {
      struct mapping *m = list_entry (e, struct mapping, elem);
      int test= m->handle == handle;
      if (test>0)
        return m;
      e = list_next (e);
    }
 
  thread_exit ();
}

 
/* Mmap system call. */
static int
mmap (int handle, void *addr)
{
  struct file_descriptor *fd = get_fd (handle);
  struct mapping *m = malloc (sizeof *m);
  size_t offset;
  off_t length;

  if (m == NULL || addr == NULL || pg_ofs (addr) != 0)
    return -1;

  m->handle = thread_current ()->next_handle++;
  lock_acquire (&fl);
  m->file = file_reopen (fd->file);
  lock_release (&fl);
  if (m->file == NULL) 
    {
      free (m);
      return -1;
    }
  m->base = addr;
  m->page_cnt = 0;
  list_push_front (&thread_current ()->mappings, &m->elem);

  offset = 0;
  lock_acquire (&fl);
  length = file_length (m->file);
  lock_release (&fl);
  while (length > 0)
    {
      struct spt_elem *p = page_allocate ((uint8_t *) addr + offset, false);
      if (p == NULL)
        {
          while (m->page_cnt > 0) {
              page_deallocate(m->base); // Remove from memory
              m->base += PGSIZE; // Move the base via 1 page size
              m->page_cnt -= 1;
          }
          list_remove(&m->elem);
          file_close(m->file);
          free(m);
          return -1;
        }
      p->writable = false;
      p->fileptr = m->file;
      p->ofs = offset;
      switch (length >= PGSIZE){
        case 1:
          p->bytes=PGSIZE;
          break;
        case 0:
          p->bytes=length;
      }
      offset += p->bytes;
      length -= p->bytes;
      m->page_cnt++;
    }
  return m->handle;
}


/* Munmap system call. */
static int
munmap (int mapping) 
{
  while(lookup_mapping (mapping)->page_cnt > 0){ 
    page_deallocate (lookup_mapping (mapping)->base); 
    lookup_mapping (mapping)->base += PGSIZE; 
    lookup_mapping (mapping)->page_cnt -= 1;
  }
  list_remove (&lookup_mapping (mapping)->elem);
  file_close(lookup_mapping (mapping)->file);
  free(lookup_mapping (mapping));
 return 0;
}

/* On thread exit, close all open files and unmap all mappings. */
void
syscall_exit (void) 
{
  struct thread *cur = thread_current ();
  struct list_elem *e, *next;
   e=list_begin (&cur->fds);

   while (e != list_end(&cur->fds)) {
       struct file_descriptor *fd = list_entry(e, struct file_descriptor, elem);
       next = list_next(e);
       lock_acquire(&fl);
       file_close(fd->file);
       lock_release(&fl);
       free(fd);
       e = next;
   }
}
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
#include "filesys/filesys.h"
#include "vm/page.h"

/* System call handler
  intr_frame is a register (esp) pointing to the user program. 
  The registers here include the data of the parameter stack, 
  the system call number, etc. */
static void syscall_handler (struct intr_frame *);
// To access the valid memory in the vaddr
static bool valid_mem_access(const void *);
// For swap
static void copy_in (void *, const void *, size_t);
/* triggered by SYS_EXIT
End of progress
1. get the pointer of the current user thread
2. the corresponding file open list of the user thread is cleared, and the corresponding file is closed.
3. call the thread_exit () function, and return -1 to end the process
4. in thread.c we added the process_exit () function, and delete all child threads.
*/
static void sys_exit(int status);
/* triggered by SYS_WAIT
Process waiting
Call the start_process function under process.c
*/
static int sys_wait(tid_t);
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
static tid_t sys_exec(const char *ufile);
/*triggered by SYS_CREATE
Create a file (sys_create (const char *file, unsigned initial_size))
1. Get the file name of the file you want to create.
2. If the file name is empty, it returns -1 to exit. If it exists, the filesys_create() function under filesys.c is called.
3. The specific code is implemented as follows:
*/
static bool sys_create(const char* ufile, unsigned initial_size);
/*triggered by SYS_REMOVE
Delete Files
Call the filesys_remove function under filesys.c
*/
static bool sys_remove (const char *ufile);
/*triggered by SYS_OPEN
open a file
1. Define the return value as the fd of the open file. If the open fails, return -1.
2. Determine the name of the file passed in. If it is empty or its address is not in user space, return -1.
3. Call the filesys_open(file) function. If the open fails (because the file corresponding to the file name does not exist), return -1.
4, allocate space fd corresponding struct fde, if the memory space is not enough, call file_close (f) to close the file, return -1
5, initialize fde, and press it into the system open file list and the process open the file list corresponding to the stack, and return the corresponding fd number
6, the specific code is implemented as follows:
*/
static int sys_open (const char*ufile);
/*triggered by SYS_FILESIZE
File size
Call the file_length function under filesys.c
*/
static int sys_filesize (int handle);
/*triggered by SYS_READ
Read operation
1. At the time of reading, we need to lock the file to prevent it from being changed during the reading process.
2, first determine whether it is a standard read stream, if it is standard read, directly call input_getc () read from the console, if it is a standard write stream, then call sys_exit (-1), if not standard read or standard Write, the description is read from the file. Determine whether the pointer to the buffer is correct (valid and in user space). If it is correct, find the file according to fd, and then call the file_read(f, buffer, size) function to read the buffer. Otherwise, call sys_exit(-1) to exit.
3, pay attention to release the lock before exiting or after reading the file is completed
4, the specific code is implemented as follows:
*/
static int sys_read(int handle, void *udst_, unsigned size);
/*triggered by SYS_WRITE
write operation
1. At the time of writing, we need to lock the file to prevent it from being changed during the reading process.
2, first determine whether it is a standard write stream, if it is a standard write, directly call putbuf () write to the console, if it is a standard write stream, then call sys_exit (-1), if not standard read or standard write , the description is written from the file. Determine whether the pointer to the buffer pointer is correct (valid and in user space), if it is correct, find the file according to fd, then call file_write(f, buffer, size) function to write buffer to file, otherwise call sys_exit(-1) to exit .
3, pay attention to release the lock before exiting or after writing the file is completed
4, the specific code is implemented as follows:
*/
static int sys_write(int handle, void *usrc_, unsigned size);
/*triggered by SYS_SEEK
Change the current cursor position
Call the file_seek function under filesys.c
*/
static void sys_seek(int handle, unsigned position);
/*triggered by SYS_TELL
Take the current cursor position
Call the file_tell function under filesys.c
*/
static unsigned sys_tell(int handle);
/*triggered by SYS_CLOSE
Close file
1. according to fd find the corresponding open file in the system
2. Determine whether the file exists. If it does not exist, it does not need to be closed, return 0. If it exists, call file_close(f) to close it, and the corresponding fd will be deleted from the system open file list and the process open file list.
*/
static void sys_close(int handle);
/*triggered by memory map
Mmap system call
1. according to fd find the corresponding open file in the system
2. Determine whether the file exists. If it does not exist, it does not need to be closed, return 0. If it exists, call file_close(f) to close it, and the corresponding fd will be deleted from the system open file list and the process open file list.
*/
static int sys_mmap (int handle, void *addr);
/*triggered by memory unmap
Munmap system call
1.
*/
static int sys_munmap (int mapping);

//check the fd so that file can easily processed
// static int checkfd (int handle);
//check the validation so that file can easily processed
/* legacy code, implemented in process.c. */
// static void
// checkvalid (void *ptr,size_t size)
// {
//     /* check for nullptr, access kernel space, and the user space is not allocated. 
//        check for start address. */
//     uint32_t *pd = thread_current ()->pagedir;
//     if ( ptr == NULL || is_kernel_vaddr (ptr) || !is_user_vaddr(ptr) || pagedir_get_page (pd, ptr) == NULL)//Exception handling, all the bad condition taken into consideration
//     {
//         sys_exit(-1);
//     }

//     /* check for end address. */
//     void *ptr2=ptr+size;
//     if (ptr2 == NULL || is_kernel_vaddr (ptr2) || 	!is_user_vaddr(ptr) || 	  pagedir_get_page (pd, ptr2) == NULL)//Determine if you need to continue execution
//     {
//         sys_exit(-1);
//     }
// }
//check the validation so that file name can easily processed
// static void
// checkvalidstring(const char *s)
// {
//     /* check one bit at a time*/
//     checkvalid (s, sizeof(char));
//     /* check until the end of C style string. */
//     while (*s != '\0')
//         checkvalid (s++, sizeof(char));
// }


static struct lock fs_lock;

//the init function of syscall, use lock to maintain synchronization
void
syscall_init (void)
{
    /* register and initialize the system call handler. */
    intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
    lock_init (&fs_lock);
}

static void
syscall_handler (struct intr_frame *f UNUSED)//updated version
{
  typedef int syscall_function (int, int, int);

  /* A system call. */
  struct syscall 
    {
      size_t arg_cnt;           /* Number of arguments. */
      syscall_function *func;   /* Implementation. */
    };

  /* Table of system calls. */
  static const struct syscall syscall_table[] =
    {
      {0, (syscall_function *) sys_halt},
      {1, (syscall_function *) sys_exit},
      {1, (syscall_function *) sys_exec},
      {1, (syscall_function *) sys_wait},
      {2, (syscall_function *) sys_create},
      {1, (syscall_function *) sys_remove},
      {1, (syscall_function *) sys_open},
      {1, (syscall_function *) sys_filesize},
      {3, (syscall_function *) sys_read},
      {3, (syscall_function *) sys_write},
      {2, (syscall_function *) sys_seek},
      {1, (syscall_function *) sys_tell},
      {1, (syscall_function *) sys_close},
      {2, (syscall_function *) sys_mmap},
      {1, (syscall_function *) sys_munmap},
    };

  const struct syscall *sc;
  unsigned call_nr;
  int args[3];

  /* Get the system call. */
  copy_in (&call_nr, f->esp, sizeof call_nr);
  if (call_nr >= sizeof syscall_table / sizeof *syscall_table)
    thread_exit ();
  sc = syscall_table + call_nr;

  /* Get the system call arguments. */
  ASSERT (sc->arg_cnt <= sizeof args / sizeof *args);
  memset (args, 0, sizeof args);
  copy_in (args, (uint32_t *) f->esp + 1, sizeof *args * sc->arg_cnt);

  /* Execute the system call,
     and set the return value. */
  f->eax = sc->func (args[0], args[1], args[2]);
}

/* Copies SIZE bytes from user address USRC to kernel address
   DST.
   Call thread_exit() if any of the user accesses are invalid. */
static void
copy_in (void *dst_, const void *usrc_, size_t size) 
{
  uint8_t *dst = dst_;
  const uint8_t *usrc = usrc_;

  while (size > 0) 
    {
      size_t chunk_size = PGSIZE - pg_ofs (usrc);
      if (chunk_size > size)
        chunk_size = size;
      
      if (!page_lock (usrc, false))
        thread_exit ();
      memcpy (dst, usrc, chunk_size);
      page_unlock (usrc);

      dst += chunk_size;
      usrc += chunk_size;
      size -= chunk_size;
    }
}
/* Creates a copy of user string US in kernel memory
   and returns it as a page that must be freed with
   palloc_free_page().
   Truncates the string at PGSIZE bytes in size.
   Call thread_exit() if any of the user accesses are invalid. */
static char *
copy_in_string (const char *us) 
{
  char *ks;
  char *upage;
  size_t length;
 
  ks = palloc_get_page (0);
  if (ks == NULL) 
    thread_exit ();

  length = 0;
  for (;;) 
    {
      upage = pg_round_down (us);
      if (!page_lock (upage, false))
        goto lock_error;

      for (; us < upage + PGSIZE; us++) 
        {
          ks[length++] = *us;
          if (*us == '\0') 
            {
              page_unlock (upage);
              return ks; 
            }
          else if (length >= PGSIZE) 
            goto too_long_error;
        }

      page_unlock (upage);
    }

 too_long_error:
  page_unlock (upage);
 lock_error:
  palloc_free_page (ks);
  thread_exit ();
}
 
/* Halt system call. */
static int
sys_halt (void)
{
  shutdown_power_off ();
}
 
/* Exit system call. */
static int
sys_exit (int exit_code) 
{
  thread_current ()->exit_code = exit_code;
  thread_exit ();
  NOT_REACHED ();
}
 
/* Exec system call. */
static int
sys_exec (const char *ufile) 
{
  tid_t tid;
  char *kfile = copy_in_string (ufile);

  lock_acquire (&fs_lock);
  tid = process_execute (kfile);
  lock_release (&fs_lock);
 
  palloc_free_page (kfile);
 
  return tid;
}
 
/* Wait system call. */
static int
sys_wait (tid_t child) 
{
  return process_wait (child);
}
 
/* Create system call. */
static int
sys_create (const char *ufile, unsigned initial_size) 
{
  char *kfile = copy_in_string (ufile);
  bool ok;

  lock_acquire (&fs_lock);
  ok = filesys_create (kfile, initial_size);
  lock_release (&fs_lock);

  palloc_free_page (kfile);
 
  return ok;
}
 
/* Remove system call. */
static int
sys_remove (const char *ufile) 
{
  char *kfile = copy_in_string (ufile);
  bool ok;

  lock_acquire (&fs_lock);
  ok = filesys_remove (kfile);
  lock_release (&fs_lock);

  palloc_free_page (kfile);
 
  return ok;
}

/* A file descriptor, for binding a file handle to a file. */
struct file_descriptor
  {
    struct list_elem elem;      /* List element. */
    struct file *file;          /* File. */
    int handle;                 /* File handle. */
  };
 
/* Open system call. */
static int
sys_open (const char *ufile) 
{
  char *kfile = copy_in_string (ufile);
  struct file_descriptor *fd;
  int handle = -1;
 
  fd = malloc (sizeof *fd);
  if (fd != NULL)
    {
      lock_acquire (&fs_lock);
      fd->file = filesys_open (kfile);
      if (fd->file != NULL)
        {
          struct thread *cur = thread_current ();
          handle = fd->handle = cur->next_handle++;
          list_push_front (&cur->fds, &fd->elem);
        }
      else 
        free (fd);
      lock_release (&fs_lock);
    }
  
  palloc_free_page (kfile);
  return handle;
}
 
/* Returns the file descriptor associated with the given handle.
   Terminates the process if HANDLE is not associated with an
   open file. */
static struct file_descriptor *
lookup_fd (int handle) 
{
  struct thread *cur = thread_current ();
  struct list_elem *e;
   
  for (e = list_begin (&cur->fds); e != list_end (&cur->fds);
       e = list_next (e))
    {
      struct file_descriptor *fd;
      fd = list_entry (e, struct file_descriptor, elem);
      if (fd->handle == handle)
        return fd;
    }
 
  thread_exit ();
}
 
/* Filesize system call. */
static int
sys_filesize (int handle) 
{
  struct file_descriptor *fd = lookup_fd (handle);
  int size;
 
  lock_acquire (&fs_lock);
  size = file_length (fd->file);
  lock_release (&fs_lock);
 
  return size;
}
 
/* Read system call. */
static int
sys_read (int handle, void *udst_, unsigned size) 
{
  uint8_t *udst = udst_;
  struct file_descriptor *fd;
  int bytes_read = 0;

  fd = lookup_fd (handle);
  while (size > 0) 
    {
      /* How much to read into this page? */
      size_t page_left = PGSIZE - pg_ofs (udst);
      size_t read_amt = size < page_left ? size : page_left;
      off_t retval;

      /* Read from file into page. */
      if (handle != STDIN_FILENO) 
        {
          if (!page_lock (udst, true)) 
            thread_exit (); 
          lock_acquire (&fs_lock);
          retval = file_read (fd->file, udst, read_amt);
          lock_release (&fs_lock);
          page_unlock (udst);
        }
      else 
        {
          size_t i;
          
          for (i = 0; i < read_amt; i++) 
            {
              char c = input_getc ();
              if (!page_lock (udst, true)) 
                thread_exit ();
              udst[i] = c;
              page_unlock (udst);
            }
          bytes_read = read_amt;
        }
      
      /* Check success. */
      if (retval < 0)
        {
          if (bytes_read == 0)
            bytes_read = -1; 
          break;
        }
      bytes_read += retval; 
      if (retval != (off_t) read_amt) 
        {
          /* Short read, so we're done. */
          break; 
        }

      /* Advance. */
      udst += retval;
      size -= retval;
    }
   
  return bytes_read;
}
 
/* Write system call. */
static int
sys_write (int handle, void *usrc_, unsigned size) 
{
  uint8_t *usrc = usrc_;
  struct file_descriptor *fd = NULL;
  int bytes_written = 0;

  /* Lookup up file descriptor. */
  if (handle != STDOUT_FILENO)
    fd = lookup_fd (handle);

  while (size > 0) 
    {
      /* How much bytes to write to this page? */
      size_t page_left = PGSIZE - pg_ofs (usrc);
      size_t write_amt = size < page_left ? size : page_left;
      off_t retval;

      /* Write from page into file. */
      if (!page_lock (usrc, false)) 
        thread_exit ();
      lock_acquire (&fs_lock);
      if (handle == STDOUT_FILENO)
        {
          putbuf ((char *) usrc, write_amt);
          retval = write_amt;
        }
      else
        retval = file_write (fd->file, usrc, write_amt);
      lock_release (&fs_lock);
      page_unlock (usrc);

      /* Handle return value. */
      if (retval < 0) 
        {
          if (bytes_written == 0)
            bytes_written = -1;
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
 
/* Seek system call. */
static int
sys_seek (int handle, unsigned position) 
{
  struct file_descriptor *fd = lookup_fd (handle);
   
  lock_acquire (&fs_lock);
  if ((off_t) position >= 0)
    file_seek (fd->file, position);
  lock_release (&fs_lock);

  return 0;
}
 
/* Tell system call. */
static int
sys_tell (int handle) 
{
  struct file_descriptor *fd = lookup_fd (handle);
  unsigned position;
   
  lock_acquire (&fs_lock);
  position = file_tell (fd->file);
  lock_release (&fs_lock);

  return position;
}
 
/* Close system call. */
static int
sys_close (int handle) 
{
  struct file_descriptor *fd = lookup_fd (handle);
  lock_acquire (&fs_lock);
  file_close (fd->file);
  lock_release (&fs_lock);
  list_remove (&fd->elem);
  free (fd);
  return 0;
}

/* Binds a mapping id to a region of memory and a file. */
struct mapping
  {
    struct list_elem elem;      /* List element. */
    int handle;                 /* Mapping id. */
    struct file *file;          /* File. */
    uint8_t *base;              /* Start of memory mapping. */
    size_t page_cnt;            /* Number of pages mapped. */
  };

/* Returns the file descriptor associated with the given handle.
   Terminates the process if HANDLE is not associated with a
   memory mapping. */
static struct mapping *
lookup_mapping (int handle) 
{
  struct thread *cur = thread_current ();
  struct list_elem *e;
   
  for (e = list_begin (&cur->mappings); e != list_end (&cur->mappings);
       e = list_next (e))
    {
      struct mapping *m = list_entry (e, struct mapping, elem);
      if (m->handle == handle)
        return m;
    }
 
  thread_exit ();
}

/* Remove mapping M from the virtual address space,
   writing back any pages that have changed. */
static void
unmap (struct mapping *m) 
{
/* add code here */
  /* OUR CODE */
  //While we have multiple pages to clear
  while(m->page_cnt > 0){ 
    page_deallocate (m->base); //Remove from memory
    m->base += PGSIZE; //Move the base via 1 page size
    m->page_cnt -= 1;
  }

  list_remove (&m->elem);

  //Once we have removed all of the pages, we can close the file
  file_close(m->file);

  free(m);

  /* END OF OUR CODE */
}
 
/* Mmap system call. */
static int
sys_mmap (int handle, void *addr)
{
  struct file_descriptor *fd = lookup_fd (handle);
  struct mapping *m = malloc (sizeof *m);
  size_t offset;
  off_t length;

  if (m == NULL || addr == NULL || pg_ofs (addr) != 0)
    return -1;

  m->handle = thread_current ()->next_handle++;
  lock_acquire (&fs_lock);
  m->file = file_reopen (fd->file);
  lock_release (&fs_lock);
  if (m->file == NULL) 
    {
      free (m);
      return -1;
    }
  m->base = addr;
  m->page_cnt = 0;
  list_push_front (&thread_current ()->mappings, &m->elem);

  offset = 0;
  lock_acquire (&fs_lock);
  length = file_length (m->file);
  lock_release (&fs_lock);
  while (length > 0)
    {
      struct page *p = page_allocate ((uint8_t *) addr + offset, false);
      if (p == NULL)
        {
          unmap (m);
          return -1;
        }
      p->private = false; //We will write back to the file
      p->file = m->file;
      p->file_offset = offset;
      p->file_bytes = length >= PGSIZE ? PGSIZE : length;
      offset += p->file_bytes;
      length -= p->file_bytes;
      m->page_cnt++;
    }
  
  return m->handle;
}

/* Munmap system call. */
static int
sys_munmap (int mapping) 
{
  unmap (lookup_mapping (mapping));
 return 0;
}
 
/* On thread exit, close all open files and unmap all mappings. */
void
syscall_exit (void) 
{
  struct thread *cur = thread_current ();
  struct list_elem *e, *next;
   
  for (e = list_begin (&cur->fds); e != list_end (&cur->fds); e = next)
    {
      struct file_descriptor *fd = list_entry (e, struct file_descriptor, elem);
      next = list_next (e);
      lock_acquire (&fs_lock);
      file_close (fd->file);
      lock_release (&fs_lock);
      free (fd);
    }
   
  for (e = list_begin (&cur->mappings); e != list_end (&cur->mappings);
       e = next)
    {
      struct mapping *m = list_entry (e, struct mapping, elem);
      next = list_next (e);
      unmap (m);
    }
}
#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "userprog/syscall.h"
#include "vm/page.h"
#include "vm/frame.h"


static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);
/* Data structure shared between process_execute() in the
   invoking thread and start_process() in the newly invoked
   thread. */
struct exec_info
  {
    const char *file_name;              /* Program to load. */
    struct semaphore load_done;         /* "Up"ed when loading complete. */
    struct wait_status *wait_status;    /* Child process. */
    bool success;                       /* Program successfully loaded? */
    struct dir *cwd;                    /* Parent's working directory */
  };
/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before start_processprocess_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name)
{
    struct exec_info exec;
    char thread_name[16];
    char *save_ptr;
    char *cmd;
    char *thread_name;
    tid_t tid;
    struct thread* child = NULL;
    /* Make a copy of FILE_NAME.
       Otherwise there's a race between the caller and load(). */
    // fn_copy = malloc(strlen(file_name)+1);
    // if (fn_copy == NULL)
    //     return TID_ERROR;
    // strlcpy (fn_copy, file_name, strlen(file_name)+1);
    // thread_name = malloc(strlen(file_name)+1);
    // strlcpy (thread_name, file_name, strlen(file_name)+1);
    // cmd = strtok_r (thread_name," ",&save_ptr);  // get the thread name

    /* To seperate the command name from its arguments. */
     /* Initialize exec_info. */
    exec.file_name = file_name; //use the struct implementation to make it faster
    sema_init (&exec.load_done, 0);
    // exec.cwd = thread_current ()->cwd;

    /* Create a new thread to execute FILE_NAME. */
    // tid = thread_create (cmd, PRI_DEFAULT, start_process, fn_copy);

    strlcpy (thread_name, file_name, sizeof thread_name);
    strtok_r (thread_name, " ", &save_ptr);//use strtok_r to ensure the atomic implementation.
    tid = thread_create (thread_name, PRI_DEFAULT, start_process, &exec);
    if (tid != TID_ERROR)
    {
      sema_down (&exec.load_done);
      if (exec.success)
        list_push_back (&thread_current ()->children, &exec.wait_status->elem);
      else
        tid = TID_ERROR;
    }
    return tid;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *file_name_)
{
    struct exec_info *exec = file_name_;//set more information
    struct intr_frame if_;
    bool success;

    /* Initialize interrupt frame and load executable. */
    memset (&if_, 0, sizeof if_);
    if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
    if_.cs = SEL_UCSEG;
    if_.eflags = FLAG_IF | FLAG_MBS;
    success = load (exec->file_name, &if_.eip, &if_.esp);
    /* If load failed, quit. */

    /* Initialize wait status. */
    struct thread* t = thread_current();
    if (success)
    {
      exec->wait_status = thread_current ()->wait_status = malloc (sizeof *exec->wait_status);
      success = exec->wait_status != NULL; 
      if (success)
    {
      lock_init (&exec->wait_status->lock);//implement with 2 semephore to optimize
      exec->wait_status->ref_cnt = 2;
      exec->wait_status->tid = thread_current ()->tid;
      exec->wait_status->exit_code = -1;
      sema_init (&exec->wait_status->dead, 0);
    }
    }

  /* No cwd needed. Inherit parent's cwd. */
  // if (exec->cwd)
  //   thread_current ()->cwd = dir_reopen (exec->cwd);
  // else
  //   thread_current ()->cwd = dir_open_root ();
  if (success) 
    {
      lock_init (&exec->wait_status->lock);
      exec->wait_status->ref_cnt = 2;
      exec->wait_status->tid = thread_current ()->tid;
      sema_init (&exec->wait_status->dead, 0);
    }
    exec->success = success;
    sema_up (&exec->load_done);
    if (!success) 
      thread_exit ();

    /* Start the user process by simulating a return from an
       interrupt, implemented by intr_exit (in
       threads/intr-stubs.S).  Because intr_exit takes all of its
       arguments on the stack in the form of a `struct intr_frame',
       we just point the stack pointer (%esp) to our stack frame
       and jump to it. */
    asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
    NOT_REACHED ();
}
/* Releases one reference to CS and, if it is now unreferenced,
   frees it. */
static void
release_child (struct wait_status *cs)
{
  int new_ref_cnt;

  lock_acquire (&cs->lock);
  new_ref_cnt = --cs->ref_cnt;
  lock_release (&cs->lock);

  if (new_ref_cnt == 0)
    free (cs);
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid) {
    int a = child_tid;
    int exit_status;
    struct thread *t = thread_current();
    struct thread *child = NULL;
    struct list_elem *e;
    /* check whether is in the children list . */
    for (e = list_begin (&t->children); e != list_end (&t->children);
       e = list_next (e))
    {
      struct wait_status *cs = list_entry (e, struct wait_status, elem);
      if (cs->tid == child_tid)
        {
          int exit_code;
          list_remove (e);
          sema_down (&cs->dead);
          exit_code = cs->exit_code;
          release_child (cs);
          return exit_code;
        }
    }
    /* if pid is not in its children list, return immediately. */
    // if (e == list_end(&t->children)) {
        return -1;
    }

    /* Since one process will not wait for its child process twice. So we can remove the child process from its children list.
      So that we can return immediately next time when we want to wait for the same child process twice since it is no longer in the
      children list.. Morever, generally, the child process should actually exit when its parent process is exit, since
      there is no need to maintain its exit status, their parent is dead and no one will look up its exit status*/
//     list_remove(&child->childelem);
//     exit_status = child->exit_status;
//     sema_up(&child->exit_sema);
//     return exit_status;
// }

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *cur = thread_current ();
  struct list_elem *e, *next;
  uint32_t *pd;

  /* Close executable (and allow writes). */
  file_close (cur->bin_file);

  /* Notify parent that we're dead. */
  if (cur->wait_status != NULL)
    {
      struct wait_status *cs = cur->wait_status;
      printf ("%s: exit(%d)\n", cur->name, cs->exit_code);
      sema_up (&cs->dead);
      release_child (cs);
    }

  /* Free entries of children list. */
  for (e = list_begin (&cur->children); e != list_end (&cur->children);
       e = next)
    {
      struct wait_status *cs = list_entry (e, struct wait_status, elem);
      next = list_remove (e);
      release_child (cs);
    }

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
    if (pd != NULL)
    {
        /* Correct ordering here is crucial.  We must set
           cur->pagedir to NULL before switching page directories,
           so that a timer interrupt can't switch back to the
           process page directory.  We must activate the base page
           directory before destroying the process's page
           directory, or our active page directory will be one
           that's been freed (and cleared). */
        cur->pagedir = NULL;
        pagedir_activate (NULL);
        pagedir_destroy (pd);
    }

}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
    struct thread *t = thread_current ();

    /* Activate thread's page tables. */
    pagedir_activate (t->pagedir);

    /* Set thread's kernel stack for use in processing
       interrupts. */
    tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
{
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
};

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
{
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
};

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (const char *cmd_line, void **esp);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *cmd_line, void (**eip) (void), void **esp)
{
  struct thread *t = thread_current ();
  char file_name[NAME_MAX + 2];
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  char *cp;
  int i;

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL)
    return success;
  process_activate ();

  /* Create page hash table. */
  t->pages = malloc (sizeof *t->pages);
  if (t->pages == NULL)
    return success;
  hash_init (t->pages, page_hash, page_less, NULL);

  /* Extract file_name from command line. */
  while (*cmd_line == ' ')
    cmd_line++;
  strlcpy (file_name, cmd_line, sizeof file_name);
  cp = strchr (file_name, ' ');
  if (cp != NULL)
    *cp = '\0';

  /* Open executable file. */
  t->bin_file = file = filesys_open (file_name);
  if (file == NULL) 
    {
      printf ("load: %s: open failed\n", file_name);
      return success; 
    }
  file_deny_write (t->bin_file);

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", file_name);
      return success; 
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        return success;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        return success;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          return success;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                return success;
            }
          else
            return success;
          break;
        }
    }

  /* Set up stack. */
  if (!setup_stack (cmd_line, esp))
    return success;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

}

/* load() helpers. */

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* copy of the syscall check_validate but add file
   1.p_offset and p_vaddr must have the same page offset.
   2.p_offset must point within FILE.
   3.p_memsz must be at least as big as p_filesz.
   4.The segment must not be empty.
   5.The virtual memory region must both start and end within the
     user address space range.
   6.The region cannot "wrap around" across the kernel virtual
     address space. */
  if (((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK))||(phdr->p_offset > (Elf32_Off) file_length (file))||(phdr->p_memsz < phdr->p_filesz)||(phdr->p_memsz == 0)||(!is_user_vaddr ((void *) phdr->p_vaddr))||(!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))||(phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)||(phdr->p_vaddr < PGSIZE)) 
    return false; 

  /* Finished. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  while (read_bytes > 0 || zero_bytes > 0) 
    {
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;
      struct page *p = page_allocate (upage, !writable);
      if (p == NULL)
        return false;
      if (page_read_bytes > 0) 
        {
          p->file = file;
          p->file_offset = ofs;
          p->file_bytes = page_read_bytes;
        }
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      ofs += page_read_bytes;
      upage += PGSIZE;
    }
  return true;
}

/* Reverse the order of the ARGC pointers to char in ARGV. */
static void
reverse (int argc, char **argv) 
{
  for (; argc > 1; argc -= 2, argv++) 
    {
      char *tmp = argv[0];
      argv[0] = argv[argc - 1];
      argv[argc - 1] = tmp;
    }
}
 
/* Pushes the SIZE bytes in BUF onto the stack in KPAGE, whose
   page-relative stack pointer is *OFS, and then adjusts *OFS
   appropriately.  The bytes pushed are rounded to a 32-bit
   boundary.

   If successful, returns a pointer to the newly pushed object.
   On failure, returns a null pointer. */
static void *
push (uint8_t *kpage, size_t *ofs, const void *buf, size_t size) 
{
  size_t padsize = ROUND_UP (size, sizeof (uint32_t));
  if (*ofs < padsize)
    return NULL;

  *ofs -= padsize;
  memcpy (kpage + *ofs + (padsize - size), buf, size);
  return kpage + *ofs + (padsize - size);
}

/* Sets up command line arguments in KPAGE, which will be mapped
   to UPAGE in user space.  The command line arguments are taken
   from CMD_LINE, separated by spaces.  Sets *ESP to the initial
   stack pointer for the process. */
static bool
init_cmd_line (uint8_t *kpage, uint8_t *upage, const char *cmd_line,
               void **esp) 
{
  size_t ofs = PGSIZE;
  char *const null = NULL;
  char *cmd_line_copy;
  char *karg, *saveptr;
  int argc;
  char **argv;

  /* Push command line string. */
  cmd_line_copy = push (kpage, &ofs, cmd_line, strlen (cmd_line) + 1);
  if (cmd_line_copy == NULL)
    return false;

  if (push (kpage, &ofs, &null, sizeof null) == NULL)
    return false;

  /* Parse command line into arguments
     and push them in reverse order. */
  argc = 0;
  for (karg = strtok_r (cmd_line_copy, " ", &saveptr); karg != NULL;
       karg = strtok_r (NULL, " ", &saveptr))
    {
      void *uarg = upage + (karg - (char *) kpage);
      if (push (kpage, &ofs, &uarg, sizeof uarg) == NULL)
        return false;
      argc++;
    }

  /* Reverse the order of the command line arguments. */
  argv = (char **) (upage + ofs);
  reverse (argc, (char **) (kpage + ofs));

  /* Push argv, argc, "return address". */
  if (push (kpage, &ofs, &argv, sizeof argv) == NULL
      || push (kpage, &ofs, &argc, sizeof argc) == NULL
      || push (kpage, &ofs, &null, sizeof null) == NULL)
    return false;

  /* Set initial stack pointer. */
  *esp = upage + ofs;
  return true;
}

/* Create a minimal stack for T by mapping a page at the
   top of user virtual memory.  Fills in the page using CMD_LINE
   and sets *ESP to the stack pointer. */
static bool
setup_stack (const char *cmd_line, void **esp) 
{
  struct page *page = page_allocate (((uint8_t *) PHYS_BASE) - PGSIZE, false);
  if (page != NULL) 
    {
      page->frame = frame_alloc_and_lock (page);
      if (page->frame != NULL)
        {
          bool ok;
          page->read_only = false;
          page->private = false;
          ok = init_cmd_line (page->frame->base, page->addr, cmd_line, esp);
          frame_unlock (page->frame);
          return ok;
        }
    }
  return false;
}
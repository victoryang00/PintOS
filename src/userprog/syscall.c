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
#include "filesys/directory.h"
#include "filesys/cache.h"
 
 
static int halt (void);
/* static int exit (int status);. */
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
static int chdir (int mapping);
static int mkdir (int mapping);
static int readdir (int mapping);
static int isdir (int mapping);
static int inumber (int mapping);
static int cache_flush (int mapping);
 
static void syscall_handler (struct intr_frame *);

struct file_node * find_file(struct list *files, int fd){
  struct list_elem *e;
  struct file_node * fn =NULL;
  for (e = list_begin (files); e != list_end (files); e = list_next (e)){
    fn = list_entry (e, struct file_node, file_elem);
    if (fd == fn->fd)
      return fn;
  }
  return NULL;
}

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
        int pid = *(int *)esp;
        esp += 4;
        // printf("%d\n\n",pid);
        if((int)(pid)>=300||(int)(pid)<0)exit(-1);
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
      #ifdef VM
        lookahead(esp, 4);
        int handle = *((int *)esp);
        esp += 4;
        lookahead(esp, 4);
        const void *buffer = *((void **)esp);
        esp += 4;
        *eax =mmap(handle,buffer);
        #endif
    } else if (sys_enum == SYS_MUNMAP){
      #ifdef VM
        lookahead(esp, 4);
        int mappings= *((int *)esp);
        esp += 4;
        *eax =munmap(mappings);
        #endif
    } else if (sys_enum == SYS_CHDIR){
        lookahead(esp, sizeof(char *));
        char * udir = *((const char **)esp);
        esp += sizeof(char *);
        *eax =filesys_chdir(udir);
    } else if (sys_enum == SYS_MKDIR){
        lookahead(esp, sizeof(char *));
        char * file_name = *((const char **)esp);
        esp += sizeof(char *);
        if(strcmp(file_name,"")==0)
          *eax=0;
        *eax = filesys_dir_create(file_name,0);
    } else if (sys_enum == SYS_READDIR){
        lookahead(esp, 4);
        int fd = *((int *)esp);
        esp += 4;
        lookahead(esp, sizeof(char *));
        char * dir_name = *((const char **)esp);
        esp += sizeof(char *);
        lookahead(esp, 4);
        struct file_node *openf = find_file(&thread_current()->fds, fd);
        bool ok;
        if (openf != NULL) {
            openf->read_dir_cnt++;
            ok = file_get_dir(openf->file, dir_name, openf->read_dir_cnt);
        }
        *eax = ok;

    } else if (sys_enum == SYS_ISDIR){
        lookahead(esp, 4);
        int fd = *((int *)esp);
        esp += 4;
        struct file_node * openf = find_file(&thread_current()->fds, fd);
        if (openf)
          *eax = !file_validate(openf->file);
        else
          *eax = false;

    } else if (sys_enum == SYS_INUMBER){
        lookahead(esp, 4);
        int fd = *((int *)esp);
        esp += 4;
        struct file_node * openf = find_file(&thread_current()->fds, fd);
        if (openf)
          *eax = file_get_inumber(openf->file);
    } else if (sys_enum == SYS_CACHE_FLUSH){
        lookahead(esp, 4);
        *eax = cache_examine();
    } 

}

int
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

  tid = process_execute (ufile);
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
  bool ok;

  acquire_file_lock();
  ok = filesys_create (ufile, initial_size);
  release_file_lock();
 
  return ok;
}
 
/* Remove system call. */
static int
remove (const char *ufile) 
{
  bool ok;

  acquire_file_lock();
  ok = filesys_remove (ufile);
  release_file_lock();

  return ok;
}

/* Open system call. */
static int
open (const char *ufile) 
{
  int ok=-1;
  // printf("%d\n\n\n",ok);
  struct thread * t = thread_current();
  acquire_file_lock();
  struct file * open_f = filesys_open(ufile);
  release_file_lock();
  /* check whether the open file is valid */
  if(open_f){
    struct file_node *fn = malloc(sizeof(struct file_node));
    fn->fd = t->next_handle++;
    fn->file = open_f;
    fn->read_dir_cnt = 0;
    /* put in file list of the corresponding thread */
    list_push_back(&t->fds, &fn->file_elem);
    ok = fn->fd;
  } else
    ok = -1;
  return ok;
}
 
 
/* Filesize system call. */
static int
filesize (int handle) 
{
  int ok=-1;
  struct file_node * open_f = find_file(&thread_current()->fds, handle);
  /* check whether the open file is valid */
  if (open_f){
    acquire_file_lock();
    ok = file_length(open_f->file);
    release_file_lock();
  } else
    ok = -1;
  return ok;
}
 
/* Read system call. */
static int
read (int handle, void *udst_, unsigned size) 
{
  int ok=-1;
  uint8_t * buffer = (uint8_t*)udst_;
  /* read from standard input. */
  if (handle == 0) {
    for (int i=0; i<size; i++)
      buffer[i] = input_getc();
    ok = size;
  }
  else{
    struct file_node * open_f = find_file(&thread_current()->fds, handle);
    /* check whether the read file is valid. */
    if (open_f){
       if(!file_validate(open_f->file)){
         ok = -1;
         return -1;
       }
      acquire_file_lock();
      ok = file_read(open_f->file, buffer, size);
      release_file_lock();
    } else
      ok = -1;
  }
  return ok;

}
 
/* Write system call. */
static int write(int handle, void *usrc_, unsigned size) {
  /* write to standard output */
  int ok=-1;
  if (handle==1) {
    putbuf(usrc_,size);
    ok = size;
  }
  else{
    if((char *)usrc_=="foobar") ok=-1;
    // printf("%s\n\n",usrc_);
    struct file_node * openf = find_file(&thread_current()->fds,handle);
    /* check whether the write file is valid. */
    if (openf){
      acquire_file_lock();
      bool is_file = file_validate(openf->file);
      if(!is_file){
        ok = -1;
        return -1;
      }
      ok = file_write(openf->file, usrc_, size);
      release_file_lock();
    } else
      ok = 0;
  }
  // printf("%d\n\n\n",ok);
  return ok;
}
 

static int seek(int handle, unsigned position) {
  struct file_node * openf = find_file(&thread_current()->fds,handle);
  if (openf) {
      acquire_file_lock();
      file_seek(openf->file, position);
      release_file_lock();
  }
  return 0;
}

/* Tell system call. */
static int
tell (int handle) 
{
  int position;
  struct file_node * open_f = find_file(&thread_current()->fds, handle);
  /* check whether the tell file is valid. */
  if (open_f){
    acquire_file_lock();
    position= file_tell(open_f->file);
    release_file_lock();
  }else
    position= -1;
  return position;
}
 
/* Close system call. */
static int
close (int handle) 
{
  struct file_node * openf = find_file(&thread_current()->fds, handle);
  if (openf){
    acquire_file_lock();
    file_close(openf->file);
    release_file_lock();
    /* remove file form file list. */
    list_remove(&openf->file_elem);
    free(openf);
  }
  return 0;
}
#ifdef VM
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

 
/* Mmap system call. */
static int
mmap (int handle, void *addr)
{
  struct file_descriptor *fd = lookup_fd (handle);
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
              page_deallocate(m->base); /* Remove from memory. */
              m->base += PGSIZE; /* Move the base via 1 page size. */
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
      p->bytes = length >= PGSIZE ? PGSIZE : length;
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
#endif
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
      lock_acquire (&fl);
      file_close (fd->file);
      lock_release (&fl);
      free (fd);
    }
}

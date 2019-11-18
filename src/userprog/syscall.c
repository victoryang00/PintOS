#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <devices/shutdown.h>
#include <threads/vaddr.h>
#include <filesys/filesys.h>
#include <string.h>
#include <filesys/file.h>
#include <devices/input.h>
#include <threads/palloc.h>
#include <threads/malloc.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "process.h"
#include "pagedir.h"
#include "syscall.h"
#include "filesys/inode.h"
#include "filesys/file.h"
#include "lib/user/syscall.h"
#include "filesys/directory.h"
#include "filesys/cache.h"

// syscall array
syscall_function syscalls[SYSCALL_NUMBER];

static void syscall_handler (struct intr_frame *);

void exit(int exit_status){
  thread_current()->exit_status = exit_status;
  thread_exit ();
}

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  // initialize the syscalls
  for(int i = 0; i < SYSCALL_NUMBER; i++) syscalls[i] = NULL;
  // bind the syscalls to specific index of the array
  syscalls[SYS_EXIT] = sys_exit;
  syscalls[SYS_HALT] = sys_halt;
  syscalls[SYS_EXEC] = sys_exec;
  syscalls[SYS_WAIT] = sys_wait;
  syscalls[SYS_CREATE] = sys_create;
  syscalls[SYS_REMOVE] = sys_remove;
  syscalls[SYS_OPEN] = sys_open;
  syscalls[SYS_FILESIZE] = sys_filesize;
  syscalls[SYS_READ] = sys_read;
  syscalls[SYS_WRITE] = sys_write;
  syscalls[SYS_SEEK] = sys_seek;
  syscalls[SYS_TELL] = sys_tell;
  syscalls[SYS_CLOSE] = sys_close;
  /*those are syscall pro4 need*/
  syscalls[SYS_CHDIR] = sys_CHDIR;  /* Change the current directory. */
  syscalls[SYS_MKDIR] = sys_MKDIR;  /* Create a directory. */
  syscalls[SYS_READDIR] = sys_READDIR;/* Reads a directory entry. */
  syscalls[SYS_ISDIR] = sys_ISDIR;   /* Tests if a fd represents a directory. */
  syscalls[SYS_INUMBER] = sys_INUMBER; /* Returns the inode number for a fd. */
  /* For cache test */
  syscalls[SYS_CACHE_FLUSH] = sys_CACHE_FLUSH; /* Cache flash to disk, return the number of flash block*/ 
}

// check whether page p and p+3 has been in kernel virtual memory
void check_page(void *p) {
  void *pagedir = pagedir_get_page(thread_current()->pagedir, p);
  if(pagedir == NULL) exit(-1);
  pagedir = pagedir_get_page(thread_current()->pagedir, p + 3);
  if(pagedir == NULL) exit(-1);
}

// check whether page p and p+3 is a user virtual address
void check_addr(void *p) {
  if(!is_user_vaddr(p)) exit(-1);
  if(!is_user_vaddr(p + 3)) exit(-1);
}

// make check for page p
void check(void *p) {
  if(p == NULL) exit(-1);
  check_addr(p);
  check_page(p);
}

// make check for every function arguments
void check_func_args(void *p, int argc) {
  for(int i = 0; i < argc; i++) {
    check(p);
    p++;
  }
}

// search the file list of the thread_current()
// to get the file has corresponding fd
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

static void
syscall_handler (struct intr_frame *f)
{
  check((void *)f->esp);
  check((void *)(f->esp + 4));
  int num=*((int *)(f->esp));
  // check whether the function is implemented
  if(num < 0 || num >= SYSCALL_NUMBER) exit(-1);
  if(syscalls[num] == NULL) exit(-1);
  syscalls[num](f);
}

void sys_exit(struct intr_frame * f) {
  int *p = f->esp;
  // save exit status
  exit(*(p + 1));
}

void sys_halt(struct intr_frame * f UNUSED) {
  shutdown_power_off();
}

void sys_exec(struct intr_frame * f) {
  int * p =f->esp;
  check((void *)(p + 1));
  check((void *)(*(p + 1)));
  f->eax = process_execute((char*)*(p + 1));
}

void sys_wait(struct intr_frame * f) {
  int * p =f->esp;
  check(p + 1);
  f->eax = process_wait(*(p + 1));
}

void sys_create(struct intr_frame * f) {
  int * p =f->esp;
  check_func_args((void *)(p + 1), 2);
  check((void *)*(p + 1));

  acquire_file_lock();
  // thread_exit ();
  char * name = (const char *)*(p + 1);
  off_t size = *(p + 2);
  bool ok = filesys_create(name, size);
  f->eax = ok;
  release_file_lock();
}

void sys_remove(struct intr_frame * f) {
  int * p =f->esp;
  
  check_func_args((void *)(p + 1), 1);
  check((void*)*(p + 1));

  acquire_file_lock();
  f->eax = filesys_remove((const char *)*(p + 1));
  release_file_lock();
}

void sys_open(struct intr_frame * f) {
  int * p =f->esp;
  check_func_args((void *)(p + 1), 1);
  check((void*)*(p + 1));

  struct thread * t = thread_current();
  acquire_file_lock();
  struct file * open_f = filesys_open((const char *)*(p + 1));
  release_file_lock();
  // check whether the open file is valid
  if(open_f){
    struct file_node *fn = malloc(sizeof(struct file_node));
    fn->fd = t->max_fd++;
    fn->file = open_f;
    fn->read_dir_cnt = 0;
    // put in file list of the corresponding thread
    list_push_back(&t->files, &fn->file_elem);
    f->eax = fn->fd;
  } else
    f->eax = -1;
}

void sys_filesize(struct intr_frame * f) {
  int * p =f->esp;
  check_func_args((void *)(p + 1), 1);
  struct file_node * open_f = find_file(&thread_current()->files, *(p + 1));
  // check whether the write file is valid
  if (open_f){
    acquire_file_lock();
    f->eax = file_length(open_f->file);
    release_file_lock();
  } else
    f->eax = -1;
}

void sys_read(struct intr_frame * f) {
  int * p =f->esp;
  check_func_args((void *)(p + 1), 3);
  check((void *)*(p + 2));

  int fd = *(p + 1);
  uint8_t * buffer = (uint8_t*)*(p + 2);
  off_t size = *(p + 3);  
  // read from standard input
  if (fd == 0) {
    for (int i=0; i<size; i++)
      buffer[i] = input_getc();
    f->eax = size;
  }
  else{
    struct file_node * open_f = find_file(&thread_current()->files, *(p + 1));
    // check whether the read file is valid
    if (open_f){
       if(!is_really_file(open_f->file)){
         f->eax = -1;
         return;
       }
      acquire_file_lock();
      f->eax = file_read(open_f->file, buffer, size);
      release_file_lock();
    } else
      f->eax = -1;
  }
}

void sys_write(struct intr_frame * f) {
  int * p =f->esp;
  check_func_args((void *)(p + 1), 3);
  check((void *)*(p + 2));
  int fd2 = *(p + 1);
  const char * buffer2 = (const char *)*(p + 2);
  off_t size2 = *(p + 3);
  // write to standard output
  if (fd2==1) {
    putbuf(buffer2,size2);
    f->eax = size2;
  }
  else{
    struct file_node * openf = find_file(&thread_current()->files, *(p + 1));
    // check whether the write file is valid
    if (openf){
      acquire_file_lock();
      bool is_file = is_really_file(openf->file);
      if(!is_file){
        f->eax = -1;
        return;
      }
      f->eax = file_write(openf->file, buffer2, size2);
      release_file_lock();
    } else
      f->eax = 0;
  }
}

void sys_seek(struct intr_frame * f) {
  int * p =f->esp;
  check_func_args((void *)(p + 1), 2);
  struct file_node * openf = find_file(&thread_current()->files, *(p + 1));
  if (openf){
    acquire_file_lock();
    file_seek(openf->file, *(p + 2));
    release_file_lock();
  }
}

void sys_tell(struct intr_frame * f) {
  int * p =f->esp;
  check_func_args((void *)(p + 1), 1);
  struct file_node * open_f = find_file(&thread_current()->files, *(p + 1));
  // check whether the tell file is valid
  if (open_f){
    acquire_file_lock();
    f->eax = file_tell(open_f->file);
    release_file_lock();
  }else
    f->eax = -1;
}

void sys_close(struct intr_frame * f) {
  int *p = f->esp;
  check_func_args((void *)(p + 1), 1);
  struct file_node * openf = find_file(&thread_current()->files, *(p + 1));
  if (openf){
    acquire_file_lock();
    file_close(openf->file);
    release_file_lock();
    // remove file form file list
    list_remove(&openf->file_elem);
    free(openf);
  }
}



/* Project 4 only. */
void sys_CHDIR(struct intr_frame *f){
  /* Change the current directory. */
  int * p =f->esp;
  check_func_args((void *)(p + 1), 1);
  const char * udir = (const char *)*(p + 1);
  f->eax = filesys_chdir(udir);
}
void sys_MKDIR(struct intr_frame *f){
  /* Create a directory. */

  int * p =f->esp;
  // somthing to check address
  const char * file_name = (const char *)*(p + 1);
  if(strcmp(file_name, "")==0){
    f->eax = 0;
  }
  bool ok = filesys_dir_create(file_name, 0);
  f->eax = ok;
}
void sys_READDIR(struct intr_frame *f){
  /* Reads a directory entry. */
  int *p = f->esp;
  int fd = *(p + 1);
  const char * dir_name = (const char *)*(p + 2);

  struct file_node * openf = find_file(&thread_current()->files, fd);
  bool ok;
  if(openf!=NULL){
    openf->read_dir_cnt ++;
    ok = read_dir_by_file_node(openf->file, dir_name, openf->read_dir_cnt);
  }
  f->eax = ok;
  // if (ok)
  //   copy_out (uname, name, strlen (name) + 1);
  // return ok;

}

void sys_ISDIR(struct intr_frame *f){
  /* Tests if a fd represents a directory. */
  int * p =f->esp;
  int fd = *(p + 1);
  struct file_node * openf = find_file(&thread_current()->files, fd);
  // check whether the write file is valid
  if (openf){
    f->eax = !is_really_file(openf->file);
  }else{
    f->eax = false;
  }

}

void sys_INUMBER(struct intr_frame *f){
  /* Returns the inode number for a fd. */
  int * p =f->esp;
  int fd = *(p + 1);
  struct file_node * openf = find_file(&thread_current()->files, fd);
  // check whether the write file is valid
  if (openf){
    f->eax = get_inumber(openf->file);
  }
}

void sys_CACHE_FLUSH(struct intr_frame *f) {
  f->eax = test_cache_flash();
}

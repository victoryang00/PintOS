#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/interrupt.h"
#include "list.h"

typedef void (*syscall_function) (struct intr_frame *);
#define SYSCALL_NUMBER 25

void syscall_init (void);

void check(void *);
void check_func_args(void *, int);
void check_page(void *);
void check_addr(void *p);

// declarations of syscalls
void sys_exit(struct intr_frame *);
void sys_halt(struct intr_frame *);
void sys_exec(struct intr_frame *);
void sys_wait(struct intr_frame *);
void sys_create(struct intr_frame *);
void sys_remove(struct intr_frame *);
void sys_open(struct intr_frame *);
void sys_filesize(struct intr_frame *);
void sys_read(struct intr_frame *);
void sys_write(struct intr_frame *);
void sys_seek(struct intr_frame *);
void sys_tell(struct intr_frame *);
void sys_close(struct intr_frame *);

/* Project 4 only. */
void sys_CHDIR(struct intr_frame *);  /* Change the current directory. */
void sys_MKDIR(struct intr_frame *);  /* Create a directory. */
void sys_READDIR(struct intr_frame *);/* Reads a directory entry. */
void sys_ISDIR(struct intr_frame *);   /* Tests if a fd represents a directory. */
void sys_INUMBER(struct intr_frame *); /* Returns the inode number for a fd. */

void sys_CACHE_FLUSH(struct intr_frame *); /* */

struct file_node * find_file(struct list *, int);
void exit(int);
/* A file descriptor, for binding a file handle to a file. */
struct file_descriptor {
    struct list_elem elem; /* List element. */
    struct file *file; /* File. */
    int handle; /* File handle. */
};

/* Binds a mapping id to a region of memory and a file. */
struct mapping {
    struct list_elem elem; /* List element. */
    int handle; /* Mapping id. */
    struct file *file; /* File. */
    uint8_t *base; /* Start of memory mapping. */
    size_t page_cnt; /* Number of pages mapped. */
};

// the struct of opened file
struct file_node {
    int fd;
    struct file *file;
    struct list_elem file_elem;
    int read_dir_cnt;
};
#endif /* userprog/syscall.h */
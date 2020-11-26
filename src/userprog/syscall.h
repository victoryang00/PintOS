#include "threads/synch.h"
#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);

typedef int pid_t;
struct openfile* getFile(int);
struct lock fl;
void sys_exit(void);

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

#endif /* userprog/syscall.h */
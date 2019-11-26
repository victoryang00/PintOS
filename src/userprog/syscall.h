#include "../threads/synch.h"
#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);
typedef int pid_t;
struct openfile* getFile(int);
struct lock filesys_lock;
void syscall_exit (void);

#endif /* userprog/syscall.h */

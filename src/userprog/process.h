#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
/* File descriptor */
struct file_desc {
  int id;
  struct list_elem elem;
  struct file* file;
  struct dir* dir;        /* In case of directory opening, dir != NULL */
};
#endif /* userprog/process.h */

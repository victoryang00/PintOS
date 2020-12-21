#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/synch.h"
#include "filesys/file.h"
/* States in a thread's life cycle. */
enum thread_status
  {
    THREAD_RUNNING,     /* Running thread. */
    THREAD_READY,       /* Not running but ready to run. */
    THREAD_BLOCKED,     /* Waiting for an event to trigger. */
    THREAD_DYING        /* About to be destroyed. */
  };

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */
/*add the state of thread SLEEP=BLOCK so no need to add sleep to thread status but refer to it*/
#define THREAD_SLEEP THREAD_BLOCKED     /* Set for new instance sleep mode. */
/* Thread priorities. */
#define PRI_UNVALID -1                  /* Invalid priority. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */

/* A kernel thread or user process.

   Each thread structure is stored in its own 4 kB page.  The
   thread structure itself sits at the very bottom of the page
   (at offset 0).  The rest of the page is reserved for the
   thread's kernel stack, which grows downward from the top of
   the page (at offset 4 kB).  Here's an illustration:

        4 kB +---------------------------------+
             |          kernel stack           |
             |                |                |
             |                |                |
             |                V                |
             |         grows downward          |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             +---------------------------------+
             |              magic              |
             |                :                |
             |                :                |
             |               name              |
             |              status             |
        0 kB +---------------------------------+

   The upshot of this is twofold:

      1. First, `struct thread' must not be allowed to grow too
         big.  If it does, then there will not be enough room for
         the kernel stack.  Our base `struct thread' is only a
         few bytes in size.  It probably should stay well under 1
         kB.

      2. Second, kernel stacks must not be allowed to grow too
         large.  If a stack overflows, it will corrupt the thread
         state.  Thus, kernel functions should not allocate large
         structures or arrays as non-static local variables.  Use
         dynamic allocation with malloc() or palloc_get_page()
         instead.

   The first symptom of either of these problems will probably be
   an assertion failure in thread_current(), which checks that
   the `magic' member of the running thread's `struct thread' is
   set to THREAD_MAGIC.  Stack overflow will normally change this
   value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
   the run queue (thread.c), or it can be an element in a
   semaphore wait list (synch.c).  It can be used these two ways
   only because they are mutually exclusive: only a thread in the
   ready state is on the run queue, whereas only a thread in the
   blocked state is on a semaphore wait list. */
struct thread
  {
    /* Owned by thread.c. */
    tid_t tid;                          /* Thread identifier. */
    enum thread_status status;          /* Thread state. */
    char name[16];                      /* Name (for debugging purposes). */
    uint8_t *stack;                     /* Saved stack pointer. */
    int priority;                       /* Priority. */
    struct list_elem allelem;           /* List element for all threads list. */

    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /* List element. */

    /* For Userporg, just need code file. */
    struct file *elffile;               /* Exec File */
    struct file_desc {
        struct file *file[128];         /* all the exec file */
        int fd;                         /* file descriptor */;
    }file_desc;

     /* For the process status */
    int ret_status;                     /* The return status code. */
    int load_status;                    /* The load status code. */
    bool awaited;                       /* waited status */
    struct list child_list;             /* List element for children processes list. */
    struct list_elem childelem;         /* List element for children processes list. */
    struct list fd_list;                /* List of opened file. */
    struct semaphore ltem,tsem;         /* The semaphore used to notify the parent process whether the child process is loaded successfully. */


    /* Deprecated */
    struct list_elem slpelem;           /* the element in sleep_list. */
    int64_t sleep_ticks;                /* the time to wait */
    struct lock *lock_waiting;          /* locks still waiting. */
    struct list locks;                  /* locks owned by the thread. */

    int locks_priority;                 /* the top priority in the thread. */
    int base_priority;                  /* the right now priority. */
    int nice;                           /* the parameter in the cpu equation. */
    int recent_cpu;                     /* the float emulated by integet. */

    /* For Virtual Memory */
    void *stack_pointer;                /* The variable to store the thread's esp. */
    struct hash *pages;                 /* The variable to store the page table. */
    struct list mapped_file;            /* The memory mapped file. */
    struct list fds;                    /* List of file descriptors. */
    struct list mappings;               /* Memory-mapped files. */
    int next_handle;                    /* For next handle to deal with the problem */
    struct wsem * wsem;                 /* This process's completion status, to a smaller granularity. */
    struct semaphore esem;              /* semaphore for child thread load. */
    struct semaphore cwem;              /* semaphore for child thread exit. */
    struct thread* parent; 

    /* The dir thread hold. */
    struct dir* curr_dir;

#ifdef USERPROG
    /* Owned by userprog/process.c. */
    uint32_t *pagedir;                  /* Page directory. */
#endif

    /* Owned by thread.c. */
    unsigned magic;                     /* Detects stack overflow. */
  };
  
  /* Tracks the completion of a process.
   Reference held by both the parent, in its `children' list,
   and by the child, in its `wait_status' pointer. */
struct wsem {
  struct list_elem wsem_elem;
  int ret_status;
  tid_t tid;
  struct lock lock;               /* Protects ref_cnt. */
  int ref_cnt;                    /* 2=child and parent both alive,
                                       1=either child or parent alive,
                                       0=child and parent both dead. */
  struct semaphore dead;          /* 1=child alive, 0=child dead. */

};

struct list sleep_list;

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

/* Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func (struct thread *t, void *aux);
void thread_foreach (thread_action_func *, void *);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

struct thread* find_thread_id(tid_t id);
void thread_sleep(int64_t ticks);
void thread_foreach_sleep (void);
bool thread_less_priority(const struct list_elem *compare1,const struct list_elem *compare2,void *aux UNUSED);

void thread_priority_donate_nest(struct thread *t);
void thread_priority(struct thread *t);
void lock_priority_update(struct lock *l);

void thread_increase_recent_cpu(void);
void thread_recalculate_load_avg(void);
void thread_recalculate_recent_cpu(struct thread *t,void *);
void thread_recalculate_priority(struct thread *t,void *);

void thread_set_tid(struct thread *t, tid_t tid);

/* For file lock and release. */
void acquire_file_lock(void);
void release_file_lock(void);
void thread_wait(struct thread *t,int child_tid);

#endif /* threads/thread.h */

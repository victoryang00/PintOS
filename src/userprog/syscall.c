#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/init.h"
#include "userprog/process.h"
#include <list.h>
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "devices/input.h"
#include "threads/synch.h"


static void syscall_handler (struct intr_frame *);//系统调用handler



typedef int pid_t;

static int sys_write (int fd, const void *buffer, unsigned length);	//写操作
static int sys_halt (void);											//关闭系统
static int sys_create (const char *file, unsigned initial_size);	//创建文件
static int sys_open (const char *file);								//打开文件
static int sys_close (int fd);										//关闭文件
static int sys_read (int fd, void *buffer, unsigned size);			//读操作
static int sys_exec (const char *cmd);								//运行一个可执行文件
static int sys_wait (pid_t pid);									//等待进程pid死亡并且通过exit返回状态
static int sys_filesize (int fd);									//获取文件大小
static int sys_tell (int fd);										//返回Open 语句打开的文件fd中指定下一个读/写位置，表示为从文件开始的byte数。
static int sys_seek (int fd, unsigned pos);							//在 Open 语句打开的文件中指定当前的读/写位置，表示为从文件开始的byte数
static int sys_remove (const char *file);							//删除文件

static struct file *find_file_by_fd (int fd);						//自己添加的函数，方便程序根据fd找到文件指针
static struct fd_elem *find_fd_elem_by_fd (int fd);
static int alloc_fid (void);
static struct fd_elem *find_fd_elem_by_fd_in_process (int fd);

typedef int (*handler) (uint32_t, uint32_t, uint32_t);
static handler syscall_vec[128];
static struct lock file_lock;

struct fd_elem
  {
    int fd;															//文件的id
    struct file *file;												//文件指针
    struct list_elem elem;											//系统打开文件列表
    struct list_elem thread_elem;									//用户打开文件列表
  };
  
static struct list file_list;



void
syscall_init (void) 
{
  //初始化30号中断（即系统调用），使其指向syscall_handler函数
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  
  //系统调用的对应的系统调用号

  //初始化系统调用号对应的操作函数的函数指针
  syscall_vec[SYS_EXIT] = (handler)sys_exit;
  syscall_vec[SYS_HALT] = (handler)sys_halt;
  syscall_vec[SYS_CREATE] = (handler)sys_create;
  syscall_vec[SYS_OPEN] = (handler)sys_open;
  syscall_vec[SYS_CLOSE] = (handler)sys_close;
  syscall_vec[SYS_READ] = (handler)sys_read;
  syscall_vec[SYS_WRITE] = (handler)sys_write;
  syscall_vec[SYS_EXEC] = (handler)sys_exec;
  syscall_vec[SYS_WAIT] = (handler)sys_wait;
  syscall_vec[SYS_FILESIZE] = (handler)sys_filesize;
  syscall_vec[SYS_SEEK] = (handler)sys_seek;
  syscall_vec[SYS_TELL] = (handler)sys_tell;
  syscall_vec[SYS_REMOVE] = (handler)sys_remove;
  
  //初始化锁和系统打开文件列表
  list_init (&file_list);
  lock_init (&file_lock);
  
}

static void
syscall_handler (struct intr_frame *f /* intr_frame是指向用户程序的寄存器(esp),这里的寄存器包括参数传递所压栈的数据、系统调用号等 */) 
{
  /* 旧的实现，直接输出一句信息
  printf ("system call!\n");
  thread_exit (); */
 
  handler h;
  int *p;
  int ret;
  
  p = f->esp;
  
  if (!is_user_vaddr (p))
    goto terminate;
  
  if (*p < SYS_HALT || *p > SYS_INUMBER)//异常处理
    goto terminate;
  
  h = syscall_vec[*p];//获得向量中的系统调用号
  
  if (!(is_user_vaddr (p + 1) && is_user_vaddr (p + 2) && is_user_vaddr (p + 3)))//判断是否需要继续执行
    goto terminate;
  
  ret = h (*(p + 1), *(p + 2), *(p + 3));//处理对应的系统调用
  
  f->eax = ret;
  
  return;
  
terminate:
  sys_exit (-1);
  
}



/*	写操作
1、	在写的时候，我们需要给文件加锁，防止在读的过程中被改动
2、	先判断是否是标准写入流，如果是标准写入的话，直接调用putbuf()写到控制台，如果是标准写入流，则调用sys_exit(-1)，如果不是标准读或者标准写，则说明是从文件写入。判断指向buffer指针是否正确（是否有效且在用户空间），如果正确，则根据fd找到文件，然后调用file_write(f, buffer, size)函数写入buffer到文件，反之则调用sys_exit(-1)退出。
3、	注意在退出之前或者写文件完成之后要释放锁
4、具体代码实现如下：
*/
static int
sys_write (int fd, const void *buffer, unsigned length)
{
  struct file * f;
  int ret;
  
  ret = -1;
  lock_acquire (&file_lock);
  if (fd == STDOUT_FILENO) /* stdout */
    putbuf (buffer, length);
  else if (fd == STDIN_FILENO) /* stdin */
    goto done;
  else if (!is_user_vaddr (buffer) || !is_user_vaddr (buffer + length))//异常处理，防止用户进程访问内核空间
    {
      lock_release (&file_lock);
      sys_exit (-1);
    }
  else
    {
      f = find_file_by_fd (fd);
      if (!f)
        goto done;
        
      ret = file_write (f, buffer, length);
    }
    
done:
  lock_release (&file_lock);
  return ret;
}

/*	进程终止
1、	得到当前用户线程的指针
2、	讲该用户线程的对应的文件打开列表清空，并关闭对应的文件
3、	调用thread_exit()函数，并返回-1表示结束进程
4、	在thread.c里面我们添加了process_exit()函数，并移除所有子线程并关闭文件
*/
int
sys_exit (int status)
{
  
  struct thread *t;
  struct list_elem *l;
  
  t = thread_current ();
  while (!list_empty (&t->files))//关闭全部文件
    {
      l = list_begin (&t->files);
      sys_close (list_entry (l, struct fd_elem, thread_elem)->fd);
    }
  
  t->ret_status = status;
  thread_exit ();//结束进程
  return -1;
}

/*	关闭系统
调用init.c下面的power_off()函数
*/
static int
sys_halt (void)
{
  shutdown_power_off ();
}

/*	创建文件（sys_create (const char *file, unsigned initial_size)）
1、	获得需要创建的文件的文件名称
2、	如果文件名为空，则返回-1退出，如果存在，则调用filesys.c下面的filesys_create()函数
3、	具体代码实现如下：
*/
static int
sys_create (const char *file, unsigned initial_size)
{
  if (!file)
    return sys_exit (-1);
  return filesys_create (file, initial_size);
}

/*	打开文件
1、	定义返回值为打开文件的fd，如果打开失败，则返回-1
2、	判断传进来的文件名，如果为空或者它的地址不在用户空间，返回-1
3、	调用filesys_open(file)函数，如果打开失败（原因是文件名对应的文件不存在），则返回-1
4、	分配空间个fd对应的struct fde，如果内存空间不够，则调用file_close(f)关闭文件，返回-1
5、	初始化fde，并将其压入系统打开文件列表和进程打开文件列表相对应的栈中，并返回对应的fd号
6、	具体代码实现如下：
*/
static int
sys_open (const char *file)
{
  struct file *f;
  struct fd_elem *fde;
  int ret;
  
  ret = -1; //初始化为-1
  if (!file) //异常处理，不存在文件
    return -1;
  if (!is_user_vaddr (file))//确认在用户程序空间中
    sys_exit (-1);
  f = filesys_open (file);
  if (!f) //文件名字不符合文件系统的规则
    goto done;
    
  fde = (struct fd_elem *)malloc (sizeof (struct fd_elem));
  if (!fde) //内存不足
    {
      file_close (f);
      goto done;
    }
    
  fde->file = f;
  fde->fd = alloc_fid ();
  list_push_back (&file_list, &fde->elem);
  list_push_back (&thread_current ()->files, &fde->thread_elem);
  ret = fde->fd;
done:
  return ret;
}

/*	关闭文件 
1、	根据fd找到系统中对应的打开文件
2、	判断文件是否存在，如果不存在，则不需要关闭，返回0，如果存在，则调用file_close(f)将其关闭，并讲对应的fd从系统打开文件列表和进程打开文件列表中删除
*/
static int
sys_close(int fd)
{
  struct fd_elem *f;
  int ret;
  
  f = find_fd_elem_by_fd_in_process (fd);
  
  if (!f) //错误的fd
    goto done;
  file_close (f->file);
  list_remove (&f->elem);
  list_remove (&f->thread_elem);
  free (f);
  
done:
  return 0;
}

/*	读操作
1、	在读的时候，我们需要给文件加锁，防止在读的过程中被改动
2、	先判断是否是标准读入流，如果是标准读入的话，直接调用input_getc()从控制台读入，如果是标准写入流，则调用sys_exit(-1)，如果不是标准读或者标准写，则说明是从文件读入。判断指向buffer指针是否正确（是否有效且在用户空间），如果正确，则根据fd找到文件，然后调用file_read(f, buffer, size)函数读取到buffer，反之则调用sys_exit(-1)退出。
3、	注意在退出之前或者读文件完成之后要释放锁
4、具体代码实现如下：
*/
static int
sys_read (int fd, void *buffer, unsigned size)
{
  struct file * f;
  unsigned i;
  int ret;
  
  ret = -1; 
  lock_acquire (&file_lock);
  if (fd == STDIN_FILENO) /* stdin */
    {
      for (i = 0; i != size; ++i)
        *(uint8_t *)(buffer + i) = input_getc ();
      ret = size;
      goto done;
    }
  else if (fd == STDOUT_FILENO) /* stdout */
      goto done;
  else if (!is_user_vaddr (buffer) || !is_user_vaddr (buffer + size)) //异常处理
    {
      lock_release (&file_lock);
      sys_exit (-1);
    }
  else
    {
      f = find_file_by_fd (fd);
      if (!f)
        goto done;
      ret = file_read (f, buffer, size);
    }
    
done:    
  lock_release (&file_lock);
  return ret;
}

/*	进程执行
调用process.c下面的process_execute函数
*/
static int
sys_exec (const char *cmd)
{
  int ret;
  
  if (!cmd || !is_user_vaddr (cmd)) //异常处理
    return -1;
  lock_acquire (&file_lock);
  ret = process_execute (cmd);
  lock_release (&file_lock);
  return ret;
}

/*	进程等待
调用process.c下面的start_process函数
*/
static int
sys_wait (pid_t pid)
{
  return process_wait (pid);
}

static struct file *
find_file_by_fd (int fd)
{
  struct fd_elem *ret;
  
  ret = find_fd_elem_by_fd (fd);
  if (!ret)
    return NULL;
  return ret->file;
}

static struct fd_elem *
find_fd_elem_by_fd (int fd)
{
  struct fd_elem *ret;
  struct list_elem *l;
  
  for (l = list_begin (&file_list); l != list_end (&file_list); l = list_next (l))
    {
      ret = list_entry (l, struct fd_elem, elem);
      if (ret->fd == fd)
        return ret;
    }
    
  return NULL;
}

static int
alloc_fid (void)
{
  static int fid = 2;
  return fid++;
}

/*	取文件大小
调用filesys.c下面的file_length函数
*/
static int
sys_filesize (int fd)
{
  struct file *f;
  
  f = find_file_by_fd (fd);
  if (!f)
    return -1;
  return file_length (f);
}

/*	取当前光标位置
调用filesys.c下面的file_tell函数
*/
static int
sys_tell (int fd)
{
  struct file *f;
  
  f = find_file_by_fd (fd);
  if (!f)
    return -1;
  return file_tell (f);
}

/*	改变当前光标所在位置
调用filesys.c下面的file_seek函数
*/
static int
sys_seek (int fd, unsigned pos)
{
  struct file *f;
  
  f = find_file_by_fd (fd);
  if (!f)
    return -1;
  file_seek (f, pos);
  return 0; 
}

/*	删除文件
调用filesys.c下面的filesys_remove函数
*/
static int
sys_remove (const char *file)
{
  if (!file)
    return false;
  if (!is_user_vaddr (file))
    sys_exit (-1);
    
  return filesys_remove (file);
}

static struct fd_elem *
find_fd_elem_by_fd_in_process (int fd)
{
  struct fd_elem *ret;
  struct list_elem *l;
  struct thread *t;
  
  t = thread_current ();
  
  // for (l = list_begin (&t->files); l != list_end (&t->files); l = list_next (l))
  //   {
  //     ret = list_entry (l, struct fd_elem, thread_elem);
  //     if (ret->fd == fd)
  //       return ret;
  //   }
    
  return NULL;
}

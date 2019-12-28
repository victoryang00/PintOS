#ifndef FILESYS_FILE_H
#define FILESYS_FILE_H

#include "filesys/off_t.h"
#include "devices/block.h"
#include "filesys/inode.h"
#include "threads/malloc.h"

struct inode;

struct file {
    off_t pos;                  /* Current position. */
    bool deny_write;            /* Has file_deny_write() been called? */
    struct inode *inode;        /* File's inode. */
};

/* Opening and closing files. */
struct file *file_open (struct inode *);
struct file *file_reopen (struct file *);
void file_close (struct file *);
struct inode *file_get_inode (struct file *);

/* Reading and writing. */
off_t file_read (struct file *, void *, off_t);
off_t file_read_at (struct file *, void *, off_t size, off_t start);
off_t file_write (struct file *, const void *, off_t);
off_t file_write_at (struct file *, const void *, off_t size, off_t start);

/* Preventing writes. */
void file_deny_write (struct file *);
void file_allow_write (struct file *);

/* File position. */
void file_seek (struct file *, off_t);
off_t file_tell (struct file *);
off_t file_length (struct file *);

/* Create a file, pass by syscall */
struct inode *file_create (block_sector_t sector, off_t length);

/* get file type*/
bool is_really_file(struct file* file);
bool read_dir_by_file_node(struct file* file, char* name, int order);
int get_inumber(struct file* file);

/* An open file. */

#endif /* filesys/file.h */

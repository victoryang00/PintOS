#ifndef FILESYS_INODE_H
#define FILESYS_INODE_H

#include <stdbool.h>
#include "filesys/off_t.h"
#include "devices/block.h"
#include <list.h>
#include "threads/synch.h"
struct bitmap;

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    // block_sector_t start;               /* First data sector. */
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
     
    block_sector_t pointers[(DIRECT_POINTER_NUM + SINGLE_POINTER_NUM + DOUBLE_POINTER_NUM)];
    uint32_t level0_ptr_index;                  /* index of the pointer list */
    uint32_t level1_ptr_index;               /* index of the level 1 pointer table */
    uint32_t level2_ptr_index;               /* index of the level 2 pointer table */

    uint32_t is_file;                    /* 1 for file, 0 for dir */
    uint32_t not_used[122 - (DIRECT_POINTER_NUM + SINGLE_POINTER_NUM + DOUBLE_POINTER_NUM)];
  };


/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int count;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */

    struct lock extend_lock;
    
    off_t length;                       /* File size in bytes. */
    off_t length_for_read;              /* Calculate the File size in bytes. */
  };

void inode_init (void);
bool inod_create (block_sector_t, off_t);

struct inode *inode_open (block_sector_t);
struct inode *inode_reopen (struct inode *);
block_sector_t inode_get_inumber (const struct inode *);
void inode_close (struct inode *);
void inode_remove (struct inode *);
off_t inode_read_at (struct inode *, void *, off_t size, off_t offset);
off_t inode_write_at (struct inode *, const void *, off_t size, off_t offset);
void inode_deny_write (struct inode *);
void inode_allow_write (struct inode *);
off_t inode_length (const struct inode *);

#endif /* filesys/inode.h */

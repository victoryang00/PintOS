#include "filesys/inode.h"

#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "cache.h"


/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44



/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_data_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}


/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);

    if (pos < inode->data.length) {
        if (pos < 100 * BLOCK_SECTOR_SIZE) {
            return inode->data.pointers[pos / BLOCK_SECTOR_SIZE];
        } else if (pos < (BLOCK_SECTOR_SIZE/4 + 100) * BLOCK_SECTOR_SIZE) {
            uint32_t L1index;
            uint32_t L1table[BLOCK_SECTOR_SIZE/4];
            pos -= 100 * BLOCK_SECTOR_SIZE;
            L1index = pos / BLOCK_SECTOR_SIZE;
            block_read(fs_device, inode->data.pointers[100], &L1table);
            return L1table[L1index];
        } else {
            uint32_t level_index;
            uint32_t level_table[BLOCK_SECTOR_SIZE/4];

            /* read the first level pointer table. */
            block_read(fs_device, inode->data.pointers[101], &level_table);
            pos -= (100 + BLOCK_SECTOR_SIZE/4) * BLOCK_SECTOR_SIZE;
            level_index = pos / (BLOCK_SECTOR_SIZE/4 * BLOCK_SECTOR_SIZE);

            /* read the second level pointer table. */
            block_read(fs_device, level_table[level_index], &level_table);
            pos -= level_index * (BLOCK_SECTOR_SIZE/4 * BLOCK_SECTOR_SIZE);
            return level_table[pos / BLOCK_SECTOR_SIZE];
        }
    } else 
        return -1;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list inode_opened;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&inode_opened);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length, uint32_t is_file)
{
  struct inode_disk *disk_inode = NULL;
  /* init the inode_disk as NULL */
  bool success = false;

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  /* calloc is the best memory func for filesys, returns a*b */
  disk_inode = calloc (1, sizeof *disk_inode);

  if (disk_inode)
  {
    /* dealt with the disk inode with thread operation. */
    disk_inode->length = length;
    disk_inode->magic = INODE_MAGIC;
    disk_inode-> is_file = is_file;

     if (inode_allocate(disk_inode)) {
        block_write(fs_device, sector, disk_inode);
        success = true;
      }
    free (disk_inode);
  }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&inode_opened); e != list_end (&inode_opened);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&inode_opened, &inode->elem);
  inode->sector = sector;
  inode->count = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  block_read (fs_device, inode->sector, &inode->data);
  
  /* Synchronize with other thread. */
  lock_init(&inode->extend_lock);
  block_read(fs_device, inode->sector, &inode->data);
  inode->length = inode->data.length;
  inode->length_for_read = inode->data.length;

  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->count++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->count == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          free_map_release (inode->sector, 1);
          inode_deallocate (inode);
        }

      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  off_t read_length = inode->length_for_read;

  /* do not allow to read beyound the read length
     because someone may be write beyound current length */
  if(offset >= read_length) {
    return bytes_read;
  }

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = read_length - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      /* write to cache */
      struct cache_entry *c = cache_get_block(sector_idx, false);
      memcpy(buffer + bytes_read, (uint8_t *)&c->block + sector_ofs, chunk_size);
      c->reference_bit= 1;
      c->count--;
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;

  if (inode->deny_write_cnt)
    return 0;

   /* Extend the file. */
  if (offset + size > inode_length(inode))
  {
    if (inode->data.is_file)
    {
      lock_acquire(&inode->extend_lock);
    }
    inode->length = inode_extend(inode, offset + size);
    inode->data.length = inode->length;

    /* Write the extended information to the disk */
    block_write(fs_device, inode->sector, &inode->data);

    if (inode->data.is_file)
    {
      lock_release(&inode->extend_lock);
    }
  }

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      /* write to cache */
      struct cache_entry *cache = cache_get_block(sector_idx, true);
      memcpy((uint8_t *)&cache->block + sector_ofs, buffer + bytes_written, chunk_size);
      cache->reference_bit= true;
      cache->dirty = true;
      cache->count-=1;

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }

  inode->length_for_read = inode->length;

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->count);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}


/** extend the file size to NEW_LENGTH (in bytes) 
*   return NEW_LENGTH if allocated success
*/
off_t inode_extend(struct inode *inode, off_t new_length)
{
  static char blank[BLOCK_SECTOR_SIZE];
  size_t sector_desired = DIV_ROUND_UP (new_length, BLOCK_SECTOR_SIZE) -
                            DIV_ROUND_UP (inode->length, BLOCK_SECTOR_SIZE);

  if (sector_desired == 0)
  
    return new_length;
  

  /* allocate for the sector that direct pointer points to */
  while (inode->data.ptr0 < 100)
  {
    free_map_allocate(1, &inode->data.pointers[inode->data.ptr0]);
    block_write(fs_device, inode->data.pointers[inode->data.ptr0], blank);
    inode->data.ptr0 ++;
    sector_desired--;
    if (sector_desired == 0)
    
      return new_length;
    
  }
  int inode_state = (inode->data.ptr0 == 100) * 2 + (inode->data.ptr0 == 101);
  switch (inode_state){
    case 2:
      sector_desired = inode_sb(inode,sector_desired);
      if(sector_desired ==0)
        return new_length;
      break;
    case 1:
      sector_desired = inode_db(inode,sector_desired);
    case 3:
    case 0:
      return new_length-sector_desired*BLOCK_SECTOR_SIZE;
  }

}

size_t inode_sb(struct inode *inode, size_t sector_desired)
{
  static char blank[BLOCK_SECTOR_SIZE];
  block_sector_t ptr_block[128];
  if (inode->data.ptr1 == 0)
  {
    free_map_allocate(1, &inode->data.pointers[inode->data.ptr0]);
  }
  else
  {
    block_read(fs_device, inode->data.pointers[inode->data.ptr0], &ptr_block);
  }
  while (inode->data.ptr1 < 128)
  {
    free_map_allocate(1, &ptr_block[inode->data.ptr1]);
    block_write(fs_device, ptr_block[inode->data.ptr1], blank);
    inode->data.ptr1 ++;
    sector_desired--;
    if (sector_desired == 0)
    {
      break;
    }
  }
  block_write(fs_device, inode->data.pointers[inode->data.ptr0], &ptr_block);
  if (inode->data.ptr1 == 128)
  {
    inode->data.ptr1 = 0;
    inode->data.ptr0 ++;
  }
  return sector_desired;
}

size_t inode_db(struct inode *inode,
                                          size_t sector_desired)
{
  block_sector_t ptr_block[BLOCK_SECTOR_SIZE/4];
  int inode_state =inode->data.ptr2 == 0 && inode->data.ptr1 == 0;
  switch (inode_state){
    case 1:
        free_map_allocate(1, &inode->data.pointers[inode->data.ptr0]);
        break;
    case 0:
        block_read(fs_device, inode->data.pointers[inode->data.ptr0], &ptr_block);
    }

    while (inode->data.ptr1 < BLOCK_SECTOR_SIZE / 4 || sector_desired != 0) {
        // sector_desired = inode_db2(inode, sector_desired, &ptr_block);
        static char blank[BLOCK_SECTOR_SIZE];
        block_sector_t L2_block[128];
        if (inode->data.ptr2 == 0)

            free_map_allocate(1, &ptr_block[inode->data.ptr1]);

        else

            block_read(fs_device, ptr_block[inode->data.ptr1], &L2_block);

        while (inode->data.ptr2 < 128 || sector_desired != 0) {
            free_map_allocate(1, &L2_block[inode->data.ptr2]);
            block_write(fs_device, L2_block[inode->data.ptr2], blank);
            inode->data.ptr2++;
            sector_desired--;
        }
        block_write(fs_device, ptr_block[inode->data.ptr1], &L2_block);
        if (inode->data.ptr2 == 128) {
            inode->data.ptr2 = 0;
            inode->data.ptr1++;
        }
    }

  block_write(fs_device, inode->data.pointers[inode->data.ptr0], &ptr_block);
  return sector_desired;
}

bool inode_allocate(struct inode_disk *disk_inode)
{
  struct inode inode = {
      .length = 0
  };
  inode_extend(&inode, disk_inode->length);

  /* copy the disk inode */
  for (int i = 0; i < 102; i++) {
    disk_inode->pointers[i] = inode.data.pointers[i];
  }
  disk_inode->ptr0 = inode.data.ptr0;
  disk_inode->ptr1 = inode.data.ptr1;
  disk_inode->ptr2 = inode.data.ptr2;
  return true;
}


void inode_deallocate(struct inode *inode)
{
  size_t sector0 = DIV_ROUND_UP (inode->length, BLOCK_SECTOR_SIZE);
  size_t L2_sectors = ((inode->length) <= BLOCK_SECTOR_SIZE * (100 + BLOCK_SECTOR_SIZE/4)) ? 0 : 1;
  size_t L1sectors;
  unsigned int ptr0 = 0;
  size_t single_data_ptrs = sector0;
  size_t data_per_block = 128;
  unsigned int i;
  block_sector_t ptr_block[128];
  if (inode->length <= BLOCK_SECTOR_SIZE * 100)

      {L1sectors = 0;
      goto SB2;}
  inode->length -= BLOCK_SECTOR_SIZE * 100;
  L1sectors = DIV_ROUND_UP(inode->length, BLOCK_SECTOR_SIZE * 128);
SB2:
  while (sector0 && ptr0 < 100) {
      free_map_release(inode->data.pointers[ptr0], 1);
      sector0--;
      ptr0++;
  }
  if (L1sectors)
  {
    if (single_data_ptrs > 128) {
      single_data_ptrs = 128;
    }
    block_read(fs_device, inode->data.pointers[ptr0], &ptr_block);
    for (i = 0; i < single_data_ptrs; i++) {
        free_map_release(ptr_block[i], 1);
    }
    free_map_release(inode->data.pointers[ptr0], 1);
    sector0 -= single_data_ptrs;
    L1sectors --;
    ptr0++;
  }
  if (L2_sectors)
  {
    block_read(fs_device, inode->data.pointers[ptr0], &ptr_block);
    for (i = 0; i < L1sectors; i++) {
        if (data_per_block > sector0) {
            data_per_block = sector0;
        }
        block_read(fs_device, ptr_block[i], &ptr_block);
        for ( i = 0; i < data_per_block; i++) {
            free_map_release(ptr_block[i], 1);
        }
        free_map_release(ptr_block[i], 1);
        sector0 -= data_per_block;
    }
    free_map_release(inode->data.pointers[ptr0], 1);
  }
}


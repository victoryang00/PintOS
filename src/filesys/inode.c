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

size_t inode_expand_single_block(struct inode *inode, size_t needed_allocated_sectors);
size_t inode_expand_double_block(struct inode *inode, size_t needed_allocated_sectors);
size_t inode_expand_double_block2(struct inode *inode, size_t needed_allocated_sectors, block_sector_t *level1_block);
bool allocate_inode(struct inode_disk *disk_inode);


void deallocate_inode(struct inode *inode);
void inode_dealloc_double_indirect_block(block_sector_t *ptr, size_t level1_sectors, size_t level0_sectors);
void inode_dealloc_indirect_block(block_sector_t *ptr, size_t data_ptrs);


/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_data_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}


/* Indirect sectors implementation */
static size_t
bytes_to_indirect_sectors(off_t size)
{
  if (size <= BLOCK_SECTOR_SIZE * DIRECT_POINTER_NUM)
  {
    return 0;
  }
  size -= BLOCK_SECTOR_SIZE * DIRECT_POINTER_NUM;
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE * PTRS_PER_SECTOR);
}

static size_t bytes_to_double_indirect_sector(off_t size)
{
  if (size <= BLOCK_SECTOR_SIZE * (DIRECT_POINTER_NUM + PTRS_PER_SECTOR))
  {
    return 0;
  }
  return 1;
}

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);

  if(pos < inode->data.length) {
    if(pos < DIRECT_POINTER_NUM * BLOCK_SECTOR_SIZE) 
    {
      return inode->data.pointers[pos / BLOCK_SECTOR_SIZE];
    } 
    else if (pos < (PTRS_PER_SECTOR + DIRECT_POINTER_NUM) * BLOCK_SECTOR_SIZE) 
    {
      uint32_t level1_index;
      uint32_t level1_table[PTRS_PER_SECTOR];
      pos -= DIRECT_POINTER_NUM * BLOCK_SECTOR_SIZE;
      level1_index = pos / BLOCK_SECTOR_SIZE;
      block_read(fs_device, inode->data.pointers[TOTAL_POINTER_NUM - 2],
                 &level1_table);
      return level1_table[level1_index];
    }
    else 
    {
      uint32_t level_index;
      uint32_t level_table[PTRS_PER_SECTOR];


      // read the first level pointer table
      block_read(fs_device, inode->data.pointers[TOTAL_POINTER_NUM - 1],
                 &level_table);
      pos -= (DIRECT_POINTER_NUM + PTRS_PER_SECTOR) * BLOCK_SECTOR_SIZE;
      level_index = pos / (PTRS_PER_SECTOR * BLOCK_SECTOR_SIZE);


      // read the second level pointer table
      block_read(fs_device, level_table[level_index], &level_table);
      pos -= level_index * (PTRS_PER_SECTOR * BLOCK_SECTOR_SIZE);
      return level_table[pos / BLOCK_SECTOR_SIZE];
    }
  }
  else 
  {
    return -1;
  }
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}



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

  if (disk_inode != NULL)
  {
    /* dealt with the disk inode with thread operation. */
    disk_inode->length = length;
    disk_inode->magic = INODE_MAGIC;
    disk_inode-> is_file = is_file;

     if (allocate_inode(disk_inode)) {
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
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
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
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  block_read (fs_device, inode->sector, &inode->data);
  
   /* for synchronization. */
  lock_init(&inode->extend_lock);
  block_read (fs_device, inode->sector, &inode->data);
  inode->length = inode->data.length;
  inode->length_for_read = inode->data.length;

  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
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
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          free_map_release (inode->sector, 1);
          deallocate_inode (inode);

          // free_map_release (inode->data.start,
          //                   bytes_to_data_sectors (inode->data.length)); 
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
      memcpy (buffer + bytes_read, (uint8_t *) &c->block + sector_ofs,
	      chunk_size);
      c->referebce_bit= true;
      c->open_cnt--;
      
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

   // extend the file
  if (offset + size > inode_length(inode))
  {
    if (inode->data.is_file)
    {
      lock_acquire(&inode->extend_lock);
    }
    inode->length = inode_extend(inode, offset + size);
    inode->data.length = inode->length;

    // write the extended information to the disk
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
      memcpy ((uint8_t *) &cache->block + sector_ofs, buffer + bytes_written,
	      chunk_size);
      cache->referebce_bit= true;
      cache->dirty = true;
      cache->open_cnt-=1;

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
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
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
  static char zeros[BLOCK_SECTOR_SIZE];
  size_t needed_allocated_sectors = bytes_to_data_sectors(new_length) -
                            bytes_to_data_sectors(inode->length);

  if (needed_allocated_sectors == 0)
  {
    return new_length;
  }

  /* allocate for the sector that direct pointer points to */
  while (inode->data.level0_ptr_index < DIRECT_POINTER_NUM)
  {
    free_map_allocate(1, &inode->data.pointers[inode->data.level0_ptr_index]);
    block_write(fs_device, inode->data.pointers[inode->data.level0_ptr_index], zeros);
    inode->data.level0_ptr_index ++;
    needed_allocated_sectors--;
    if (needed_allocated_sectors == 0)
    {
      return new_length;
    }
  }
  /* allocate for the sector of single indirect pointers */
  if (inode->data.level0_ptr_index == DIRECT_POINTER_NUM)
  {
    needed_allocated_sectors = inode_expand_single_block(inode, needed_allocated_sectors);
    if (needed_allocated_sectors == 0)
    {
      return new_length;
    }
  }
  if (inode->data.level0_ptr_index == DIRECT_POINTER_NUM + SINGLE_POINTER_NUM)
  {
    needed_allocated_sectors = inode_expand_double_block(inode, needed_allocated_sectors);
  }
  return new_length - needed_allocated_sectors * BLOCK_SECTOR_SIZE;
}

size_t inode_expand_single_block(struct inode *inode, size_t needed_allocated_sectors)
{
  static char zeros[BLOCK_SECTOR_SIZE];
  block_sector_t ptr_block[PTRS_PER_SECTOR];
  if (inode->data.level1_ptr_index == 0)
  {
    free_map_allocate(1, &inode->data.pointers[inode->data.level0_ptr_index]);
  }
  else
  {
    block_read(fs_device, inode->data.pointers[inode->data.level0_ptr_index], &ptr_block);
  }
  while (inode->data.level1_ptr_index < PTRS_PER_SECTOR)
  {
    free_map_allocate(1, &ptr_block[inode->data.level1_ptr_index]);
    block_write(fs_device, ptr_block[inode->data.level1_ptr_index], zeros);
    inode->data.level1_ptr_index ++;
    needed_allocated_sectors--;
    if (needed_allocated_sectors == 0)
    {
      break;
    }
  }
  block_write(fs_device, inode->data.pointers[inode->data.level0_ptr_index], &ptr_block);
  if (inode->data.level1_ptr_index == PTRS_PER_SECTOR)
  {
    inode->data.level1_ptr_index = 0;
    inode->data.level0_ptr_index ++;
  }
  return needed_allocated_sectors;
}

size_t inode_expand_double_block(struct inode *inode,
                                          size_t needed_allocated_sectors)
{
  block_sector_t ptr_block[PTRS_PER_SECTOR];
  if (inode->data.level2_ptr_index == 0 && inode->data.level1_ptr_index == 0)
  {
    free_map_allocate(1, &inode->data.pointers[inode->data.level0_ptr_index]);
  }
  else
  {
    block_read(fs_device, inode->data.pointers[inode->data.level0_ptr_index], &ptr_block);
  }

  while (inode->data.level1_ptr_index < PTRS_PER_SECTOR)
  {
    needed_allocated_sectors = inode_expand_double_block2(inode, needed_allocated_sectors, &ptr_block);
    if (needed_allocated_sectors == 0)
    {
      break;
    }
  }
  block_write(fs_device, inode->data.pointers[inode->data.level0_ptr_index], &ptr_block);
  return needed_allocated_sectors;
}

size_t inode_expand_double_block2(struct inode *inode,
                                                  size_t needed_allocated_sectors,
                                                  block_sector_t *level1_block)
{
  
  static char zeros[BLOCK_SECTOR_SIZE];
  block_sector_t level2_block[PTRS_PER_SECTOR];
  if (inode->data.level2_ptr_index == 0)
  {
    free_map_allocate(1, &level1_block[inode->data.level1_ptr_index]);
  }
  else
  {
    block_read(fs_device, level1_block[inode->data.level1_ptr_index],
               &level2_block);
  }
  while (inode->data.level2_ptr_index < PTRS_PER_SECTOR)
  {
    free_map_allocate(1, &level2_block[inode->data.level2_ptr_index]);
    block_write(fs_device, level2_block[inode->data.level2_ptr_index],
                zeros);
    inode->data.level2_ptr_index ++;
    needed_allocated_sectors--;
    if (needed_allocated_sectors == 0)
    {
      break;
    }
  }
  block_write(fs_device, level1_block[inode->data.level1_ptr_index], &level2_block);
  if (inode->data.level2_ptr_index == PTRS_PER_SECTOR)
  {
    inode->data.level2_ptr_index = 0;
    inode->data.level1_ptr_index ++;
  }
  return needed_allocated_sectors;
}

bool allocate_inode(struct inode_disk *disk_inode)
{
  struct inode inode = {
      .length = 0
  };
  // memcpy();
  inode_extend(&inode, disk_inode->length);

  // copy the disk inode
  for (int i = 0; i < TOTAL_POINTER_NUM; i++) {
    disk_inode->pointers[i] = inode.data.pointers[i];
  }
  disk_inode->level0_ptr_index = inode.data.level0_ptr_index;
  disk_inode->level1_ptr_index = inode.data.level1_ptr_index;
  disk_inode->level2_ptr_index = inode.data.level2_ptr_index;
  return true;
}


void deallocate_inode(struct inode *inode)
{
  size_t level0_sectors = bytes_to_data_sectors(inode->length);
  size_t level1_sectors = bytes_to_indirect_sectors(inode->length);
  size_t level2_sectors = bytes_to_double_indirect_sector(inode->length);
  unsigned int level0_ptr_index = 0;
  while (level0_sectors && level0_ptr_index < DIRECT_POINTER_NUM)
  {
    free_map_release(inode->data.pointers[level0_ptr_index], 1);
    level0_sectors--;
    level0_ptr_index++;
  }
  if (level1_sectors)
  {
    size_t single_data_ptrs = level0_sectors;
    if (single_data_ptrs > PTRS_PER_SECTOR) {
      single_data_ptrs = PTRS_PER_SECTOR;
    }
    inode_dealloc_indirect_block(&inode->data.pointers[level0_ptr_index], single_data_ptrs);
    level0_sectors -= single_data_ptrs;
    level1_sectors --;
    level0_ptr_index++;
  }
  if (level2_sectors)
  {
    inode_dealloc_double_indirect_block(&inode->data.pointers[level0_ptr_index], 
                                 level1_sectors,level0_sectors);
  }
}

void inode_dealloc_double_indirect_block(block_sector_t *ptr,
                                         size_t level1_sectors,
                                         size_t level0_sectors)
{
  unsigned int i;
  block_sector_t ptr_block[PTRS_PER_SECTOR];
  block_read(fs_device, *ptr, &ptr_block);
  for (i = 0; i < level1_sectors; i++)
  {
    size_t data_per_block = PTRS_PER_SECTOR;
    if (data_per_block > level0_sectors) {
      data_per_block = level0_sectors;
    }
    inode_dealloc_indirect_block(&ptr_block[i], data_per_block);
    level0_sectors -= data_per_block;
  }
  free_map_release(*ptr, 1);
}

void inode_dealloc_indirect_block(block_sector_t *ptr, size_t data_ptrs)
{
  block_sector_t ptr_block[PTRS_PER_SECTOR];
  block_read(fs_device, *ptr, &ptr_block);
  for (unsigned int i = 0; i < data_ptrs; i++)
  {
    free_map_release(ptr_block[i], 1);
  }
  free_map_release(*ptr, 1);
}
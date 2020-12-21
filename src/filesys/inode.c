#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "filesys/cache.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

size_t inode_expand_single(struct inode *inode, size_t sector_desired);
size_t inode_expand_double(struct inode *inode, size_t sector_desired);
size_t inode_expand_double_second(struct inode *inode, size_t sector_desired, block_sector_t *level1_block);
bool allocate_inode(struct inode_disk *disk_inode);
void deallocate_inode(struct inode *inode);
void inode_dealloc_indirect_block(block_sector_t *ptr, size_t data_ptrs);

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_data_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* Indirect sectors implementation */
static size_t bytes_to_indirect_sectors(off_t size) {
    while (size <= BLOCK_SECTOR_SIZE * 100) {
        return 0;
    }
    size -= BLOCK_SECTOR_SIZE * 100;
    return DIV_ROUND_UP(size, BLOCK_SECTOR_SIZE * BLOCK_SECTOR_SIZE/4);
}

static size_t bytes_to_double_indirect_sector(off_t size) {
    return (size <= BLOCK_SECTOR_SIZE * (100 + BLOCK_SECTOR_SIZE/4)) ? 0 : 1;
}

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
    ASSERT(inode != NULL);

    if (pos < inode->data.length) {
        if (pos < 100 * BLOCK_SECTOR_SIZE) {
            return inode->data.pointers[pos / BLOCK_SECTOR_SIZE];
        } else if (pos < (BLOCK_SECTOR_SIZE/4 + 100) * BLOCK_SECTOR_SIZE) {
            uint32_t level1_index;
            uint32_t level1_table[BLOCK_SECTOR_SIZE/4];
            pos -= 100 * BLOCK_SECTOR_SIZE;
            level1_index = pos / BLOCK_SECTOR_SIZE;
            block_read(fs_device, inode->data.pointers[TOTAL_POINTER_NUM - 2], &level1_table);
            return level1_table[level1_index];
        } else {
            uint32_t level_index;
            uint32_t level_table[BLOCK_SECTOR_SIZE/4];

            /* read the first level pointer table. */
            block_read(fs_device, inode->data.pointers[TOTAL_POINTER_NUM - 1], &level_table);
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
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length, bool is_file)
{
  struct inode_disk *disk_inode = NULL;
  /* First init the inode dist as NULL */
  bool success = false;

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  /* calloc is the best memory func for filesys, returns a*b */
  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL) {
      /* dealt with the disk inode with thread operation. */
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      disk_inode->is_file = is_file;
      if (allocate_inode(disk_inode)) {
          block_write(fs_device, sector, disk_inode);
          success = true;
      }
      free(disk_inode);
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
  off_t read_length = inode->lengh_for_read;

  if(offset >=read_length)
    return bytes_read;
  

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
      /* To write back to the cache. */
      struct cache_entry *cache = cache_get_block(sector_idx, false);
      memcpy (buffer+bytes_read,(uint8_t *)&cache->block +sector_ofs,chunk_size);

      /* Advance. */
      cache->reference_bit = 1;
      cache->count++;
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

  if(offset + size > inode_length(inode)){

    /* Always check the inode is directory or file. */
    if (inode->data.is_file)
      lock_acquire(&inode->extend_lock);
    inode->length = inode_extend(inode,offset+size);
    inode->data.length = inode->length;

    /* Will write back the tedious information back to the disk. */
    block_write(fs_device,inode->sector, &inode->data);
    if(inode->data.is_file)
      lock_release(&inode->extend_lock);
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

      /* Write to the cache. */
      struct cache_entry *cache = cache_get_block(sector_idx,true);
      memcpy ((uint8_t *)&cache->block + sector_ofs, buffer + bytes_written,chunk_size);
      cache->reference_bit = 1;
      cache->dirty = 1;
      cache->count = 1;

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
    /* Set the left pointer to the end. */
    inode->length_for_read = inode->length;
    return bytes_written;
}

/* Extend the file size to the length desired. */
off_t inode_extend(struct inode *inode, off_t new_length){
  static char blank[BLOCK_SECTOR_SIZE];
  size_t sector_desired = bytes_to_data_sectors(new_length) - bytes_to_data_sectors(inode ->length);

  if (!sector_desired)
    return new_length;
  
  while(inode->data.level0_ptr_index< 100){
    free_map_allocate(1,&inode->data)
    block_write(fs_device,inode->data.pointers[inode->data.level0_ptr_index],blank);
    inode->data.level0_ptr_index ++;
    sector_desired--;
    if(sector_desired)
      return new_length;
  }
  int inode_state = (Pinode->data.level0_ptr_index == 100)*2 +(inode->data.level0_ptr_index == 101);
  switch (inode_state){
    case 2:
      sector_desired = inode_expand_single(inode,sector_desired);
      if(sector_desired ==0)
        return new_length;
      break;
    case 1:
      sector_desired = inode_expand_double(inode,sector_desired);
    case 3:
    case 0:
      return new_length-sector_desired*BLOCK_SECTOR_SIZE;
  }

}

size_t inode_expand_single(struct inode *inode, size_t sector_desired){
  static char blank[BLOCK_SECTOR_SIZE];
  block_sector_t ptr_block[BLOCK_SECTOR_SIZE/4];
  int inode_state = node->data.level1_ptr_index == 0;
  switch(inode_state){
    case 1:
      free_map_allocate(1, &inode->data.pointers[inode->data.level0_ptr_index]);
      break;
    case 2;
      block_read(fs_device, inode->data.pointers[inode->data.level0_ptr_index], &ptr_block); 
  }
  while (inode->data.level1_ptr_index<BLOCK_SECTOR_SIZE/4 || sector_desired !=0){
    free_map_allocate(1,&ptr_block[inode->data.level1_ptr_index]);
    block_write(fs_device,ptr_block[inode->data.level1_ptr_index],zeros);
    inode->data.level0_ptr_index ++;
    sector_desired--;
  }
  block_write(fs_device, inode->data.pointers[inode->data.level0_ptr_index], &ptr_block);
  if(inode->data.level1_ptr_index == BLOCK_SECTOR_SIZE/4){
    inode->data.level0_ptr_index++;
    inode->data.level1_ptr_index=0;
  }
  return sector_desired;
}

size_t inode_expand_double(struct inode *inode,
                                          size_t sector_desired)
{
  block_sector_t ptr_block[BLOCK_SECTOR_SIZE/4];
  int inode_state =inode->data.level2_ptr_index == 0 && inode->data.level1_ptr_index == 0;
  switch (inode_state){
    case 1:
        free_map_allocate(1, &inode->data.pointers[inode->data.level0_ptr_index]);
        break;
    case 0:
        block_read(fs_device, inode->data.pointers[inode->data.level0_ptr_index], &ptr_block);
    }

  while (inode->data.level1_ptr_index < BLOCK_SECTOR_SIZE / 4 || sector_desired != 0)
      sector_desired = inode_expand_double_block2(inode, sector_desired, &ptr_block);

  block_write(fs_device, inode->data.pointers[inode->data.level0_ptr_index], &ptr_block);
  return sector_desired;
}

size_t inode_expand_double_second(struct inode *inode,
                                                  size_t sector_desired,
                                                  block_sector_t *level1_block)
{
  
  static char zeros[BLOCK_SECTOR_SIZE];
  block_sector_t level2_block[BLOCK_SECTOR_SIZE/4];
  int inode_state=inode->data.level2_ptr_index == 0
  switch (inode_state){
    case 1:
        free_map_allocate(1, &level1_block[inode->data.level1_ptr_index]);
        break;
    case 0:
        block_read(fs_device, level1_block[inode->data.level1_ptr_index], &level2_block);
  }

  while (inode->data.level2_ptr_index < BLOCK_SECTOR_SIZE / 4 || sector_desired != 0) {
      free_map_allocate(1, &level2_block[inode->data.level2_ptr_index]);
      block_write(fs_device, level2_block[inode->data.level2_ptr_index], zeros);
      inode->data.level2_ptr_index++;
      sector_desired--;
  }
  block_write(fs_device, level1_block[inode->data.level1_ptr_index], &level2_block);
  if (inode->data.level2_ptr_index == BLOCK_SECTOR_SIZE / 4) {
      inode->data.level2_ptr_index = 0;
      inode->data.level1_ptr_index++;
  }
  return sector_desired;
}

bool allocate_inode(struct inode_disk *disk_inode) {
    struct inode inode;
    /* Initiate the inode. */
    inode.length = 0;
    inode_extend(&inode, disk_inode->length);

    /* Copy the disk inode. */
    int count = 0;
    while ( count < 102) {
        disk_inode->pointers[count] = inode.data.pointers[count];
        count++;
    }
    disk_inode->level0_ptr_index = inode.data.level0_ptr_index;
    disk_inode->level1_ptr_index = inode.data.level1_ptr_index;
    disk_inode->level2_ptr_index = inode.data.level2_ptr_index;
    return true;
}

void deallocate_inode(struct inode *inode){
  /* Initiate the block sectors location. */
  size_t level0_sectors = bytes_to_data_sectors(inode->length);
  size_t level1_sectors = bytes_to_indirect_sectors(inode->length);
  size_t level2_sectors = bytes_to_double_indirect_sector(inode->length);

  unsigned int level0_ptr_index = 0;

  while (level0_sectors && level0_ptr_index < 100) {
      free_map_release(inode->data.pointers[level0_ptr_index], 1);
      level0_sectors--;
      level0_ptr_index++;
  }

  if(level1_sectors){
    size_t single = level0_sectors;
    single=(single>BLOCK_SECTOR_SIZE/4)?(BLOCK_SECTOR_SIZE/4):single;
    inode_dealloc_indirect_block(&inode->data.pointers[level0_ptr_index], single);
    level0_sectors -= single;
    level1_sectors --;
    level0_ptr_index++;
  }
  if(level2_sectors){
      unsigned int i;
      block_sector_t ptr_block[BLOCK_SECTOR_SIZE/4];
      block_read(fs_device, inode->data.pointers[level0_ptr_index] , &ptr_block);
      for (i = 0; i < level1_sectors; i++) {
          size_t data_per_block = BLOCK_SECTOR_SIZE/4;
          if (data_per_block > level0_sectors) {
              data_per_block = level0_sectors;
          }
          inode_dealloc_indirect_block(&ptr_block[i], data_per_block);
          level0_sectors -= data_per_block;
      }
      free_map_release(inode->data.pointers[level0_ptr_index], 1);
  }
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

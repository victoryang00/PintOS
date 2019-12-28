#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"

#include "threads/thread.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* add by hya: to change a file/dir name to coresponding inode */
static struct inode * name_to_inode (const char *name);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();
  // initial cache
  filesys_cache_init();

  if (format) 
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  // write back all cache t
  filesys_cache_write_to_disk(true);
  
  free_map_close ();
}

/* hya add: Extracts a file name part from *SRCP into PART,
   and updates *SRCP so that the next call will return the next
   file name part.
   Returns 1 if successful, 0 at end of string, -1 for a too-long
   file name part. */
static int
get_next_part (char part[NAME_MAX], const char **srcp)
{
  const char *src = *srcp;
  char *dst = part;

  /* Skip leading slashes.
     If it's all slashes, we're done. */
  while (*src == '/')
    src++;
  if (*src == '\0')
    return 0;

  /* Copy up to NAME_MAX character from SRC to DST.
     Add null terminator. */
  while (*src != '/' && *src != '\0') 
    {
      if (dst < part + NAME_MAX)
        *dst++ = *src;
      else
        return -1;
      src++; 
    }
  *dst = '\0';

  /* Advance source pointer. */
  *srcp = src;
  return 1;
}

/* hya add: parse the path and then open the last level directory in dirp, 
  the name of file/dir need to create is put in base_name.
.*/
static bool
parse_file_path (const char *name,
                       struct dir **dirp, char base_name[NAME_MAX + 1]) 
{
  struct dir *dir = NULL;
  struct inode *inode;
  const char *cp;
  char part[NAME_MAX + 1], next_part[NAME_MAX + 1];
  int ok;
  
  /* Find starting directory. */
  if (name[0] == '/' || thread_current ()->cwd == NULL)
    dir = dir_open_root ();
  else
    dir = dir_reopen (thread_current ()->cwd);
  if (dir == NULL || !is_dir_exist(dir)){  // check if this directory has been removed
    /* Return failure. */
    dir_close (dir);
    *dirp = NULL;
    base_name[0] = '\0';
    return false;
  }

  /* Get first name part. */
  cp = name;
  if (get_next_part (part, &cp) <= 0){
    /* Return failure. */
    dir_close (dir);
    *dirp = NULL;
    base_name[0] = '\0';
    return false;
  }

  /* As long as another part follows the current one,
     traverse down another directory. */
  while ((ok = get_next_part (next_part, &cp)) > 0)
    {
      if (!dir_lookup (dir, part, &inode)){
        /* Return failure. */
        dir_close (dir);
        *dirp = NULL;
        base_name[0] = '\0';
        return false;

      }

      dir_close (dir);
      dir = dir_open (inode);
      if (dir == NULL || !is_dir_exist(dir)){
        /* Return failure. */
        dir_close (dir);
        *dirp = NULL;
        base_name[0] = '\0';
        return false;
      }

      strlcpy (part, next_part, NAME_MAX + 1);
    }
  if (ok < 0){
    /* Return failure. */
    dir_close (dir);
    *dirp = NULL;
    base_name[0] = '\0';
    return false;
  }

  /* Return our results. */
  *dirp = dir;
  strlcpy (base_name, part, NAME_MAX + 1);
  return true;
}


/* add by hya: parse path and create the file*/
bool
filesys_create (const char *name, off_t initial_size) 
{
  struct dir *dir;
  char base_name[NAME_MAX + 1];
  block_sector_t inode_sector;  // new mallocate sector space to store new dir

  bool success = (parse_file_path (name, &dir, base_name)
                  && free_map_allocate (1, &inode_sector));
  if (success) 
    {
      struct inode *inode;
      inode = file_create (inode_sector, initial_size); 
      if (inode != NULL)
        {
          success = dir_add (dir, base_name, inode_sector);
          if (!success)
            inode_remove (inode);
          inode_close (inode);
        }
      else
        success = false;
        
    }
  dir_close (dir);

  return success;
}

/* add by hya: parse path and create the dir */
bool
filesys_dir_create (const char *name, off_t initial_size) 
{
  struct dir *dir;
  char base_name[NAME_MAX + 1];
  block_sector_t inode_sector;  // new mallocate sector space to store new dir

  bool success = (parse_file_path (name, &dir, base_name)
                  && free_map_allocate (1, &inode_sector));
  if (success) 
    {
      struct inode *inode;
      inode = dir_create (inode_sector,
                            inode_get_inumber (dir_get_inode (dir))); 
      if (inode != NULL)
        {
          success = dir_add (dir, base_name, inode_sector);
          if (!success)
            inode_remove (inode);
          inode_close (inode);
        }
      else
        success = false;
        
    }
  dir_close (dir);

  return success;
}


/* modified by hya:, Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists, in parse_file_path check if upper level directory exist. */
struct file *
filesys_open (const char *name)
{


   if (name[0] == '/' && name[strspn (name, "/")] == '\0') 
    {
      /*if it is root dir */
      return file_open(inode_open (ROOT_DIR_SECTOR));
    }
  else 
    {
      struct dir *dir;
      char base_name[NAME_MAX + 1];

      if (parse_file_path (name, &dir, base_name)) 
        {
          struct inode *inode;
          dir_lookup (dir, base_name, &inode);
          dir_close (dir);
          return file_open(inode); 
        }
      else
        return NULL;
    }
}

/* hya add: test if it is can move, usually test dirctory, I treat dirctory as filer, 
since it is also inode*/
bool can_move(const char* name){

  struct file * file = filesys_open (name);
  if(file==NULL){
    return false;  // file does not exist, so need not move
  }else if(is_really_file(file)){
      file_close(file);
      return true; // we don't care about file
  }
  struct dir* dir = dir_open(file->inode);
  if(dir==NULL || !is_empty_dir(dir)){
    dir_close(dir);
    return false;
  }
  dir_close(dir);
  return true;
}
/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  // struct dir *dir = dir_open_root ();
  // bool success = dir != NULL && dir_remove (dir, name);

  if(!can_move(name)){
    return false;
  }
  struct dir *dir;
  char base_name[NAME_MAX + 1];
  bool success = parse_file_path (name, &dir, base_name);
  if(success){
    success = dir_remove(dir, base_name);
  }

  dir_close (dir); 

  return success;
}

/* modified by hya: Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();

  /* Set up root directory. */
  struct inode *inode = dir_create (ROOT_DIR_SECTOR, ROOT_DIR_SECTOR);
  if (inode == NULL)
    PANIC ("root directory creation failed");
  inode_close (inode);

  free_map_close ();
  printf ("done.\n");
}

/* add by hya: Change current directory to the given path.
   Return true if successful, otherwise false. */
bool
filesys_chdir (const char *name) 
{
  struct dir *dir = dir_open (name_to_inode (name));
  if (dir != NULL) 
    {
      dir_close (thread_current ()->cwd);
      thread_current ()->cwd = dir;
      return true;
    }
  else
    return false;
}


/* add by hya:  Resolves relative or absolute file NAME to an inode.
   Returns an inode if successful, otherwise a null pointer.*/
static struct inode *
name_to_inode (const char *name)
{
  if (name[0] == '/' && name[strspn (name, "/")] == '\0') 
    {
      // it it as root dirtory
      return inode_open (ROOT_DIR_SECTOR);
    }
  else 
    {
      struct dir *dir;
      char base_name[NAME_MAX + 1];

      if (parse_file_path (name, &dir, base_name)) 
        {
          struct inode *inode;
          dir_lookup (dir, base_name, &inode);
          dir_close (dir);
          return inode; 
        }
      else
        return NULL;
    }
}
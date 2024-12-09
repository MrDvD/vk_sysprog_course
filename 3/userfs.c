#include "userfs.h"
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#include <stdio.h> // to remove

enum {
	BLOCK_SIZE = 512,
	MAX_FILE_SIZE = 1024 * 1024 * 100,
};

/** Global error code. Set from any function on any error. */
static enum ufs_error_code ufs_error_code = UFS_ERR_NO_ERR;

struct block {
	/** Block memory. */
	char *memory;
	/** How many bytes are occupied. */
	int occupied;
	/** Next block in the file. */
	struct block *next;
	/** Previous block in the file. */
	struct block *prev;

	/* PUT HERE OTHER MEMBERS */
};

struct file {
	/** Double-linked list of file blocks. */
	struct block *block_list;
	/**
	 * Last block in the list above for fast access to the end
	 * of file.
	 */
	struct block *last_block;
	/** How many file descriptors are opened on the file. */
	int refs;
	/** File name. */
	char *name;
	/** Files are stored in a double-linked list. */
	struct file *next;
	struct file *prev;

	/* PUT HERE OTHER MEMBERS */
   int size; // filesize
};

/** List of all files. */
static struct file *file_list = NULL;

struct filedesc {
	struct file *file;

	/* PUT HERE OTHER MEMBERS */
};

/**
 * An array of file descriptors. When a file descriptor is
 * created, its pointer drops here. When a file descriptor is
 * closed, its place in this array is set to NULL and can be
 * taken by next ufs_open() call.
 */
static struct filedesc **file_descriptors = NULL;
static int file_descriptor_count = 0;
static int file_descriptor_capacity = 0;

enum ufs_error_code
ufs_errno()
{
	return ufs_error_code;
}

int
ufs_get_null_filedesc()
{
   if (file_descriptors == NULL) {
      return -1;
   }
   for (unsigned long int i = 0; i < sizeof (file_descriptors) / sizeof (struct filedesc**); i++)
   {
      if (file_descriptors[i] == NULL)
      {
         return i;
      }
   }
   return -1;
}

struct file*
ufs_get_null_file()
{
   if (file_list == NULL) {
      return NULL;
   }
   for (unsigned int i = 0; i < sizeof (file_list) / sizeof (struct file*); i++)
   {
      if (file_list[i].refs == 0)
      {
         return &file_list[i];
      }
   }
   return NULL;
}

int
ufs_fd_exists(int fd)
{
   return !(fd < 0 || fd + 1 > file_descriptor_capacity || file_descriptors[fd] == NULL);
}

int
ufs_open(const char *filename, int flags)
{
   struct filedesc fd_obj;
   if (file_list != NULL)
   {
      for (unsigned long i = 0; i < sizeof (file_list) / sizeof (struct file*); i++)
      {
         if (strcmp((const char *) file_list[i].name, filename) == 0)
         {
            int fd = ufs_get_null_filedesc();
            fd_obj.file = &file_list[i];
            if (fd == -1) {
               fd = sizeof (file_descriptors) / sizeof (struct filedesc**);
               if (file_descriptors == NULL)
               {
                  fd--;
               }
               file_descriptors = (struct filedesc **) realloc(file_descriptors, sizeof(struct filedesc *) * (fd + 1));
               file_descriptors[fd] = &fd_obj;
               file_descriptor_capacity++;
            } else {
               file_descriptors[fd] = &fd_obj;
            }
            printf("!!! [well, this line is necessary for working]\n");
            file_descriptor_count++;
            return ++fd;
         }
      }
   }
   if (flags & 0x1)
   {
      struct file *f = ufs_get_null_file();
      struct file new_f;
      if (f == NULL) {
         f = &new_f;
         int list_size = sizeof (file_list) / sizeof (struct file*);
         if (file_list == NULL)
         {
            list_size--;
         }
         f->refs++;
         f->name = (char *) filename;
         file_list = (struct file*) realloc(file_list, sizeof(struct file) * (list_size + 1));
         file_list[list_size] = *f;
      } else {
         f->refs++;
         f->name = (char *) filename;
      }
      int fd = ufs_get_null_filedesc();
      if (fd == -1) {
         fd = sizeof (file_descriptors) / sizeof (struct filedesc**);
         if (file_descriptors == NULL)
         {
            fd--;
         }
         file_descriptors = (struct filedesc **) realloc(file_descriptors, sizeof(struct filedesc *) * (fd + 1));
         file_descriptors[fd] = &fd_obj;
         file_descriptor_capacity++;
      } else
      {
         file_descriptors[fd] = &fd_obj;
      }
      file_descriptors[fd]->file = f;
      file_descriptor_count++;
      return ++fd;

   } else
   {
      ufs_error_code = UFS_ERR_NO_FILE;
      return -1;
   }
}

ssize_t
ufs_write(int fd, const char *buf, size_t size)
{
   int real_fd = fd - 1;
   if (!ufs_fd_exists(real_fd)) {
      ufs_error_code = UFS_ERR_NO_FILE;
	   return -1;
   }
   if (file_descriptors[real_fd]->file->size + size > MAX_FILE_SIZE) {
      ufs_error_code = UFS_ERR_NO_MEM;
      return -1;
   }
   int last = sizeof(file_descriptors[real_fd]->file->block_list) / sizeof(struct block*);
   // if file had no data
   if (file_descriptors[real_fd]->file->block_list == NULL)
   {
      file_descriptors[real_fd]->file->block_list = (struct block*) malloc(sizeof(struct block));
      struct block b;
      b.memory = (char *) malloc(BLOCK_SIZE);
      file_descriptors[real_fd]->file->block_list[last] = b;
   }
   int j = file_descriptors[real_fd]->file->block_list->occupied;
   for (size_t i = 0; i < size; i++)
   {
      // if current block is full
      if (j >= BLOCK_SIZE)
      {
         last++; j = 0;
         file_descriptors[real_fd]->file->block_list = (struct block*) realloc(file_descriptors[real_fd]->file->block_list, sizeof(struct block) * last);
         struct block b;
         b.memory = (char *) malloc(BLOCK_SIZE);
         b.prev = &file_descriptors[real_fd]->file->block_list[last - 1]; 
         file_descriptors[real_fd]->file->block_list[last] = b;
         file_descriptors[real_fd]->file->block_list[last - 1].next = &b;
      }
      file_descriptors[real_fd]->file->block_list[last].memory[j] = buf[j];
      file_descriptors[real_fd]->file->block_list[last].occupied++;
      j++;
   }
   file_descriptors[real_fd]->file->size += size;
   return size;
}

ssize_t
ufs_read(int fd, char *buf, size_t size)
{
	/* IMPLEMENT THIS FUNCTION */
	(void)fd;
	(void)buf;
	(void)size;
	ufs_error_code = UFS_ERR_NOT_IMPLEMENTED;
	return -1;
}

int
ufs_close(int fd)
{
   int real_fd = fd - 1;
   if (!ufs_fd_exists(real_fd)) {
      ufs_error_code = UFS_ERR_NO_FILE;
	   return -1;
   }
   file_descriptors[real_fd]->file->refs--;
   file_descriptors[real_fd] = NULL;
   file_descriptor_count--;
   return 0;
}

int
ufs_delete(const char *filename)
{
	struct file new_file; 
   new_file.refs = 0;
   for (unsigned long i = 0; i < sizeof (file_list) / sizeof (struct file*); i++)
   {
      if (strcmp((const char *) file_list[i].name, filename) == 0)
      {
         file_list[i] = new_file;
         return 0;
      }
   }
   ufs_error_code = UFS_ERR_NO_FILE;
	return -1;
}

#if NEED_RESIZE

int
ufs_resize(int fd, size_t new_size)
{
	/* IMPLEMENT THIS FUNCTION */
	(void)fd;
	(void)new_size;
	ufs_error_code = UFS_ERR_NOT_IMPLEMENTED;
	return -1;
}

#endif

void
ufs_destroy(void)
{
}

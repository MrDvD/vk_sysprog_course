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
   int size;
   struct block *first_block;
};

/** List of all files. */
static struct file *file_list = NULL;

struct filedesc {
	struct file *file;
	/* PUT HERE OTHER MEMBERS */
   struct block *curr_block;
   int block_number;
   int offset;
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
ufs_get_free_fd()
{
   for (int i = 0; i < file_descriptor_capacity; i++)
   {
      if (file_descriptors[i] == NULL)
      {
         return i;
      }
   }
   return -1;
}

struct file*
ufs_get_last_file()
{
   struct file *curr_file = &file_list[0];
   while (curr_file->next != NULL) {
      curr_file = curr_file->next;
   }
   return curr_file;
}

int
ufs_get_fd() {
   file_descriptor_count++;
   if (file_descriptors == NULL)
   {
      file_descriptors = malloc(sizeof(struct filedesc*));
      struct filedesc *new_fd = malloc(sizeof(struct filedesc));
      file_descriptors[0] = new_fd;
      file_descriptor_capacity++;
      return 0;
   }
   int fd = ufs_get_free_fd();
   if (fd == -1)
   {
      fd = file_descriptor_capacity;
      file_descriptors = (struct filedesc **) realloc(file_descriptors, sizeof(struct filedesc *) * (fd + 1));
      file_descriptor_capacity++;
   }
   struct filedesc *new_fd = malloc(sizeof(struct filedesc));
   file_descriptors[fd] = new_fd;
   return fd;
}

int
ufs_fd_exists(int fd)
{
   return !(fd < 0 || fd + 1 > file_descriptor_capacity || file_descriptors[fd] == NULL);
}

int
ufs_open(const char *filename, int flags)
{
   if (file_list != NULL)
   {
      struct file *curr_file = &file_list[0];
      while (1)
      {
         if (strcmp((const char *) curr_file->name, filename) == 0)
         {
            // filling a new fd
            int fd = ufs_get_fd();
            curr_file->refs++;
            file_descriptors[fd]->file = curr_file;
            file_descriptors[fd]->curr_block = curr_file->first_block;
            file_descriptors[fd]->block_number = 0;
            file_descriptors[fd]->offset = 0;
            return ++fd;
         }
         if (curr_file->next == NULL)
            break;
         curr_file = curr_file->next;
      }
   }
   if (flags & 0x1)
   {
      // creating a new file
      struct file* new_file;
      if (file_list == NULL) {
         file_list = malloc(sizeof(struct file));
         new_file = &file_list[0];
      } else {
         struct file *last_file = ufs_get_last_file();
         new_file = malloc(sizeof(struct file));
         last_file->next = new_file;
      }
      // filling a new file
      char *name_copy = malloc(sizeof(filename));
      strcpy(name_copy, filename);
      new_file->name = name_copy;
      new_file->refs++; 
      new_file->block_list = malloc(sizeof(struct block));
      struct block *b = malloc(sizeof(struct block));
      b->memory = (char*) malloc(BLOCK_SIZE);
      new_file->block_list[0] = *b;
      new_file->first_block = b;
      new_file->last_block = b;
      new_file->size = 0;
      // filling a new fd
      int fd = ufs_get_fd();
      file_descriptors[fd]->file = new_file;
      file_descriptors[fd]->curr_block = file_descriptors[fd]->file->first_block;
      file_descriptors[fd]->block_number = 0;
      file_descriptors[fd]->offset = 0;
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
   if (!ufs_fd_exists(real_fd))
   {
      ufs_error_code = UFS_ERR_NO_FILE;
	   return -1;
   }
   if (file_descriptors[real_fd]->file->size + size > MAX_FILE_SIZE)
   {
      ufs_error_code = UFS_ERR_NO_MEM;
      return -1;
   }
   for (size_t i = 0; i < size; i++)
   {
      // if current block is full
      if (file_descriptors[real_fd]->offset >= BLOCK_SIZE)
      {
         file_descriptors[real_fd]->offset = 0;
         file_descriptors[real_fd]->block_number++;
         // if the next block is not allocated
         if (file_descriptors[real_fd]->curr_block->next == NULL) {
            file_descriptors[real_fd]->file->block_list = (struct block*) realloc(file_descriptors[real_fd]->file->block_list, sizeof(struct block) * (file_descriptors[real_fd]->block_number + 1));
            struct block *b = malloc(sizeof(struct block));
            b->memory = (char *) malloc(BLOCK_SIZE);
            b->prev = file_descriptors[real_fd]->file->last_block;
            file_descriptors[real_fd]->file->last_block->next = b;
            file_descriptors[real_fd]->file->last_block = b;
            file_descriptors[real_fd]->curr_block = file_descriptors[real_fd]->file->last_block;
         } else {
            file_descriptors[real_fd]->curr_block = file_descriptors[real_fd]->curr_block->next;
         }
      }
      file_descriptors[real_fd]->curr_block->memory[file_descriptors[real_fd]->offset] = buf[i];
      file_descriptors[real_fd]->offset++;
      if (file_descriptors[real_fd]->curr_block->next == NULL && file_descriptors[real_fd]->offset > file_descriptors[real_fd]->curr_block->occupied) {
         file_descriptors[real_fd]->file->last_block->occupied++;
      }
   }
   int curr_idx = file_descriptors[real_fd]->block_number * BLOCK_SIZE + file_descriptors[real_fd]->offset;
   file_descriptors[real_fd]->file->size = (curr_idx >= file_descriptors[real_fd]->file->size ? curr_idx : file_descriptors[real_fd]->file->size);
   return size;
}

ssize_t
ufs_read(int fd, char *buf, size_t size)
{
   int real_fd = fd - 1;
   if (!ufs_fd_exists(real_fd))
   {
      ufs_error_code = UFS_ERR_NO_FILE;
	   return -1;
   }
   size_t i = file_descriptors[real_fd]->block_number * BLOCK_SIZE + file_descriptors[real_fd]->offset;
   size_t start = i;
   size_t min = ((size_t) file_descriptors[real_fd]->file->size >= size + i ? size + i : (size_t) file_descriptors[real_fd]->file->size);
   while (i < min)
   {
      buf[i - start] = file_descriptors[real_fd]->curr_block->memory[file_descriptors[real_fd]->offset];
      file_descriptors[real_fd]->offset++;
      i++;
      if (file_descriptors[real_fd]->offset >= BLOCK_SIZE) {
         file_descriptors[real_fd]->curr_block = file_descriptors[real_fd]->curr_block->next;
         file_descriptors[real_fd]->block_number++;
         file_descriptors[real_fd]->offset = 0;
      }
   }
	return min - start;
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
   // if it's the first file
   if (strcmp((const char *) file_list[0].name, filename) == 0)
   {
      if (file_list[0].next != NULL) {
         file_list[0] = *file_list[0].next;
      } else {
         file_list = NULL;
      }
      return 0;
   }
   // otherwise
   struct file *curr_file = &file_list[0];
   while (1)
   {
      if (strcmp((const char *) curr_file->name, filename) == 0)
      {
         if (curr_file->prev != NULL)
            curr_file->prev->next = curr_file->next;
         if (curr_file->next != NULL)
            curr_file->next->prev = curr_file->prev;
         return 0;
      }
      if (curr_file->next == NULL)
         break;
      curr_file = curr_file->next;
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

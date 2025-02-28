#include "userfs.h"
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

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
   int mode;
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

/* Returns a pointer to the new empty block */
struct block*
ufs_init_block()
{
   struct block *new_block = malloc(sizeof(struct block));
   new_block->memory = malloc(BLOCK_SIZE);
   new_block->occupied = 0;
   new_block->next = NULL;
   new_block->prev = NULL;
   return new_block;
}

/* Returns a pointer to the new fd */
struct filedesc*
ufs_init_fd()
{
   struct filedesc *new_fd = malloc(sizeof(struct filedesc));
   new_fd->curr_block = 0;
   new_fd->offset = 0;
   return new_fd;
}

/* Returns a pointer to the free & usable fd */
int
ufs_get_fd() {
   file_descriptor_count++;
   if (file_descriptors == NULL)
   {
      file_descriptors = malloc(sizeof(struct filedesc*));
      file_descriptors[0] = ufs_init_fd();
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
   file_descriptors[fd] = ufs_init_fd();
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
            if (!(flags & 14))
               file_descriptors[fd]->mode = 8;
            else
               file_descriptors[fd]->mode = flags;
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
         new_file->prev = NULL;
      } else {
         struct file *last_file = ufs_get_last_file();
         new_file = malloc(sizeof(struct file));
         new_file->prev = last_file;
         last_file->next = new_file;
      }
      new_file->next = NULL;
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
      if (!(flags & 14))
         file_descriptors[fd]->mode = 8;
      else
         file_descriptors[fd]->mode = flags;
      return fd + 1;
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
   if (!(file_descriptors[real_fd]->mode & 12))
   {
      ufs_error_code = UFS_ERR_NO_PERMISSION;
      return -1;
   }
   if (file_descriptors[real_fd]->block_number * BLOCK_SIZE + file_descriptors[real_fd]->offset + size > MAX_FILE_SIZE)
   {
      ufs_error_code = UFS_ERR_NO_MEM;
      return -1;
   }
   int pointer = file_descriptors[real_fd]->block_number * BLOCK_SIZE + file_descriptors[real_fd]->offset;
   if (pointer > file_descriptors[real_fd]->file->size)
   {
      file_descriptors[real_fd]->block_number = file_descriptors[real_fd]->file->size / BLOCK_SIZE;
      file_descriptors[real_fd]->offset = file_descriptors[real_fd]->file->size % BLOCK_SIZE;
      file_descriptors[real_fd]->curr_block = file_descriptors[real_fd]->file->last_block;
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
            struct block *new_block = ufs_init_block();
            new_block->prev = file_descriptors[real_fd]->file->last_block;
            file_descriptors[real_fd]->file->last_block->next = new_block;
            file_descriptors[real_fd]->file->last_block = new_block;
         }
         file_descriptors[real_fd]->curr_block = file_descriptors[real_fd]->curr_block->next;
      }
      file_descriptors[real_fd]->curr_block->memory[file_descriptors[real_fd]->offset] = buf[i];
      file_descriptors[real_fd]->offset++;
      // increment occupied
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
   if (!(file_descriptors[real_fd]->mode & 10))
   {
      ufs_error_code = UFS_ERR_NO_PERMISSION;
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
   if (!ufs_fd_exists(real_fd))
   {
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
   if (file_list != NULL)
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
   }
   ufs_error_code = UFS_ERR_NO_FILE;
	return -1;
}

#if NEED_RESIZE

int
ufs_resize(int fd, size_t new_size)
{
   int real_fd = fd - 1;
   if (!ufs_fd_exists(real_fd))
   {
      ufs_error_code = UFS_ERR_NO_FILE;
	   return -1;
   }
   if (!(file_descriptors[real_fd]->mode & 12))
   {
      ufs_error_code = UFS_ERR_NO_PERMISSION;
      return -1;
   }
   if (file_descriptors[real_fd]->file->size < (int) new_size)
   {
      int new_blocks = (new_size - file_descriptors[real_fd]->file->size + file_descriptors[real_fd]->file->size % BLOCK_SIZE - 1) / BLOCK_SIZE;
      for (int i = 0; i < new_blocks; i++)
      {
         struct block* new_block = ufs_init_block();
         new_block->prev = file_descriptors[real_fd]->file->last_block;
         file_descriptors[real_fd]->file->last_block->next = new_block;
         file_descriptors[real_fd]->file->last_block = new_block;
      }
   } else
   {
      int block_count = new_size / BLOCK_SIZE;
      struct block *last_block = file_descriptors[real_fd]->file->first_block;
      for (int i = 0; i < block_count; i++)
         last_block = last_block->next;
      last_block->next = NULL;
      int i = BLOCK_SIZE - 1;
      while (i >= ((int) new_size % BLOCK_SIZE)) {
         last_block->memory[i] = 0;
         i--;
      }
      file_descriptors[real_fd]->file->last_block = last_block;
   }
   file_descriptors[real_fd]->file->size = new_size;
   return 0;
}

#endif

void
ufs_destroy(void)
{
}

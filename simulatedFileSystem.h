#ifndef SIMULATED_FILE_SYSTEM_H
#define SIMULATED_FILE_SYSTEM_H

#include <stdbool.h>

#define BLOCK_SIZE 5000
#define MAX_STRING_LENGTH 20
#define MAX_FILE_DATA_BLOCKS (BLOCK_SIZE-64*59) //Hard-coded as of now
#define MAX_SUBDIRECTORIES  (BLOCK_SIZE - 136)/MAX_STRING_LENGTH

typedef struct dir_type {
	char name[MAX_STRING_LENGTH];		//Name of file or dir
	char top_level[MAX_STRING_LENGTH];	//Name of directory one level up(immediate parent)
	char (*subitem)[MAX_STRING_LENGTH];
	bool subitem_type[MAX_SUBDIRECTORIES];	//true if directory, false if file
	int subitem_count;
	struct dir_type *next;
} dir_type;


typedef struct file_type {
	char name[MAX_STRING_LENGTH];		//Name of file or dir
	char top_level[MAX_STRING_LENGTH];	//Name of directory one level up 
	int data_block_index[MAX_FILE_DATA_BLOCKS];
	int data_block_count;
	int size;
	struct file_type *next;
} file_type;

char *disk;

struct file_data {
    char *name;
    int is_directory;
    struct file_data *next;
};

struct file_data* do_ls(const char *path);

// Existing function declarations
int do_root(char *name, char *size);
int do_mkdir(char *name, char *size);
int do_mkfil(char *name, char *size);
int do_chdir(char *name, char *size);
int do_mvdir(char *name, char *size);
int do_mvfil(char *name, char *size);
int do_rmdir(char *name, char *size);
int do_rmfil(char *name, char *size);
int find_block(char *name, bool directory);

#endif // SIMULATED_FILE_SYSTEM_H

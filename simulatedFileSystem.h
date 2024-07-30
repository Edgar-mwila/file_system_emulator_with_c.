#ifndef SIMULATED_FILE_SYSTEM_H
#define SIMULATED_FILE_SYSTEM_H

#include <stdbool.h>

#define BLOCK_SIZE 5000
#define NUM_BLOCKS 5000
#define MAX_STRING_LENGTH 20
#define MAX_FILE_DATA_BLOCKS (BLOCK_SIZE-64*59) //Hard-coded as of now
#define MAX_SUBDIRECTORIES  (BLOCK_SIZE - 136)/MAX_STRING_LENGTH
#define MAX_FILE_SIZE (BLOCK_SIZE - sizeof(int) * (MAX_FILE_DATA_BLOCKS + 3) - MAX_STRING_LENGTH * 2)

typedef struct dir_type {
	char name[MAX_STRING_LENGTH];
	char top_level[MAX_STRING_LENGTH];
	char (*subitem)[MAX_STRING_LENGTH];
	bool subitem_type[MAX_SUBDIRECTORIES];
	int subitem_count;
	struct dir_type *next;
} dir_type;


typedef struct file_type {
	char name[MAX_STRING_LENGTH];		
	char top_level[MAX_STRING_LENGTH];
	int data_block_index[MAX_FILE_DATA_BLOCKS];
	int data_block_count;
	int size;
	struct file_type *next;
	char content[MAX_FILE_SIZE];
} file_type;

char *disk;

typedef struct {
    char path[512];
    char password[256];
} SecuredItem;

// Global array to store secured items
#define MAX_SECURED_ITEMS 100
extern SecuredItem secured_items[MAX_SECURED_ITEMS];
extern int secured_item_count;

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
int do_write(char *filename, char *content);
char* do_read(char *filename);
int find_block(char *name, bool directory);
int find_free_block();

#endif 

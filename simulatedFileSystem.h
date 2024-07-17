#ifndef SIMULATED_FILE_SYSTEM_H
#define SIMULATED_FILE_SYSTEM_H

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
int do_mvdir(char *name, char *size);
int do_mvfil(char *name, char *size);
int do_rmdir(char *name, char *size);
int do_rmfil(char *name, char *size);


#endif // SIMULATED_FILE_SYSTEM_H

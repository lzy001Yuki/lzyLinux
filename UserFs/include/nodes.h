#ifndef NODES_H
#define NODES_H


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <assert.h>
#include <time.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
// file/directory
struct inode{
    struct stat st;
    char* context;
    size_t capacity;
    pthread_mutex_t lock;
};


struct entry_{
    char name[256];
    struct inode* i_node;
};

struct fileSys{
    struct inode* root;
    pthread_mutex_t global_lock;
};

ino_t inode_idx[1024]; // queue structure to record deleted inode_idx (space reuse)
ino_t head = 0;
ino_t tail = 0;
ino_t total; 
#endif // NODES_H
#ifndef TOOLS_H
#define TOOLS_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nodes.h"
static struct inode *scanner(const char *path);

static struct entry_ *find_in_dir(struct inode *dir, const char *name);

static int find_parent(const char *path, struct inode **parent, char *child_name);

static struct inode *create_inode(mode_t mode);

static void destroy_inode(struct inode *node);

#endif // TOOLS_H
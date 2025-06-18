#define FUSE_USE_VERSION 31 
#include "../include/tools.h"
#include "../include/nodes.h"
#include <fuse.h>
static struct inode *scanner(const char *path) {
    struct inode* root_ = ((struct fileSys *)(fuse_get_context()->private_data))->root;
    if (strcmp(path, "/") == 0) {
        return root_;
    }
    
    struct inode *cur = root_;
    char *path_copy = strdup(path);
    char *token = strtok(path_copy, "/");
    
    while (token != NULL) {
        pthread_mutex_lock(&cur->lock);
        struct entry_ *en = find_in_dir(cur, token);
        pthread_mutex_unlock(&cur->lock);
        
        if (en == NULL) {
            free(path_copy);
            //printf("ERROR: no such file/directory\n");
            return NULL;
        }
        cur = en->i_node;
        token = strtok(NULL, "/");
    }
    
    free(path_copy);
    return cur;
}

static struct entry_ *find_in_dir(struct inode *dir, const char *name) {
    if (!S_ISDIR(dir->st.st_mode)) return NULL;
    
    int entries = dir->st.st_size / sizeof(struct entry_);
    struct entry_ *all = (struct entry_ *)dir->context;
    
    for (int i = 0; i < entries; ++i) {
        if (strcmp(all[i].name, name) == 0) {
            return &all[i];
        }
    }
    return NULL;
}

static int find_parent(const char *path, struct inode **p, char *ch) {
    char *copy = strdup(path);
    char *end = strrchr(copy, '/');

    strcpy(ch, end + 1);
    
    struct inode *p_node;
    if (strcmp(end, copy) == 0) { 
        p_node = ((struct fileSys *)fuse_get_context()->private_data)->root;
    } else {
        *end = '\0';
        p_node = scanner(copy);
    }
    
    free(copy);
    
    if (p_node == NULL) {
        return -ENOENT;
    }
    *p = p_node;
    return 0;
}

static struct inode *create_inode(mode_t mode) {
    struct inode *node = (struct inode *)malloc(sizeof(struct inode));
    if (!node) return NULL;
    
    memset(node, 0, sizeof(struct inode));

    
    if (head != tail) {
        node->st.st_ino = inode_idx[head];
        head = (head + 1) % 1024;
    } else {
        node->st.st_ino = total;
        total++;
    }
    
    node->st.st_mode = mode;
    node->st.st_uid = fuse_get_context()->uid;
    node->st.st_gid = fuse_get_context()->gid;
    node->st.st_atime = node->st.st_mtime = node->st.st_ctime = time(NULL);

    if (S_ISDIR(mode)) {
        node->st.st_nlink = 2; 
    } else {
        node->st.st_nlink = 1;
    }
    
    pthread_mutex_init(&node->lock, NULL);
    
    return node;
}

static void destroy_inode(struct inode *node) {
    if (node) {
        inode_idx[tail] = node->st.st_ino;
        tail = (tail + 1) % 1024;
        pthread_mutex_destroy(&node->lock);
        free(node->context);
        free(node);
    }
}

static void* user_init(struct fuse_conn_info *conn, struct fuse_config *cfg) {
    total = 1;
    struct fileSys *sys = (struct fileSys *)malloc(sizeof(struct fileSys));
    pthread_mutex_init(&sys->global_lock, NULL);

    sys->root = create_inode(S_IFDIR | 0755);
    sys->root->st.st_ino = total; 
    total++;
    
    fuse_get_context()->private_data = sys;
    return sys;
}


static void user_destroy(void *private_data) {
    struct fileSys *ctx = (struct fileSys *)private_data;
    destroy_inode(ctx->root);
    pthread_mutex_destroy(&ctx->global_lock);
    free(ctx);
}

static int user_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    struct inode *node = scanner(path);
    if (!node) return -ENOENT;
    pthread_mutex_lock(&node->lock);
    memcpy(stbuf, &node->st, sizeof(struct stat));
    fprintf(stderr, "getattr called: %d\n", node->st.st_ino);
    stbuf->st_ino = node->st.st_ino;
    pthread_mutex_unlock(&node->lock);
    
    return 0;
}

static int user_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    struct inode *dir = scanner(path);
    if (!dir) return -ENOENT;
    if (!S_ISDIR(dir->st.st_mode)) return -ENOTDIR;
    filler(buf, ".", &dir->st, 0);
    int cnt = dir->st.st_size / sizeof(struct entry_);
    struct entry_ *entries = (struct entry_ *)dir->context;
    
    for (int i = 0; i < cnt; i++) {
        filler(buf, entries[i].name, &entries[i].i_node->st, 0);
    }
    
    pthread_mutex_unlock(&dir->lock);
    return 0;
}


static int user_mknod(const char *path, mode_t mode, dev_t rdev) {
    char child[256];
    struct inode *p;
    if (find_parent(path, &p, child) != 0) return -ENOENT;
    
    pthread_mutex_lock(&p->lock);
    
    if (find_in_dir(p, child)) {
        pthread_mutex_unlock(&p->lock);
        return -EEXIST;
    }
    
    struct inode *new_node = create_inode(S_IFREG | mode);
    if (!new_node) {
        pthread_mutex_unlock(&p->lock);
        return -ENOMEM;
    }
    size_t old_size = p->st.st_size;
    size_t new_size = old_size + sizeof(struct entry_);
    
    p->context = realloc(p->context, new_size);
    if (!p->context) { 
         destroy_inode(new_node);
         pthread_mutex_unlock(&p->lock);
         return -ENOMEM;
    }
    
    struct entry_ *new_entry_ = (struct entry_ *)(p->context + old_size);
    strcpy(new_entry_->name, child);
    new_entry_->i_node = new_node;
    
    p->st.st_size = new_size;
    p->st.st_mtime = p->st.st_ctime = time(NULL);
    
    pthread_mutex_unlock(&p->lock);
    return 0;
}

static int user_mkdir(const char *path, mode_t mode) {
    char child[256];
    struct inode *p;
    if (find_parent(path, &p, child) != 0) return -ENOENT;
    
    pthread_mutex_lock(&p->lock);
    
    if (find_in_dir(p, child)) {
        pthread_mutex_unlock(&p->lock);
        return -EEXIST;
    }
    
    struct inode *new_node = create_inode(S_IFDIR | mode);
    if (!new_node) {
        pthread_mutex_unlock(&p->lock);
        return -ENOMEM;
    }
    size_t old_size = p->st.st_size;
    size_t new_size = old_size + sizeof(struct entry_);
    
    p->context = realloc(p->context, new_size);
    strcpy(((struct entry_ *)(p->context + old_size))->name, child);
    ((struct entry_ *)(p->context + old_size))->i_node = new_node;
    
    p->st.st_size = new_size;
    p->st.st_nlink++;
    p->st.st_mtime = p->st.st_ctime = time(NULL);
    
    pthread_mutex_unlock(&p->lock);
    return 0;
}

static int user_unlink(const char *path) {
    char child[256];
    struct inode *p;
    if (find_parent(path, &p, child) != 0) return -ENOENT;

    pthread_mutex_lock(&p->lock);
    struct entry_ *del = find_in_dir(p, child);
    
    if (!del) {
        pthread_mutex_unlock(&p->lock);
        return -ENOENT;
    }

    struct inode *del_node = del->i_node;
    pthread_mutex_lock(&del_node->lock);
    
    if (S_ISDIR(del_node->st.st_mode)) {
        pthread_mutex_unlock(&del_node->lock);
        pthread_mutex_unlock(&p->lock);
        return -EISDIR;
    }

    int cnt = p->st.st_size / sizeof(struct entry_);
    struct entry_ *entries = (struct entry_ *)p->context;
    int i;
    for (i = 0; i < cnt; i++) {
        if (&entries[i] == del_node) {
            break;
        }
    }
    if (i < cnt) {
        memmove(&entries[i], &entries[i + 1], (cnt - i - 1) * sizeof(struct entry_));
        p->st.st_size -= sizeof(struct entry_);
        p->st.st_mtime = p->st.st_ctime = time(NULL);
    }
    
    del_node->st.st_nlink--;
    if (del_node->st.st_nlink == 0) {
        pthread_mutex_unlock(&del_node->lock);
        destroy_inode(del_node);
    } else {
        pthread_mutex_unlock(&del_node->lock);
    }
    
    pthread_mutex_unlock(&p->lock);
    return 0;
}

static int user_rmdir(const char *path) {
    char child[256];
    struct inode *p;
    if (find_parent(path, &p, child) != 0) return -ENOENT;

    pthread_mutex_lock(&p->lock);
    struct entry_ *del_dir = find_in_dir(p, child);
    
    if (!del_dir) {
        pthread_mutex_unlock(&p->lock);
        return -ENOENT;
    }
    
    struct inode *del_node = del_dir->i_node;
    pthread_mutex_lock(&del_node->lock);
    
    if (!S_ISDIR(del_node->st.st_mode)) {
        pthread_mutex_unlock(&del_node->lock);
        pthread_mutex_unlock(&p->lock);
        return -ENOTDIR;
    }
    if (del_node->st.st_size > 0) {
        pthread_mutex_unlock(&del_node->lock);
        pthread_mutex_unlock(&p->lock);
        return -ENOTEMPTY;
    }
    int count = p->st.st_size / sizeof(struct entry_);
    struct  entry_ *entries = (struct entry_ *)p->context;
    int i;
    for (i = 0; i < count; i++) {
        if (&entries[i] == del_dir) {
            break;
        }
    }
    if (i < count) {
        memmove(&entries[i], &entries[i + 1], (count - i - 1) * sizeof (struct entry_));
        p->st.st_size -= sizeof(struct entry_);
        p->st.st_nlink--;
        p->st.st_mtime = p->st.st_ctime = time(NULL);
    }

    del_node->st.st_nlink = 0; 
    pthread_mutex_unlock(&del_node->lock);
    destroy_inode(del_node);
    
    pthread_mutex_unlock(&p->lock);
    return 0;
}


static int user_link(const char* from, const char* to) {
    struct inode* from_node = scanner(from);
    if (!from_node) return -ENOENT;
    if (S_ISDIR(from_node->st.st_mode)) return -EPERM; 

    char child[256];
    struct inode *p;
    if (find_parent(to, &p, child) != 0) return -ENOENT;

    if ((void*)from_node < (void*)p) {
        pthread_mutex_lock(&from_node->lock);
        pthread_mutex_lock(&p->lock);
    } else {
        pthread_mutex_lock(&p->lock);
        pthread_mutex_lock(&from_node->lock);
    }

    if (find_in_dir(p, child)) {
        pthread_mutex_unlock(&p->lock);
        pthread_mutex_unlock(&from_node->lock);
        return -EEXIST;
    }

    size_t old_size = p->st.st_size;
    size_t new_size = old_size + sizeof(struct entry_);
    p->context = realloc(p->context, new_size);
    p->st.st_mtime = p->st.st_ctime = time(NULL);
    
    struct entry_ *new_entry_ = (struct entry_ *)(p->context + old_size);
    strcpy(new_entry_->name, child);
    new_entry_->i_node = from_node;
    new_entry_->i_node->st.st_ino = from_node->st.st_ino;
    
    p->st.st_size = new_size;

    from_node->st.st_nlink++;

    pthread_mutex_unlock(&p->lock);
    pthread_mutex_unlock(&from_node->lock);
    return 0;
}

// 不能直接link再unlink！ 不适用于文件夹 
// rename == mv
static int user_rename(const char *from, const char *to) {
    if (strncmp(to, from, strlen(from)) == 0 && to[strlen(from)] == '/') return -EINVAL;
    struct inode *src_node = scanner(from);
    if (!src_node) return -ENOENT;
    
    char f_name[256], t_name[256];
    struct inode *from_p, *to_p;
    
    int ret = find_parent(from, &from_p, f_name);
    if (ret != 0) return ret;
    
    ret = find_parent(to, &to_p, t_name);
    if (ret != 0) return ret;

    if ((uintptr_t)from_p < (uintptr_t)to_p) {
        pthread_mutex_lock(&from_p->lock);
        if (from_p != to_p) 
            pthread_mutex_lock(&to_p->lock);
    } else {
        pthread_mutex_lock(&to_p->lock);
        if (from_p != to_p) 
            pthread_mutex_lock(&from_p->lock);
    }
    
    struct entry_ *src_entry = find_in_dir(from_p, f_name);
    if (!src_entry) {
        if (from_p == to_p)
            pthread_mutex_unlock(&from_p->lock);
        else {
            pthread_mutex_unlock(&from_p->lock);
            pthread_mutex_unlock(&to_p->lock);
        }
        return -ENOENT;
    }
    
    pthread_mutex_lock(&src_entry->i_node->lock);
    
    struct entry_ *dest_entry = find_in_dir(to_p, t_name);
    if (dest_entry) {
        if (S_ISDIR(dest_entry->i_node->st.st_mode)) {
            pthread_mutex_lock(&dest_entry->i_node->lock);
            if (dest_entry->i_node->st.st_size > 0) {
                pthread_mutex_unlock(&dest_entry->i_node->lock);
                pthread_mutex_unlock(&src_entry->i_node->lock);
                if (from_p == to_p)
                    pthread_mutex_unlock(&from_p->lock);
                else {
                    pthread_mutex_unlock(&from_p->lock);
                    pthread_mutex_unlock(&to_p->lock);
                }
                return -ENOTEMPTY;
            }
            
            to_p->st.st_nlink--;
            dest_entry->i_node->st.st_nlink = 0;
            pthread_mutex_unlock(&dest_entry->i_node->lock);
            destroy_inode(dest_entry->i_node);
        } else {
            // 删除目标文件
            pthread_mutex_lock(&dest_entry->i_node->lock);
            dest_entry->i_node->st.st_nlink--;
            if (dest_entry->i_node->st.st_nlink == 0) {
                pthread_mutex_unlock(&dest_entry->i_node->lock);
                destroy_inode(dest_entry->i_node);
            } else {
                pthread_mutex_unlock(&dest_entry->i_node->lock);
            }
        }
        
        int entries = to_p->st.st_size / sizeof(struct entry_);
        struct entry_ *all_entries = (struct entry_ *)to_p->context;
        
        for (int i = 0; i < entries; i++) {
            if (strcmp(all_entries[i].name, t_name) == 0) {
                if (i < entries - 1) {
                    memmove(&all_entries[i], &all_entries[i+1], 
                            (entries - i - 1) * sizeof(struct entry_));
                }
                to_p->st.st_size -= sizeof(struct entry_);
                to_p->st.st_mtime = to_p->st.st_ctime = time(NULL);
                break;
            }
        }
    }
    
    size_t old_size = to_p->st.st_size;
    size_t new_size = old_size + sizeof(struct entry_);
    to_p->context = realloc(to_p->context, new_size);
    if (!to_p->context) {
        pthread_mutex_unlock(&src_entry->i_node->lock);
        if (from_p == to_p)
            pthread_mutex_unlock(&from_p->lock);
        else {
            pthread_mutex_unlock(&from_p->lock);
            pthread_mutex_unlock(&to_p->lock);
        }
        return -ENOMEM;
    }
    
    struct entry_ *new_entry = (struct entry_ *)(to_p->context + old_size);
    strcpy(new_entry->name, t_name);
    new_entry->i_node = src_entry->i_node;
    // fprintf(stderr, "new_inode: %ld\n", new_entry->i_node->st.st_ino);
    // fprintf(stderr, "prev_inode: %ld\n", src_entry->i_node->st.st_ino);
    
    to_p->st.st_size = new_size;
    to_p->st.st_mtime = to_p->st.st_ctime = time(NULL);
    
    if (S_ISDIR(src_entry->i_node->st.st_mode)) {
        to_p->st.st_nlink++;
    }
    
    // remove
    int entries = from_p->st.st_size / sizeof(struct entry_);
    struct entry_ *all_entries = (struct entry_ *)from_p->context;
    
    for (int i = 0; i < entries; i++) {
        if (strcmp(all_entries[i].name, f_name) == 0) {
            if (i < entries - 1) {
                memmove(&all_entries[i], &all_entries[i+1], 
                        (entries - i - 1) * sizeof(struct entry_));
            }
            from_p->st.st_size -= sizeof(struct entry_);
            from_p->st.st_mtime = from_p->st.st_ctime = time(NULL);
            
            if (S_ISDIR(src_entry->i_node->st.st_mode)) {
                from_p->st.st_nlink--;
            }
            break;
        }
    }
    
    // unlock
    pthread_mutex_unlock(&src_entry->i_node->lock);
    if (from_p == to_p)
        pthread_mutex_unlock(&from_p->lock);
    else {
        pthread_mutex_unlock(&from_p->lock);
        pthread_mutex_unlock(&to_p->lock);
    }
}

static int user_open(const char *path, struct fuse_file_info *fi) {
    struct inode *node = scanner(path);
    if (!node) return -ENOENT;
    return 0;
}

static int user_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    struct inode *node = scanner(path);
    if (!node) return -ENOENT;
    
    pthread_mutex_lock(&node->lock);
    
    if (S_ISDIR(node->st.st_mode)) {
        pthread_mutex_unlock(&node->lock);
        return -EISDIR;
    }
    
    if (offset >= node->st.st_size) {
        pthread_mutex_unlock(&node->lock);
        return 0;
    }
    
    size_t read_size = size;
    if (offset + size > node->st.st_size) {
        read_size = node->st.st_size - offset;
    }
    
    memcpy(buf, node->context + offset, read_size);
    
    pthread_mutex_unlock(&node->lock);
    return read_size;
}

static int user_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    struct inode *node = scanner(path);
    if (!node) return -ENOENT;

    pthread_mutex_lock(&node->lock);
    
    if S_ISDIR(node->st.st_mode) {
        pthread_mutex_unlock(&node->lock);
        return -EISDIR;
    }
    
    size_t new_size = offset + size;
    if (new_size > node->capacity) {
        size_t new_capacity = node->capacity;
        if (new_capacity == 0) new_capacity = 1;
        while (new_capacity < new_size) {
            new_capacity *= 2; 
        }
        
        char *new_data = realloc(node->context, new_capacity);
        if (!new_data) {
            pthread_mutex_unlock(&node->lock);
            return -ENOMEM;
        }
        node->context = new_data;
        node->capacity = new_capacity;
    }
    
    memcpy(node->context + offset, buf, size);
    if (new_size > node->st.st_size) {
        node->st.st_size = new_size;
    }
    
    
    pthread_mutex_unlock(&node->lock);
    return size;
}



static const struct fuse_operations ramfs_ops = {
    .init       = user_init,
    .destroy    = user_destroy,
    .getattr    = user_getattr,
    .readdir    = user_readdir,
    .mknod      = user_mknod,
    .mkdir      = user_mkdir,
    .unlink     = user_unlink,
    .rmdir      = user_rmdir,
    .link       = user_link,
    .rename     = user_rename,
    .open       = user_open,
    .read       = user_read,
    .write      = user_write,
};

int main(int argc, char *argv[]) {
    // FUSE的main函数会处理命令行参数，并进入事件循环
    return fuse_main(argc, argv, &ramfs_ops, NULL);
}
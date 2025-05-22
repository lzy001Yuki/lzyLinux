#include "impl.h"
void* mmap_remap(void *addr, size_t size) {
    void* new_map = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (addr != NULL) munmap(addr, size);
    printf("Mapped address: %p\n", new_map);
    if (new_map == MAP_FAILED) {
        perror("mmap failed");
        return NULL;
    }
    return new_map;
}

int file_mmap_write(const char* filename, size_t offset, char* content) {
    int fd = open(filename, O_CREAT | O_RDWR, 0644);
    if (fd == -1) return -1;
    struct stat st;
    if (fstat(fd, &st) == -1) {
        close(fd);
        return -1;
    }
    size_t total = strlen(content) + offset;
    if (st.st_size < total) {
        if (ftruncate(fd, total) == -1) {
            close(fd);
            return -1;
        }
    }
    size_t cur_size = (st.st_size > total) ? st.st_size : total;
    void* new_map = mmap(NULL, cur_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (new_map == MAP_FAILED) {
        close(fd);
        return -1;
    }
    
    memcpy((char*)new_map + offset, content, strlen(content));
    msync(new_map, cur_size, MS_SYNC);
    close(fd);
    return 0;
}
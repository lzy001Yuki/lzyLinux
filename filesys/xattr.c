#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/xattr.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

int get_ea(const char *path, const char *attr_name) {
    char value[1024];
    ssize_t size;
    
    size = getxattr(path, attr_name, value, sizeof(value));
    if (size == -1) {
        perror("getxattr");
        return -1;
    }
    
    printf("Sucessfully get attribute '%s' value: ", attr_name);
    for (ssize_t i = 0; i < size; i++) {
        printf("%c", value[i]);
    }
    printf("\n");
    
    return 0;
}


int set_ea(const char *path, const char *attr_name, const char *attr_value) {
    int ret;
    
    ret = setxattr(path, attr_name, attr_value, strlen(attr_value), 0);
    if (ret == -1) {
        perror("setxattr");
        return -1;
    }
    
    printf("Successfully set attribute '%s' to '%s'\n", attr_name, attr_value);
    return 0;
}


int list_ea(const char *path) {
    char list[1024];
    ssize_t size;
    
    size = listxattr(path, list, sizeof(list));
    if (size == -1) {
        perror("listxattr");
        return -1;
    }
    
    printf("List extended attributes for '%s':\n", path);
    
    for (ssize_t i = 0; i < size; i += strlen(&list[i]) + 1) {
        printf("  %s\n", &list[i]);
    }
    
    return 0;
}


int remove_ea(const char *path, const char *attr_name) {
    int ret;
    
    ret = removexattr(path, attr_name);
    if (ret == -1) {
        perror("removexattr");
        return -1;
    }
    
    printf("Successfully remove attribute '%s'\n", attr_name);
    return 0;
}

// test_main
int main(int argc, char *argv[]) {
    const char *command = argv[1];
    
    const char *file_path = argv[2];
    //printf("hello");
    if (strcmp(command, "get") == 0)  {
        get_ea(file_path, argv[3]);
    }
    else if (strcmp(command, "set") == 0)  set_ea(file_path, argv[3], argv[4]);
    else if (strcmp(command, "list") == 0)  list_ea(file_path);
    else if (strcmp(command, "remove") == 0)  remove_ea(file_path, argv[3]); 
    return 0;
}

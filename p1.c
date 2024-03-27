#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>

int main (int argc, char* argv[]){
    if (argc < 1){
        exit(-1);
    }

    DIR* directory = opendir(argv[1]);
    struct dirent *dir;

    while ((dir = readdir(directory))) {
        if (dir->d_name[0] != '.') {
            printf("%s\n", dir->d_name);
        }
    }

    closedir(directory);

    return 0;
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#define MAX_PATH_LEN 1024

void afisare(const char *director, FILE *output_file) {
    DIR *dir;
    struct dirent *entry;
    struct stat statbuf;
    char file_path[MAX_PATH_LEN];

    if (!(dir = opendir(director))) {
        printf("Eoare deschidere director!");
        exit(EXIT_FAILURE);
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        snprintf(file_path, sizeof(file_path), "%s/%s", director, entry->d_name);

        if (stat(file_path, &statbuf) == -1) {
            exit(EXIT_FAILURE);
        }

        fprintf(output_file, "%s\n", file_path);

        if (S_ISDIR(statbuf.st_mode)) {
            afisare(file_path, output_file);
        }
    }
    closedir(dir);
}

void snapshot(const char *director) {
    FILE *snap = fopen("current_snapshot.txt", "w");
    if (snap == NULL) {
        printf("Eroare creare snap!");
        exit(EXIT_FAILURE);
    }

    afisare(director, snap);

    fclose(snap);
}

void compare(const char *current_snapshot, const char *previous_snapshot){
    FILE *snaps = fopen(previous_snapshot, "r");
    if(snaps == NULL){
        printf("Nu exista snapshot anterior!");
        exit(EXIT_FAILURE);
    }

}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <directory>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *directory = argv[1];
    //const char *previous_snapshot = argv[3];

    snapshot(directory);
    //compare(snapshot_file, previous_snapshot);

    return EXIT_SUCCESS;
}
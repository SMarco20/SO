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
        perror("Eoare deschidere director!");
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

void snapshot(const char *director, const char *snapshot_file) {
    FILE *snap = fopen(snapshot_file, "w");
    if (snap == NULL) {
        perror("Eroare creare snap!");
        exit(EXIT_FAILURE);
    }

    afisare(director, snap);

    fclose(snap);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <directory> <snapshot_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *directory = argv[1];
    const char *snapshot_file = argv[2];

    snapshot(directory, snapshot_file);

    return EXIT_SUCCESS;
}

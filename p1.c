#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

struct Metadata
{
    char name[256];
    time_t last_modified;
    mode_t mode;
    off_t size;
    ino_t inode;
    int is_snapshot;
};

struct Metadata get_metadata(const char *path)
{
    struct Metadata meta;
    struct stat file_stat;

    if (stat(path, &file_stat) == -1)
    {
        perror("stat");
        exit(EXIT_FAILURE);
    }

    strncpy(meta.name, path, 255);
    meta.name[255] = '\0';
    meta.last_modified = file_stat.st_mtime;
    meta.mode = file_stat.st_mode;
    meta.size = file_stat.st_size;
    meta.inode = file_stat.st_ino;
    meta.is_snapshot = 0;

    return meta;
}

void creare_snapshot(const char *directory, FILE *snapshot_file)
{
    DIR *dir;
    struct dirent *entry;
    dir = opendir(directory);
    if (dir == NULL)
    {
        perror("opendir");
        exit(EXIT_FAILURE);
    }

    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue; // se ignora "." si ".."
        }

        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", directory, entry->d_name);

        struct Metadata meta = get_metadata(full_path);

        // marcajul fisierelor snapshot
        if (strcmp(entry->d_name, "snapshot_anterior.txt") == 0 || strcmp(entry->d_name, "snapshot_curent.txt") == 0)
        {
            meta.is_snapshot = 1;
        }

        // scriere în snapshot
        fprintf(snapshot_file, "%s %ld %o %lld %lu %d\n", meta.name, meta.last_modified, meta.mode, meta.size, meta.inode, meta.is_snapshot);

        // if director -> recursiv
        if (S_ISDIR(meta.mode))
        {
            creare_snapshot(full_path, snapshot_file);
        }
    }

    closedir(dir);
}

// se compara snapshot-urile Si se afiSeaza modificarile
void comparare(FILE *prev_snapshot, FILE *current_snapshot)
{
    struct Metadata prev_entry;
    struct Metadata current_entry;
    int changes_detected = 0; // Variabila pentru a verifica daca s-au detectat modificari
    // se citesc datele din snapshot-uri Si se compara fiSierele
    while (fscanf(current_snapshot, "%255s %ld %o %lld %lu %d\n", current_entry.name, &current_entry.last_modified, &current_entry.mode, &current_entry.size, &current_entry.inode, &current_entry.is_snapshot) != EOF)
    {
        if (current_entry.is_snapshot == 1)
        {
            continue; // se ignora fiSierele snapshot
        }

        int found = 0;
        rewind(prev_snapshot);
        /* se repozitioneaza cursorul la inceputul fisierului anterior pentru a putea parcurge din nou fisierul de la inceput pentru a compara fiecare intrare din fisierul anterior cu
        fiecare intrare din fisierul curent. */
        while (fscanf(prev_snapshot, "%255s %ld %o %lld %lu %d\n", prev_entry.name, &prev_entry.last_modified, &prev_entry.mode, &prev_entry.size, &prev_entry.inode, &prev_entry.is_snapshot) != EOF)
        {
            if (current_entry.inode == prev_entry.inode)
            {
                found = 1;
                if (strcmp(current_entry.name, prev_entry.name) != 0)
                {
                    printf("Redenumire: %s ->%s\n", prev_entry.name, current_entry.name);
                    changes_detected = 1;
                }

                if (current_entry.last_modified != prev_entry.last_modified) //Adaug asta în caz ca gasim fiSierul Si are acelaSi nume, verificam data ultimei modificari sa Stim daca e modificat
                {
                    printf("Modificare: %s\n", current_entry.name);
                    changes_detected = 1;
                }
                break;
            }
        }
        if (!found)
        {
            printf("Adaugare: %s\n", current_entry.name);
            changes_detected = 1;
        }
    }

    // verificare posibile fisiere sterse
    rewind(prev_snapshot); // Punem cursorul la început de fisier
    rewind(current_snapshot);
    while (fscanf(prev_snapshot, "%255s %ld %o %lld %lu %d\n", prev_entry.name, &prev_entry.last_modified, &prev_entry.mode, &prev_entry.size, &prev_entry.inode, &prev_entry.is_snapshot) != EOF)
    {
        if (prev_entry.is_snapshot == 1)
        {
            continue; // se ignora fisierele snapshot
        }

        int found = 0;
        rewind(current_snapshot);
        while (fscanf(current_snapshot, "%255s %ld %o %lld %lu %d\n", current_entry.name, &current_entry.last_modified, &current_entry.mode, &current_entry.size, &current_entry.inode, &current_entry.is_snapshot) != EOF)
        {
            if (prev_entry.inode == current_entry.inode)
            {
                found = 1;
                break;
            }
        }
        if (!found)
        {
            printf("Stergere: %s\n", prev_entry.name);
            changes_detected = 1;
        }
    }

    if (!changes_detected)
    {
        printf("Nu s-au detectat modificari.\n");
    }
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("Utilizare: %s <directory>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *directory = argv[1];

    FILE *prev_snapshot_file = fopen("snapshot_anterior.txt", "r");
    if (prev_snapshot_file == NULL)
    {
        printf("Snapshot-ul anterior nu a fost gasit. Se creeaza unul nou.\n");
        prev_snapshot_file = fopen("snapshot_anterior.txt", "w");
        if (prev_snapshot_file == NULL)
        {
            perror("fopen");
            return EXIT_FAILURE;
        }
        fclose(prev_snapshot_file);
        printf("Primul snapshot creat cu succes.\n");
    }

    FILE *current_snapshot_file = fopen("snapshot_curent.txt", "w");
    if (current_snapshot_file == NULL)
    {
        perror("fopen");
        return EXIT_FAILURE;
    }

    creare_snapshot(directory, current_snapshot_file);

    fclose(current_snapshot_file);
    printf("Snapshot creat cu succes.\n");

    // se compara anteriorul cu cel curent
    prev_snapshot_file = fopen("snapshot_anterior.txt", "r");
    current_snapshot_file = fopen("snapshot_curent.txt", "r");

    if (prev_snapshot_file == NULL || current_snapshot_file == NULL)
    {
        perror("fopen");
        return EXIT_FAILURE;
    }

    comparare(prev_snapshot_file, current_snapshot_file);

    fclose(prev_snapshot_file);
    fclose(current_snapshot_file);

    // se actualizeaza "snapshot_anterior.txt" prin copierea continutului din cel curent in cel anterior pentru urmatoarea rulare
    current_snapshot_file = fopen("snapshot_curent.txt", "r");
    prev_snapshot_file = fopen("snapshot_anterior.txt", "w");
    if (prev_snapshot_file == NULL || current_snapshot_file == NULL)
    {
        perror("fopen");
        return EXIT_FAILURE;
    }

    int c;
    while ((c = fgetc(current_snapshot_file)) != EOF)
    {
        fputc(c, prev_snapshot_file);
    }

    fclose(prev_snapshot_file);
    fclose(current_snapshot_file);

    return EXIT_SUCCESS;
}
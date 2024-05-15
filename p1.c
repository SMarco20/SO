
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <libgen.h>
#include <errno.h>
#include <fcntl.h>

#define MAX_PATH_LEN 1024

int cuvant_cheie(const char *linie)
 {
    const char *cuvant_cheie[] = {"corrupted", "dangerous", "risk", "attack", "malware", "malicious"};
    const int num_cuvinte_cheie = sizeof(cuvant_cheie) / sizeof(cuvant_cheie[0]);
    for (int i = 0; i < num_cuvinte_cheie; i++) 
    {
        if (strstr(linie, cuvant_cheie[i]) != NULL)
            return 1;
    }
    return 0;
}

int non_ascii(const char *linie) 
{
    while (*linie) 
    {
        if (!isascii(*linie))
            return 1;
        linie++;
    }
    return 0;
}

void mutare_fisier(const char *cale_fisier, const char *target_dir) 
{
    char cale_destinatie[MAX_PATH_LEN];
    snprintf(cale_destinatie, sizeof(cale_destinatie), "%s/%s", target_dir, basename((char *)cale_fisier));
    if (rename(cale_fisier, cale_destinatie) == 0) 
    {
        printf("%s a fost mutat in %s.\n", cale_fisier, target_dir);
    } 
    else 
    {
        perror("rename");
        printf("Esec la mutarea fisierului %s in directorul %s.\n", cale_fisier, target_dir);
    }
}

void analizare_fisier(const char *cale_fisier, const char *dir_izolat) 
{
    FILE *file = fopen(cale_fisier, "r");
    if (file == NULL) 
    {
        perror("fopen");
        return;
    }

    char linie[1024];
    int numar_linie = 0;
    int cuvant_cheie_gasit = 0;
    int nonascii_gasit = 0;

    while (fgets(linie, sizeof(linie), file) != NULL) 
    {
        numar_linie++;
        if (!cuvant_cheie_gasit && cuvant_cheie(linie)) 
        {
            printf("%s contine un cuvant cheie la linia %d.\n", cale_fisier, numar_linie);
            cuvant_cheie_gasit = 1;
        }
        if (!nonascii_gasit && non_ascii(linie)) 
        {
            printf("%s contine caractere non ascii la linia %d.\n", cale_fisier, numar_linie);
            nonascii_gasit = 1;
        }
        // se verifica doar daca unul dintre criterii este indeplinit
        if (cuvant_cheie_gasit || nonascii_gasit) 
        {
            break;
        }
    }

    fclose(file);

    // daca fisierul e corupt, se muta in directorul pt fisiere corupte
    if (cuvant_cheie_gasit || nonascii_gasit) 
    {
        mutare_fisier(cale_fisier, dir_izolat);
    } 
    else 
    {
        printf("%s este curat.\n", cale_fisier);
    }
}

void verificare_dir(const char *cale_dir, const char *dir_iesire, const char *dir_izolat) 
{
    DIR *dir;
    struct dirent *intrare;

    dir = opendir(cale_dir);
    if (dir == NULL) 
    {
        exit(EXIT_FAILURE);
    }

    while ((intrare = readdir(dir)) != NULL) 
    {
        if (strcmp(intrare->d_name, ".") == 0 || strcmp(intrare->d_name, "..") == 0)
            continue;

        char cale_fisier[MAX_PATH_LEN];
        snprintf(cale_fisier, sizeof(cale_fisier), "%s/%s", cale_dir, intrare->d_name);

        struct stat stat_fisier;
        if (stat(cale_fisier, &stat_fisier) == -1) 
        {
            perror("stat");
            continue;
        }

        if (S_ISDIR(stat_fisier.st_mode)) 
        {
            verificare_dir(cale_fisier, dir_iesire, dir_izolat);
        } 
        else 
        {
            // pentru permisiuniu
            if (!(stat_fisier.st_mode & S_IRUSR) || !(stat_fisier.st_mode & S_IWUSR) || !(stat_fisier.st_mode & S_IXUSR) ||
                !(stat_fisier.st_mode & S_IRGRP) || !(stat_fisier.st_mode & S_IWGRP) || !(stat_fisier.st_mode & S_IXGRP) ||
                !(stat_fisier.st_mode & S_IROTH) || !(stat_fisier.st_mode & S_IWOTH) || !(stat_fisier.st_mode & S_IXOTH)) 
                {
                printf("File %s has missing permissions: ", cale_fisier);
                if (!(stat_fisier.st_mode & S_IRUSR)) printf("User Read ");
                if (!(stat_fisier.st_mode & S_IWUSR)) printf("User Write ");
                if (!(stat_fisier.st_mode & S_IXUSR)) printf("User Execute ");
                if (!(stat_fisier.st_mode & S_IRGRP)) printf("Group Read ");
                if (!(stat_fisier.st_mode & S_IWGRP)) printf("Group Write ");
                if (!(stat_fisier.st_mode & S_IXGRP)) printf("Group Execute ");
                if (!(stat_fisier.st_mode & S_IROTH)) printf("Others Read ");
                if (!(stat_fisier.st_mode & S_IWOTH)) printf("Others Write ");
                if (!(stat_fisier.st_mode & S_IXOTH)) printf("Others Execute");
                printf("\n");

                // proces pentru analiza fisierului
                pid_t pid = fork();
                if (pid == -1) 
                {
                    perror("fork");
                    continue;
                } 
                else if (pid == 0) 
                {
                    // procesul copil
                    analizare_fisier(cale_fisier, dir_izolat);
                    exit(EXIT_SUCCESS);
                } 
                else 
                {
                    // procesul parinte
                    int status;
                    wait(&status); //asteapta dupa copil
                }
            }
        }
    }

    closedir(dir);
}

int main(int argc, char *argv[]) 
{
    if (argc < 5 || strcmp(argv[1], "-o") != 0 || strcmp(argv[3], "-s") != 0) 
    {
        printf("Usage: %s -o dir_iesire -s spatiu_dir_izolat dir1 dir2 dir3\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *dir_iesire = argv[2];
    const char *dir_izolat = argv[4];

    // se creeaza directorul pt fisiere izolate
    mkdir(dir_izolat, 0700);

    // se trece prin toate directoarele, prin toate subdirectoarele si se verifica fiecare fisier
    for (int i = 5; i < argc; i++) 
    {
        verificare_dir(argv[i], dir_iesire, dir_izolat);
    }

    return 0;
}

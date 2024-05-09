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
    const char *cuvinte_cheie[] = {"corrupted", "dangerous", "risk", "attack", "malware", "malicious"};
    const int num_cuvinte_cheie = sizeof(cuvinte_cheie) / sizeof(cuvinte_cheie[0]);
    for (int i = 0; i < num_cuvinte_cheie; i++) 
    {
        if (strstr(linie, cuvinte_cheie[i]) != NULL)
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

int mutare_fisier(const char *cale_fisier, const char *dir_destinatie) 
{
    char cale_destinatie[MAX_PATH_LEN];
    snprintf(cale_destinatie, sizeof(cale_destinatie), "%s/%s", dir_destinatie, basename((char *)cale_fisier));
    if (rename(cale_fisier, cale_destinatie) == 0) 
    {
        printf("Fisierul %s a fost mutat in directorul %s.\n", cale_fisier, dir_destinatie);
        return 1;
    } 
    else 
    {
        perror("rename");
        printf("Esec la mutarea fisierului %s in directorul %s.\n", cale_fisier, dir_destinatie);
        return 0;
    }
}

int analizare_fisier(const char *cale_fisier, const char *dir_izolat, int pipe_fd) 
{
    FILE *fisier = fopen(cale_fisier, "r");
    if (fisier == NULL) 
    {
        perror("fopen");
        return 0;
    }

    char linie[1024];
    int numar_linie = 0;
    int cuvant_cheie_gasit = 0;
    int nonascii_gasit = 0;

    while (fgets(linie, sizeof(linie), fisier) != NULL) 
    {
        numar_linie++;
        if (!cuvant_cheie_gasit && cuvant_cheie(linie)) 
        {
            printf("Fisierul %s contine un cuvant-cheie la linia %d.\n", cale_fisier, numar_linie);
            cuvant_cheie_gasit = 1;
            write(pipe_fd, "C", 1);
            fclose(fisier);
            return mutare_fisier(cale_fisier, dir_izolat);
        }
        if (!nonascii_gasit && non_ascii(linie)) 
        {
            printf("Fisierul %s contine caractere non-ASCII la linia %d.\n", cale_fisier, numar_linie);
            nonascii_gasit = 1;
            write(pipe_fd, "C", 1);
            fclose(fisier);
            return mutare_fisier(cale_fisier, dir_izolat);
        }
    }

    fclose(fisier);

    write(pipe_fd, "S", 1);

    return 0;
}

void verificare_dir(const char *cale_dir, const char *dir_iesire, const char *dir_izolat, int pipe_fd) 
{
    DIR *dir;
    struct dirent *intrare;

    dir = opendir(cale_dir);
    if (dir == NULL) 
    {
        perror("opendir");
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
            verificare_dir(cale_fisier, dir_iesire, dir_izolat, pipe_fd);
        } 
        else 
        {
            if (!(stat_fisier.st_mode & S_IRUSR) || !(stat_fisier.st_mode & S_IWUSR) || !(stat_fisier.st_mode & S_IXUSR) ||
                !(stat_fisier.st_mode & S_IRGRP) || !(stat_fisier.st_mode & S_IWGRP) || !(stat_fisier.st_mode & S_IXGRP) ||
                !(stat_fisier.st_mode & S_IROTH) || !(stat_fisier.st_mode & S_IWOTH) || !(stat_fisier.st_mode & S_IXOTH)) 
                {
                printf("Fisierul %s are permisiuni lipsa: ", cale_fisier);
                if (!(stat_fisier.st_mode & S_IRUSR)) printf("Citire utilizator ");
                if (!(stat_fisier.st_mode & S_IWUSR)) printf("Scriere utilizator ");
                if (!(stat_fisier.st_mode & S_IXUSR)) printf("Executie utilizator ");
                if (!(stat_fisier.st_mode & S_IRGRP)) printf("Citire grup ");
                if (!(stat_fisier.st_mode & S_IWGRP)) printf("Scriere grup ");
                if (!(stat_fisier.st_mode & S_IXGRP)) printf("Executie grup ");
                if (!(stat_fisier.st_mode & S_IROTH)) printf("Citire altii ");
                if (!(stat_fisier.st_mode & S_IWOTH)) printf("Scriere altii ");
                if (!(stat_fisier.st_mode & S_IXOTH)) printf("Executie altii");
                printf("\n");

                pid_t pid = fork();
                if (pid == -1) 
                {
                    perror("fork");
                    continue;
                } else if (pid == 0) 
                {
                    int rezultat = analizare_fisier(cale_fisier, dir_izolat, pipe_fd);
                    exit(rezultat);
                } 
                else 
                {
                    int status;
                    wait(&status);
                    if (WIFEXITED(status)) 
                    {
                        if (WEXITSTATUS(status) == 1) 
                        {
                            printf("Procesul copil a fost terminat cu PID-ul %d si a detectat 1 fisier potential periculos.\n", pid);
                        } 
                        else 
                        {
                            printf("Procesul copil a fost terminat cu PID-ul %d si a detectat 0 fisiere potential periculoase.\n", pid);
                        }
                    } 
                    else 
                    {
                        printf("Procesul copil a fost terminat cu PID-ul %d si a intampinat o eroare.\n", pid);
                    }
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
        printf("Utilizare: %s -o director_iesire -s director_izolare dir1 dir2 dir3\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *dir_iesire = argv[2];
    const char *dir_izolat = argv[4];

    mkdir(dir_izolat, 0700);

    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) 
    {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    int numar_fisiere_corupte_total = 0;

    for (int i = 5; i < argc; i++) 
    {
        verificare_dir(argv[i], dir_iesire, dir_izolat, pipe_fd[1]);
    }

    close(pipe_fd[1]);

    char c;
    while (read(pipe_fd[0], &c, 1) > 0) 
    {
        if (c == 'C') 
        {
            numar_fisiere_corupte_total++;
        }
    }

    close(pipe_fd[0]);

    printf("Total fisiere corupte gasite: %d\n", numar_fisiere_corupte_total);

    return 0;
}

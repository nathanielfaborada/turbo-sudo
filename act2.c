
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <omp.h>

#define DB_FILE "db-files3.bin"

typedef struct Profile {
    long id;
    char name[100];
    char course[100];
} Profile;

omp_lock_t lock;

void insertRecord() {
    omp_set_lock(&lock);

    FILE *f = fopen(DB_FILE, "ab+");   // append mode
    if (!f) {
        // File does not exist, create it without truncating if it exists
        f = fopen(DB_FILE, "a+b");
        if (!f) {
            printf("Error creating DB file!\n");
            omp_unset_lock(&lock);
            return;
        }
    }

    fseek(f, 0, SEEK_END);
    long bytes = ftell(f);
    long count = bytes / sizeof(Profile);
    long newId = count + 1;

    Profile p;
    p.id = newId;
    strcpy(p.name, "Juan");
    strcpy(p.course, "BSCS");

    fwrite(&p, sizeof(Profile), 1, f);
    fclose(f);

    printf("[Thread %d] INSERT → ID=%ld, Name=%s, Course=%s\n",
        omp_get_thread_num(), p.id, p.name, p.course);

    omp_unset_lock(&lock);
}


// ========================================================
// UPDATE FUNCTION: pick random record, modify fields
// ========================================================
void updateRecord() {
    omp_set_lock(&lock);

    FILE *f = fopen(DB_FILE, "r+b");
    if (!f) {
        omp_unset_lock(&lock);
        return;
    }

    fseek(f, 0, SEEK_END);
    long bytes = ftell(f);
    long count = bytes / sizeof(Profile);

    if (count == 0) {
        fclose(f);
        omp_unset_lock(&lock);
        return;
    }

    int target = rand() % count;

    Profile p;
    fseek(f, target * sizeof(Profile), SEEK_SET);
    fread(&p, sizeof(Profile), 1, f);

    // DEFAULT VALUES YOU WANT
    strcpy(p.name, "Lorem");
    strcpy(p.course, "BSIT");

    fseek(f, target * sizeof(Profile), SEEK_SET);
    fwrite(&p, sizeof(Profile), 1, f);

    printf("[Thread %d] UPDATE → index=%d → %s / %s\n",
        omp_get_thread_num(), target, p.name, p.course);

    fclose(f);
    omp_unset_lock(&lock);
}

int main() {
    srand(time(NULL));
    omp_init_lock(&lock);

    FILE *f = fopen(DB_FILE, "rb+");
    if (!f) {
        printf("DB file error!\n");
        return 1;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);   
    printf("DB size now = %ld bytes\n", size);
    fclose(f);

    double start = omp_get_wtime();

    #pragma omp parallel num_threads(100)
    {
        insertRecord();
        updateRecord();
    }

    double end = omp_get_wtime();

    printf("\nElapsed = %.5f seconds\n\n", end - start);

    FILE *db = fopen(DB_FILE, "rb");
    if (!db) {
        printf("Cannot open DB for reading!\n");
        return 1;
    }

    Profile p;
    int index = 0;

    printf("=== FINAL DATABASE CONTENTS ===\n");
    while (fread(&p, sizeof(Profile), 1, db) > 0) {
        printf("[%d] ID=%ld, Name=%s, Course=%s\n",
            index, p.id, p.name, p.course);
        index++;
    }

    fclose(db);
    omp_destroy_lock(&lock);

    return 0;
}


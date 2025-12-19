#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <omp.h>

#define DB_FILE "db-files.bin"

// REQUIREMENTS
#define NUM_INSERTS 500

// Profile Structure
typedef struct Profile {
    long id;
    char name[100];
    char course[100];
} Profile;

// GLOBALS
omp_lock_t file_lock;               // lock for appending inserts
omp_lock_t record_locks[1000];      // per-record locks (record-level locking)
long total_records = 0;

// ------------------------------------------------------------
// INSERT: thread-safe, auto-increment primary key
// ------------------------------------------------------------
void insertRecord() {
    omp_set_lock(&file_lock);

    FILE *f = fopen(DB_FILE, "ab+");   // append only
    if (!f) {
        printf("Error opening DB for insert!\n");
        omp_unset_lock(&file_lock);
        return;
    }

    long newId = total_records + 1;   // safe because file_lock protects it

    Profile p;
    p.id = newId;
    strcpy(p.name, "Juan");
    strcpy(p.course, "BSCS");

    fwrite(&p, sizeof(Profile), 1, f);
    fclose(f);

    printf("[Thread %d] INSERT → ID=%ld (%s, %s)\n",
        omp_get_thread_num(), p.id, p.name, p.course);

    // initialize record lock for this new record
    omp_init_lock(&record_locks[newId - 1]);

    total_records++;

    omp_unset_lock(&file_lock);
}

// ------------------------------------------------------------
// UPDATE: choose random record, lock only that record
// ------------------------------------------------------------
void updateRecord() {
    if (total_records == 0) return;

    int target = rand() % total_records;

    // lock ONLY the target record
    omp_set_lock(&record_locks[target]);

    FILE *f = fopen(DB_FILE, "r+b");
    if (!f) {
        omp_unset_lock(&record_locks[target]);
        return;
    }

    Profile p;
    fseek(f, target * sizeof(Profile), SEEK_SET);
    fread(&p, sizeof(Profile), 1, f);

    strcpy(p.name, "Lorem");
    strcpy(p.course, "BSIT");

    fseek(f, target * sizeof(Profile), SEEK_SET);
    fwrite(&p, sizeof(Profile), 1, f);

    printf("[Thread %d] UPDATE → index=%d → %s / %s\n",
        omp_get_thread_num(), target, p.name, p.course);

    fclose(f);
    omp_unset_lock(&record_locks[target]);
}

// ------------------------------------------------------------
// READ: lock only the target record (safe consistent read)
// ------------------------------------------------------------
void readRecord() {
    if (total_records == 0) return;

    int target = rand() % total_records;

    omp_set_lock(&record_locks[target]);

    FILE *f = fopen(DB_FILE, "rb");
    if (!f) {
        omp_unset_lock(&record_locks[target]);
        return;
    }

    Profile p;
    fseek(f, target * sizeof(Profile), SEEK_SET);
    fread(&p, sizeof(Profile), 1, f);

    printf("[Thread %d] READ → index=%d → %ld, %s, %s\n",
        omp_get_thread_num(), target, p.id, p.name, p.course);

    fclose(f);
    omp_unset_lock(&record_locks[target]);
}

// ------------------------------------------------------------
// MAIN
// ------------------------------------------------------------
int main() {
    srand(time(NULL));

    omp_init_lock(&file_lock);

    printf("Initializing record locks...\n");
    for (int i = 0; i < 1000; i++)
        omp_init_lock(&record_locks[i]);

    double start = omp_get_wtime();

    #pragma omp parallel for num_threads(NUM_INSERTS)
    for (int i = 0; i < NUM_INSERTS; i++)
    {
        insertRecord();
        updateRecord();
        readRecord();
    }
    

    
    double end = omp_get_wtime();

    printf("\nElapsed = %.5f seconds\n\n", end - start);

    // ------------------------
    // DISPLAY FINAL CONTENT
    // ------------------------
    FILE *db = fopen(DB_FILE, "rb");
    if (!db) {
        printf("Error opening DB.\n");
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
    omp_destroy_lock(&file_lock);

    return 0;
}

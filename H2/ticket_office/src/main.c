#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <signal.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "functions.c"

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

typedef struct {
    int total;
    int available;
    int purchased;
    int transactions;
} TicketShared;

static const char *DEFAULT_SHM_NAME = "/ticket_shm";
static const char *DEFAULT_SEM_NAME = "/ticket_sem";

static int shm_fd = -1;
static TicketShared *mem = NULL;
static sem_t *sem = SEM_FAILED;
static char shm_name_buf[128];
static char sem_name_buf[128];

static void Cleanup(void) {
    if (mem && mem != MAP_FAILED) {
        munmap(mem, sizeof(*mem));
        mem = NULL;
    }
    if (shm_fd >= 0) {
        close(shm_fd);
        shm_fd = -1;
    }
    if (sem && sem != SEM_FAILED) {
        sem_close(sem);
        sem = SEM_FAILED;
    }
    // Unlink resources we created
    if (shm_name_buf[0]) shm_unlink(shm_name_buf);
    if (sem_name_buf[0]) sem_unlink(sem_name_buf);
}

static void OnSigint(int sig) {
    (void)sig;
    // Print a newline for clean prompt return
    write(STDOUT_FILENO, "\n", 1);
    Cleanup();
    _Exit(0);
}


int main(int argc, char *argv[]) {

    // If no argument, default to 20 tickets (for test compatibility)
    int total = 20;
    if (argc >= 2) {
        char *end = NULL;
        long total_in = strtol(argv[1], &end, 10);
        if (!end || *end != '\0' || total_in < 0 || total_in > 100000000) {
            fprintf(stderr, "Invalid <total_tickets>: %s\n", argv[1]);
            return 1;
        }
        total = (int)total_in;
    }

    const char *shm_name = (argc >= 3) ? argv[2] : DEFAULT_SHM_NAME;
    const char *sem_name = (argc >= 4) ? argv[3] : DEFAULT_SEM_NAME;

    // Keep copies for unlink during cleanup
    snprintf(shm_name_buf, sizeof(shm_name_buf), "%s", shm_name);
    snprintf(sem_name_buf, sizeof(sem_name_buf), "%s", sem_name);

    // Handle Ctrl+C cleanly
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = OnSigint;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    // Create shared memory (fresh init)
    // O_CREAT|O_EXCL ensures we fail if it already exists (so we don't clobber a running system).
    // If you prefer to reuse, change to O_CREAT and call ftruncate unconditionally.
    shm_fd = shm_open(shm_name, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (shm_fd == -1) {
        perror("shm_open");
        fprintf(stderr, "If it already exists, try removing it: shm_unlink(\"%s\")\n", shm_name);
        return 1;
    }
    if (ftruncate(shm_fd, (off_t)sizeof(TicketShared)) == -1) {
        perror("ftruncate");
        Cleanup();
        return 1;
    }
    mem = (TicketShared *)mmap(NULL, sizeof(TicketShared), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (mem == MAP_FAILED) {
        perror("mmap");
        Cleanup();
        return 1;
    }

    // Create a named semaphore (value=1 acts like a mutex)
    sem = sem_open(sem_name, O_CREAT | O_EXCL, 0600, 1);
    if (sem == SEM_FAILED) {
        perror("sem_open");
        Cleanup();
        return 1;
    }

    // Initialize shared counters
    if (sem_wait(sem) == -1) {
        perror("sem_wait");
        Cleanup();
        return 1;
    }
    mem->total        = total;
    mem->available    = total;
    mem->purchased    = 0;
    mem->transactions = 0;
    if (sem_post(sem) == -1) {
        perror("sem_post");
        Cleanup();
        return 1;
    }

    // Reporting loop (every 1 second)
    while (1) {
        int purchased, available, transactions;

        if (sem_wait(sem) == -1) {
            if (errno == EINTR) continue;
            perror("sem_wait");
            break;
        }
        purchased    = mem->purchased;
        available    = mem->available;
        transactions = mem->transactions;
        if (sem_post(sem) == -1) {
            perror("sem_post");
            break;
        }

        // Print exactly as required (with a blank line between each line, except after the last)
        printf("TICKET REPORT:\n");
        printf("Purchased tickets: %d\n", purchased);
        printf("Available tickets: %d\n", available);
        printf("Transactions: %d\n", transactions);

        if (available <= 0) {
            printf("SOLD OUT...\n");
            fflush(stdout);
            break;
        }

        fflush(stdout);
        sleep(1);
    }

    Cleanup();
    return 0;
}
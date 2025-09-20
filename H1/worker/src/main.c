#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdbool.h>
#include <signal.h>

void SigUser1Handler(int sig) {
    FILE *log_file_1;
    char name[64];
    snprintf(name, sizeof(name), "worker_log_%d.txt", getpid());

    log_file_1 = fopen(name, "a+");

    fprintf(log_file_1, "SIGUSR1 received\n");
    fclose(log_file_1);
}

void SigUser2Handler(int sig) {
    FILE *log_file_1;
    char name[64];
    snprintf(name, sizeof(name), "worker_log_%d.txt", getpid());

    log_file_1 = fopen(name, "a+");

    fprintf(log_file_1, "SIGUSR2 received\n");
    fclose(log_file_1);
}

int main() {
    signal(SIGUSR1, SigUser1Handler);
    signal(SIGUSR2, SigUser2Handler);
    while (true) {
        pause();
    }
    return 0;
}
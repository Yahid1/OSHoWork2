#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <stdint.h>

static int ParseSignal(const char *s) {
    if (strcmp(s, "SIGUSR1") == 0) return SIGUSR1;
    if (strcmp(s, "SIGUSR2") == 0) return SIGUSR2;
    return -1;
}

static pid_t ParsePid(const char *s, const char *which) {
    char *end = NULL;
    int64_t v = strtol(s, &end, 10);
    if (!s[0] || *end != '\0' || v <= 0) {
        fprintf(stderr, "manager: invalid %s PID: '%s'\n", which, s);
        exit(EXIT_FAILURE);
    }
    return (pid_t)v;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <worker1_pid> <worker2_pid>\n", argv[0]);
        return EXIT_FAILURE;
    }
    pid_t worker1_pid = ParsePid(argv[1], "worker1");
    pid_t worker2_pid = ParsePid(argv[2], "worker2");


    FILE *fptr;
    char buffer[256];

    // Open the file in read mode
    fptr = fopen("./commands.txt", "r");

    // Check for errors
    if (fptr == NULL) {
        printf("Error: could not open file example.txt\n");
        return 1;
    }
    int lineno = 0;
    // Read and print each line until the end of the file
    while (fgets(buffer, sizeof(buffer), fptr) != NULL) {
        lineno++;

        // Trim leading whitespace
        char *p = buffer;
        while (*p == ' ' || *p == '\t') p++;

        // Skip blank lines and comments
        if (*p == '\0' || *p == '\n' || *p == '#')
            continue;

        // Parse: <worker1|worker2> <SIGUSR1|SIGUSR2>
        char worker[32], sigstr[32];
        if (sscanf(p, "%31s %31s", worker, sigstr) != 2) {
            fprintf(stderr, "manager: line %d: bad entry: %s", lineno, buffer);
            continue;
        }

        pid_t target_pid;
        if (strcmp(worker, "worker1") == 0) {
            target_pid = worker1_pid;
        } else if (strcmp(worker, "worker2") == 0) {
            target_pid = worker2_pid;
        } else {
            fprintf(stderr, "line %d: unknown '%s'\n", lineno, worker);
            continue;
        }

        int sig = ParseSignal(sigstr);
        if (sig == -1) {
            fprintf(stderr, "line %d: unknown signal '%s'\n", lineno, sigstr);
            continue;
        }

        // Wait 1 second before sending each signal
        sleep(1);

        if (kill(target_pid, sig) == -1) {
            fprintf(stderr, "manager: ERROR sending %s to PID %ld: %s\n",
                    sigstr, (int64_t)target_pid, strerror(errno));
        } else {
            fprintf(stderr, "sent %s to", sigstr);
            fprintf(stderr, " PID %ld\n", (int64_t)target_pid);
        }
    }

    fclose(fptr);
    sleep(1);

    kill(worker1_pid, SIGTERM);
    kill(worker2_pid, SIGTERM);

    return 0;
}
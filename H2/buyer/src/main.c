#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <semaphore.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "functions.c"

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
static sem_t *sem_lock = SEM_FAILED;


static void cleanup(void) {
    if (mem && mem != MAP_FAILED) {
        munmap(mem, sizeof(*mem));
        mem = NULL;
    }
    if (shm_fd >= 0) {
        close(shm_fd);
        shm_fd = -1;
    }
    if (sem_lock && sem_lock != SEM_FAILED) {
        sem_close(sem_lock);
        sem_lock = SEM_FAILED;
    }
}

static void on_sigint(int sig) {
    (void)sig;
    write(STDOUT_FILENO, "\n", 1);
    cleanup();
    _Exit(0);
}

// 1) AttachMemory: adjunta a la memoria compartida y al semáforo ya creados
static bool AttachMemory(const char *shm_name, const char *sem_name) {
    shm_fd = shm_open(shm_name, O_RDWR, 0);
    if (shm_fd == -1) {
        perror("shm_open (¿ejecutaste ticket_office primero?)");
        return false;
    }
    mem = (TicketShared *)mmap(NULL, sizeof(TicketShared),
                               PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (mem == MAP_FAILED) {
        perror("mmap");
        mem = NULL;
        return false;
    }
    sem_lock = sem_open(sem_name, 0);
    if (sem_lock == SEM_FAILED) {
        perror("sem_open");
        return false;
    }
    return true;
}

// 2) BuyTickets: aplica reglas de negocio y actualiza memoria compartida.
//    Devuelve los boletos vendidos en esta operación (0 si inválido o agotado).
static int BuyTickets(int requested) {
    // Regla 3: inválido si <= 0 (no cuenta transacción)
    if (requested <= 0) {
        printf("Try again, invalid number of tickets\n");
        fflush(stdout);
        return 0;
    }

    // Regla 1: máximo 5 por compra
    if (requested > 5) requested = 5;

    if (sem_wait(sem_lock) == -1) {
        if (errno == EINTR) return 0;
        perror("sem_wait");
        return 0;
    }

    int available = mem->available;
    int sold = 0;

    if (available > 0) {
        // Regla 2: no vender más de lo disponible
        sold = (requested <= available) ? requested : available;

        mem->available    -= sold;
        mem->purchased    += sold;
        mem->transactions += 1;     // solo contamos transacciones válidas (>0 vendidos)
        available = mem->available; // actualizado
    }

    if (sem_post(sem_lock) == -1) {
        perror("sem_post");
    }

    // Mensaje de resultado de la compra (exacto al formato de ejemplo)
    printf("Company purchased %d tickets. Available: %d\n\n", sold, available);
    fflush(stdout);

    return sold;
}

int main(int argc, char *argv[]) {
    const char *shm_name = (argc >= 2) ? argv[1] : DEFAULT_SHM_NAME;
    const char *sem_name = (argc >= 3) ? argv[2] : DEFAULT_SEM_NAME;

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_sigint;
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    if (!AttachMemory(shm_name, sem_name)) {
        return 1;
    }

    // Bucle de compra hasta que no queden boletos
    char line[128];
    while (1) {
        // Revisar si ya no hay disponibles antes de preguntar
        if (sem_wait(sem_lock) == -1 && errno != EINTR) {
            perror("sem_wait");
            break;
        }
        int available = mem->available;
        if (sem_post(sem_lock) == -1) perror("sem_post");
        if (available <= 0) {
            // ticket_office imprimirá "SOLD OUT..."
            break;
        }

        // Pedir al usuario
        // printf("//Buyer try to buy ");
        fflush(stdout);

        // Leemos línea; si EOF, salimos limpiamente
        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }

        // Intentar extraer el número al vuelo para eco del prompt
        char *endptr = NULL;
        long req = strtol(line, &endptr, 10);
        // Imprimir el número que el usuario intentó comprar (como en el ejemplo)
        // Si la línea no tenía número válido, mostramos lo que el usuario escribió (limpio)
        // pero igual vamos a tratarlos como inválidos abajo.
        // Para ajustarnos al ejemplo, imprimimos el número parseado si hay.
        if (endptr != line) {
            // teníamos un número
            printf("%ld\n", req);
        } else {
            // no se parseó número: mostramos la línea tal cual (sin salto extra)
            size_t len = strcspn(line, "\r\n");
            fwrite(line, 1, len, stdout);
            printf("\n");
        }

        // Ejecutar compra
        (void)BuyTickets((int)req);

        // Esperar 2 segundos (simular procesamiento)
        sleep(2);
    }

    cleanup();
    return 0;
}
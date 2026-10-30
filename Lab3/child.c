#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>

#define SHM_SIZE 4096

int is_vowel(char c) {
    return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u' || c == 'y' ||
           c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U' || c == 'Y';
}

int main(int argc, char *argv[]) {
    if (argc < 4)
        return 1;

    char *shm_name = argv[1];
    char *sem_name = argv[2];
    char *out_file = argv[3];

    int shm_fd = shm_open(shm_name, O_RDWR, 0);
    if (shm_fd == -1) {
        write(2, "child shm_open failed\n", 22);
        exit(1);
    }
    char *shm = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    close(shm_fd);

    sem_t *sem = sem_open(sem_name, O_RDWR);
    if (sem == SEM_FAILED) {
        write(2, "child sem_open failed\n", 22);
        exit(1);
    }

    int out_fd = open(out_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out_fd == -1) {
        write(2, "child open out failed\n", 22);
        _exit(1);
    }

    int running = 1;
    while (running) {
        sem_wait(sem);

        uint32_t *length = (uint32_t *)shm;
        char *text = shm + sizeof(uint32_t);

        if (*length == UINT32_MAX) {
            running = 0;
        } else if (*length > 0) {
            char result[SHM_SIZE];
            int j = 0;
            for (int i = 0; i < *length; i++) {
                if (!is_vowel(text[i]))
                    result[j++] = text[i];
            }
            write(out_fd, result, j);
            *length = 0;
        }
        sem_post(sem);
    }

    close(out_fd);
    sem_close(sem);
    munmap(shm, SHM_SIZE);
    return 0;
}
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <string.h>
#include <time.h>

#define SHM_SIZE 4096

int main(int argc, char *argv[]) {
    if (argc < 4) {
        write(2, "Usage: ./parent <input.txt> <out1.txt> <out2.txt>\n", 51);
        return 1;
    }

    char shm_name1[64];
    char shm_name2[64];
    char sem_name1[64];
    char sem_name2[64];

    snprintf(shm_name1, sizeof(shm_name1), "/shm1_%d", getpid());
    snprintf(shm_name2, sizeof(shm_name2), "/shm2_%d", getpid());
    snprintf(sem_name1, sizeof(sem_name1), "/sem1_%d", getpid());
    snprintf(sem_name2, sizeof(sem_name2), "/sem2_%d", getpid());

    int shm_fd1 = shm_open(shm_name1, O_RDWR | O_CREAT | O_TRUNC, 0600);
    if (shm_fd1 == -1) {
        write(2, "shm_open 1 failed\n", 18);
        return 1;
    }
    ftruncate(shm_fd1, SHM_SIZE);
    char *shm1 = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd1, 0);
    if (shm1 == MAP_FAILED) {
        write(2, "mmap 1 failed\n", 14);
        return 1;
    }

    int shm_fd2 = shm_open(shm_name2, O_RDWR | O_CREAT | O_TRUNC, 0600);
    if (shm_fd2 == -1) {
        write(2, "shm_open 2 failed\n", 18);
        return 1;
    }
    ftruncate(shm_fd2, SHM_SIZE);
    char *shm2 = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd2, 0);
    if (shm2 == MAP_FAILED) {
        write(2, "mmap 2 failed\n", 14);
        return 1;
    }

    sem_t *sem1 = sem_open(sem_name1, O_RDWR | O_CREAT | O_TRUNC, 0600, 1);
    sem_t *sem2 = sem_open(sem_name2, O_RDWR | O_CREAT | O_TRUNC, 0600, 1);

    pid_t pid1 = fork();
    if (pid1 == 0) {
        char *args[] = {"./child", shm_name1, sem_name1, argv[2], NULL};
        execv("./child", args);
        write(2, "exec child1 failed\n", 19);
        _exit(1);
    }

    pid_t pid2 = fork();
    if(pid2 == 0) {
        char *args[] = {"./child", shm_name2, sem_name2, argv[3], NULL};
        execv("./child", args);
        write(2, "exec child2 failed\n", 19);
        _exit(1);
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        write(2, "open input failed\n", 18);
        return 1;
    }

    srand(time(NULL));

    char line[SHM_SIZE - sizeof(uint32_t)];
    int line_pos = 0;
    char c;

    while (read(fd, &c, 1) == 1) {
        line[line_pos++] = c;

        if (c == '\n' || line_pos == (int)sizeof(line) - 1) {
            line[line_pos] = '\0';
            int len = line_pos;

            if (rand() % 100 < 80) {
                sem_wait(sem1);
                *(uint32_t *)shm1 = len;
                memcpy(shm1 + sizeof(uint32_t), line, len);
                sem_post(sem1);
            } else {
                sem_wait(sem2);
                *(uint32_t *)shm2 = len;
                memcpy(shm2 + sizeof(uint32_t), line, len);
                sem_post(sem2);
            }
            line_pos = 0;
        }
    }
    
    if (line_pos > 0) {
        line[line_pos] = '\0';
        int len = line_pos;
        if (rand() % 100 < 80) {
            sem_wait(sem1);
            *(uint32_t *)shm1 = len;
            memcpy(shm1 + sizeof(uint32_t), line, len);
            sem_post(sem1);
        } else {
            sem_wait(sem2);
            *(uint32_t *)shm2 = len;
            memcpy(shm2 + sizeof(uint32_t), line, len);
            sem_post(sem2);
        }
    }
    close(fd);

     sem_wait(sem1);
     *(uint32_t *)shm1 = UINT32_MAX;
     sem_post(sem1);

     sem_wait(sem2);
     *(uint32_t *)shm2 = UINT32_MAX;
     sem_post(sem2);

     waitpid(pid1, NULL, 0);
     waitpid(pid2, NULL, 0);

     munmap(shm1, SHM_SIZE);
     munmap(shm2, SHM_SIZE);
     close(shm_fd1);
     close(shm_fd2);

     shm_unlink(shm_name1);
     shm_unlink(shm_name2);

     sem_close(sem1);
     sem_close(sem2);
     sem_unlink(sem_name1);
     sem_unlink(sem_name2);

     return 0;
}
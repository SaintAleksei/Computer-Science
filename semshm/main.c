#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#define ERR(condition)\
    do\
    {\
        if (condition)\
        {\
            fprintf (stderr, "ERROR: %s:%d:%s: %s\n", __FILE__, __LINE__, __PRETTY_FUNCTION__, strerror (errno) );\
            exit (EXIT_FAILURE);\
        }\
    }\
    while (0)\

#define SEM_KEY 0x777
#define SHM_KEY 0x666

enum SEM_NUMBERS
{
    SEM_SENDER,
    SEM_RECEIVER,
    SEM_MUTEX,
    SEM_FULL,
    SEM_EMPTY,
    #define SEM_COUNT SEM_EMPTY + 1
};

struct buffer
{
    ssize_t size;
    uint8_t data[BUFSIZ];
} *buff;

struct sembuf sops[SEM_COUNT] = {};

const struct timespec timeout = {.tv_sec = 3};

int semid;
int shmid;

int main (int argc, char **argv)
{
    if (argc > 2)
    {
        fprintf (stderr, "Usage: %s <name_of_file> - to run sender, %s - to run receiver\n", argv[0], argv[0]);

        exit (EXIT_FAILURE);
    } 

    semid = semget (SEM_KEY, SEM_COUNT, IPC_CREAT | 0660);
    ERR (semid == -1);

    shmid = shmget (SHM_KEY, sizeof (*buff), IPC_CREAT | 0660);
    ERR (shmid == -1);

    if (argc == 2) /* sender */
    {
        sops[0].sem_num = SEM_SENDER;
        sops[0].sem_op  = 0;
        sops[0].sem_flg = 0;
        sops[1].sem_num = SEM_SENDER;
        sops[1].sem_op  = 1;
        sops[1].sem_flg = SEM_UNDO;
        ERR (semop (semid, sops, 2) == -1);

        int fd = open (argv[1], O_RDONLY);
        ERR (fd == -1);

        uint8_t stop = 0;

        while (!stop)
        {
            sops[0].sem_num = SEM_FULL;
            sops[0].sem_op  = 0;
            sops[0].sem_flg = 0;
            sops[1].sem_num = SEM_FULL;
            sops[1].sem_op  = 1;
            sops[1].sem_flg = 0;
            sops[2].sem_num = SEM_MUTEX;
            sops[2].sem_op  = 0;
            sops[2].sem_flg = 0;
            sops[3].sem_num = SEM_MUTEX;
            sops[3].sem_op  = 1;
            sops[3].sem_flg = SEM_UNDO;
            ERR (semtimedop (semid, sops, 4, &timeout) == -1);

            buff = shmat (shmid, NULL, 0);
            ERR (buff == (void *) -1);

            memset (buff->data, 0, BUFSIZ);


            exit (0);
            buff->size = read (fd, buff->data, BUFSIZ);
            ERR (buff->size < 0);

            if (buff->size == 0)
                stop = 1;

            ERR (shmdt (buff) );

            sops[0].sem_num = SEM_MUTEX;
            sops[0].sem_op  = -1;
            sops[0].sem_flg = SEM_UNDO | IPC_NOWAIT;
            sops[1].sem_num = SEM_EMPTY;
            sops[1].sem_op  = 1;
            sops[1].sem_flg = 0;
            ERR (semop (semid, sops, 2) == -1);
        }

        sops[0].sem_num = SEM_SENDER;
        sops[0].sem_op  = -1;
        sops[0].sem_flg = SEM_UNDO | IPC_NOWAIT;
        ERR (semop (semid, sops, 1) == -1);

        exit (EXIT_SUCCESS);
    }
    else /* receiver */
    {
        sops[0].sem_num = SEM_RECEIVER;
        sops[0].sem_op  = 0;
        sops[0].sem_flg = 0;
        sops[1].sem_num = SEM_RECEIVER;
        sops[1].sem_op  = 1;
        sops[1].sem_flg = SEM_UNDO;
        ERR (semop (semid, sops, 2) == -1);

        uint8_t stop = 0;

        while (!stop)
        {
            sops[0].sem_num = SEM_EMPTY;
            sops[0].sem_op  = -1;
            sops[0].sem_flg = 0;
            sops[1].sem_num = SEM_MUTEX;
            sops[1].sem_op  = 0;
            sops[1].sem_flg = 0;
            sops[2].sem_num = SEM_MUTEX;
            sops[2].sem_op  = 1;
            sops[2].sem_flg = SEM_UNDO;
            ERR (semtimedop (semid, sops, 3, &timeout) == -1);

            buff = shmat (shmid, NULL, 0);
            ERR (buff == (void *) -1);

            if (buff->size <= 0)
                stop = 1;
            else
                ERR(write (STDOUT_FILENO, buff->data, buff->size) == -1);

            ERR(shmdt (buff) );

            sops[0].sem_num = SEM_MUTEX;
            sops[0].sem_op  = -1;
            sops[0].sem_flg = SEM_UNDO | IPC_NOWAIT;
            sops[1].sem_num = SEM_FULL;
            sops[1].sem_op  = -1;
            sops[1].sem_flg = IPC_NOWAIT;
            ERR (semop (semid, sops, 2) == -1);
        }

        sops[0].sem_num = SEM_RECEIVER;
        sops[0].sem_op  = -1;
        sops[0].sem_flg = SEM_UNDO | IPC_NOWAIT;
        ERR (semop (semid, sops, 1) == -1);
    } 
}

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#define ERR(condition)\
    if (condition)\
    {\
        fprintf (stderr, "%s:%d: ERROR: %s\n", __FILE__, __LINE__, strerror (errno) );\
        exit (EXIT_FAILURE);\
    }

#define KEY 0x666
#define BUFSIZE 0x1000

#define SEMCOUNT 4
#define SEM_SENDER 0
#define SEM_RECIEVER 1
#define SEM_SHM 2
#define SEM_SENTCOUNT 3

#define SETSEMOP(i, sem_num, sem_op, sem_flg)\
    sops[i].sem_num = sem_num;\
    sops[i].sem_op  = sem_op;\
    sops[i].sem_flg = sem_flg;

typedef struct packet
{
    size_t nbytes;
    char buff[BUFSIZE];
} packet_t;

int main (int argc, char **argv)
{
    if (argc != 2)
    {
        fprintf (stderr, "Usage: %s <name_of_file>\n", argv[0]);
        exit (EXIT_FAILURE);
    }    

    struct sembuf sops[4] = {};

    int semid = semget (KEY, SEMCOUNT, IPC_CREAT | 0660);
    ERR (semid == -1);

    int shmid = shmget (KEY, sizeof (packet_t), IPC_CREAT | 0660);
    ERR (shmid == -1);

    int fd = open (argv[1], O_RDONLY);
    ERR (fd == -1);

    sops[0].sem_num = SEM_SENDER;
    sops[0].sem_op  = 0;
    sops[0].sem_flg = 0;
    sops[1].sem_num = SEM_SENDER;
    sops[1].sem_op  = 1;
    sops[1].sem_flg = SEM_UNDO;
    ERR (semop (semid, sops, 2) == -1);

    uint8_t stop = 0;
    while (!stop)
    {
        sops[0].sem_num = SEM_SHM;
        sops[0].sem_op  = 0;
        sops[0].sem_flg = 0;
        sops[1].sem_num = SEM_SHM;
        sops[1].sem_op  = 1;
        sops[1].sem_flg = SEM_UNDO;
        sops[2].sem_num = SEM_SENTCOUNT;
        sops[2].sem_op  = 0;
        sops[2].sem_flg = 0;
        ERR (semop (semid, sops, 3) == -1);

        packet_t *ptr = shmat (shmid, NULL, 0);
        ERR ( ptr == (void *) -1);

        ptr->nbytes = read (fd, ptr->buff, BUFSIZE);

        ERR (ptr->nbytes == (size_t) -1);

        if (!ptr->nbytes)
            stop = 1;

        ERR (shmdt (ptr) == -1);

        sops[0].sem_num = SEM_SHM;
        sops[0].sem_op  = -1;
        sops[0].sem_flg = 0;
        sops[1].sem_num = SEM_SENTCOUNT;
        sops[1].sem_op  = 1;
        sops[1].sem_flg = 0;
        ERR (semop (semid, sops, 2) == -1);
    }

    exit (EXIT_SUCCESS);
}

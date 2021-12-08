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
#include <assert.h>

#define ERR(condition)\
    do\
    {\
        if (condition)\
        {\
            fprintf (stderr, "ERROR: %s:%d:%s: %s\n", __FILE__, __LINE__,\
                     __PRETTY_FUNCTION__, strerror (errno) );\
            exit (EXIT_FAILURE);\
        }\
    }\
    while (0)\

#define SEM_KEY 0x777
#define SHM_KEY 0x666
#define NOPS    0x10

enum SEM_NUMBERS
{
    SEM_SENDER,
    SEM_RECEIVER,
    SEM_CONNECTION,
    SEM_MUTEX,
    SEM_FULL,
    SEM_EMPTY,
    #define SEM_COUNT SEM_EMPTY + 1
};

const struct timespec timeout = {.tv_sec = 60};

struct semop
{
    struct sembuf sops[NOPS];
    size_t nops;
} semops;

void semops_add (enum SEM_NUMBERS, short op, short flags);
int  semops_do  ();

int main (int argc, char **argv)
{
    if (argc > 2)
    {
        fprintf (stderr, "Usage: %s <name_of_file> - to run sender, "
                         "%s - to run receiver\n", argv[0], argv[0]);

        exit (EXIT_FAILURE);
    } 

    struct buffer
    {
        ssize_t size;
        uint8_t data[BUFSIZ];
    } *buff = NULL;

    int semid = semget (SEM_KEY, SEM_COUNT, IPC_CREAT | 0660);
    ERR (semid == -1);

    int shmid = shmget (SHM_KEY, sizeof (*buff), IPC_CREAT | 0660);
    ERR (shmid == -1);

    if (argc == 2) /* sender */
    {
        semops_add (SEM_SENDER, 0, 0);
        semops_add (SEM_SENDER, 1, SEM_UNDO); 
        ERR (semops_do (semid) == -1);

        /* 
           critical section #1 start:
           processes - senders vs receivers
           resources - shared memory && semaphores
        */

        semops_add (SEM_CONNECTION, 0, 0);
        ERR (semops_do (semid) == -1);

        ERR (semctl (semid, SEM_FULL, SETVAL, 0) == -1); 
        ERR (semctl (semid, SEM_EMPTY, SETVAL, 0) == -1); 

        semops_add (SEM_CONNECTION, 2, SEM_UNDO);
        ERR (semops_do (semid) == -1);

        semops_add (SEM_CONNECTION, -1, SEM_UNDO);
        semops_add (SEM_CONNECTION, -2, 0);
        semops_add (SEM_CONNECTION,  2, 0);
        ERR (semops_do (semid) == -1);

        int fd = open (argv[1], O_RDONLY);
        ERR (fd == -1);

        uint8_t stop = 0;

        while (!stop)
        {
            /* SEM_EMPTY == 0 && SEM_FULL == 0 */

            semops_add (SEM_FULL, 0, 0);
            semops_add (SEM_FULL, 1, SEM_UNDO);
            semops_add (SEM_MUTEX, 0, 0);
            semops_add (SEM_MUTEX, 1, SEM_UNDO);
            ERR (semops_do (semid) == -1);
    
            /* 
               critical section #3 start 
               processes - sender vs receiver
               resource - shared memory
            */

            buff = shmat (shmid, NULL, 0);
            ERR (buff == (void *) -1);

            buff->size = read (fd, buff->data, BUFSIZ);
            ERR (buff->size < 0);

            if (buff->size == 0)
                stop = 1;

            ERR (shmdt (buff) );

            /* critical section #3 end */

            semops_add (SEM_MUTEX, -1, SEM_UNDO | IPC_NOWAIT);
            semops_add (SEM_EMPTY, 1, 0);
            semops_add (SEM_FULL, -1, SEM_UNDO | IPC_NOWAIT);
            semops_add (SEM_FULL,  1, 0);
            ERR (semops_do (semid) == -1); 

            /* SEM_EMPTY == 1 && SEM_FULL == 1 */
        }

        /* critical section #1 end */

        exit (EXIT_SUCCESS);
    }
    else /* receiver */
    {
        semops_add (SEM_RECEIVER, 0, 0);
        semops_add (SEM_RECEIVER, 1, SEM_UNDO);
        ERR (semops_do (semid) == -1);

        /* 
           critical section #1 start
           processes - receivers
           resources - shared memory && semaphores
        */

        semops_add (SEM_CONNECTION, -2, 0);
        semops_add (SEM_CONNECTION,  2, 0);
        semops_add (SEM_CONNECTION,  1, SEM_UNDO);
        ERR (semops_do (semid) == -1);

        uint8_t stop = 0;

        while (!stop)
        {
            /* SEM_FULL == 1 && SEM_EMPTY == 1 */

            semops_add (SEM_EMPTY, -1, SEM_UNDO);
            semops_add (SEM_MUTEX, 0, 0);
            semops_add (SEM_MUTEX, 1, SEM_UNDO);
            ERR (semops_do (semid) == -1);

            /* 
               critical section #4 start 
               processes - sender vs receiver
               resource - shared memory
            */

            buff = shmat (shmid, NULL, 0);
            ERR (buff == (void *) -1);

            if (buff->size == 0)
                stop = 1;
            else
                ERR(write (STDOUT_FILENO, buff->data, buff->size) == -1);

            ERR(shmdt (buff) );

            /* critical section #4 end */

            semops_add (SEM_MUTEX, -1, SEM_UNDO | IPC_NOWAIT);
            semops_add (SEM_FULL, -1, IPC_NOWAIT);
            semops_add (SEM_EMPTY, 1, SEM_UNDO);
            semops_add (SEM_EMPTY, -1, IPC_NOWAIT);
            ERR (semops_do (semid) == -1);

            /* SEM_FULL == 0 && SEM_EMPTY == 0 */
        }

        /* critical section #2 end */

        exit (EXIT_SUCCESS);
    } 
}

void semops_add (enum SEM_NUMBERS num, short op, short flags)
{
    assert (semops.nops < NOPS);
    semops.sops[semops.nops].sem_num = num;
    semops.sops[semops.nops].sem_op  = op;
    semops.sops[semops.nops].sem_flg = flags;

    semops.nops++;
}

int semops_do (int semid)
{
    assert (semid >= 0);
    assert (semops.nops < NOPS);

    if (semtimedop (semid, semops.sops, semops.nops, &timeout) == -1)
        return -1;

    semops.nops = 0;

    return 0;
}

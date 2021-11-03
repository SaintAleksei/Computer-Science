#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

#ifdef DEBUG
#define CHECK_ERROR(condition)\
    if (condition)\
    {\
        fprintf (stderr, "%s:%d: ERROR: %s\n", __FILE__, __LINE__, strerror (errno) );\
        exit (EXIT_FAILURE);\
    }
#else
#define CHECK_ERROR(condition)\
    if (condition)\
        exit (EXIT_FAILURE);
#endif

typedef struct msg
{
    long type;
    char data;
} msg_t;

int main (int argc, char **argv)
{
    if (argc != 2)
    {
        fprintf (stderr, "Usage: %s <number>\n", argv[0]);
        
        exit (EXIT_FAILURE);
    }

    char *endptr = NULL;
    errno = 0;
    unsigned long long num = strtoull (argv[1], &endptr, 0);    

    if ( (num == ULLONG_MAX && errno == ERANGE) || *endptr)
    {
        fprintf (stderr, "Usage: %s <number>\n", argv[0]);
        
        exit (EXIT_FAILURE);
    }

    int id = msgget (IPC_PRIVATE, 0666);
    CHECK_ERROR (id == -1);

    pid_t main_pid = getpid ();
    size_t child_number = 0; 
    for (size_t i = 1; i <= num; i++)
    {
        pid_t pid = fork ();

        if (!pid)
        {
            child_number = i;
            break;
        }
    }

    int ret = 0;
    msg_t msg = {};
    if (getpid () == main_pid)
        for (size_t i = 1; i <= num; i++)
        {
            msg.type = i * 2;

            ret = msgsnd (id, &msg, 1, 0);
            CHECK_ERROR (ret == -1);

            ret = msgrcv (id, &msg, 1, i * 2 + 1, 0);
            CHECK_ERROR (ret == -1);
        }
    else
    {
        ret = msgrcv (id, &msg, 1, child_number * 2, 0);
        CHECK_ERROR (ret == -1);

        printf ("NUMBER: %lu\n", child_number);

        msg.type = child_number * 2 + 1;

        ret = msgsnd (id, &msg, 1, 0);
        CHECK_ERROR (ret == -1);
    }
            

    if (getpid () == main_pid)
        msgctl (id, IPC_RMID, NULL);

    exit (EXIT_SUCCESS);
}

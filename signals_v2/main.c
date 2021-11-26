#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>

#define ERR(condition)\
    do\
    {\
        if (condition)\
        {\
            fprintf (stderr, "ERROR: %s:%d in function %s: %s\n", __FILE__, __LINE__, __PRETTY_FUNCTION__, strerror (errno) );\
            exit (EXIT_FAILURE);\
        }\
    }\
    while (0)

struct buffer
{
    uint8_t data[BUFSIZ];
    size_t  size;
} buff;

const struct timespec timeout = {.tv_sec = 2};

sigset_t set;

pid_t pid;

int main (int argc, char **argv)
{    
    if (argc != 2)
    {
        fprintf (stderr, "Usage: %s <name_of_file>\n", argv[0]);
        
        exit (EXIT_FAILURE);
    }

    sigemptyset (&set);
    sigaddset (&set, SIGUSR1);
    sigaddset (&set, SIGUSR2);
    ERR (sigprocmask (SIG_BLOCK, &set, NULL) == -1);

    pid_t pid = fork ();
    ERR (pid == -1);

    if (pid > 0) /* sender */
    {
        int fd = open (argv[1], O_RDONLY);
        ERR (fd == -1);

        while (1)
        {
            ssize_t nbytes = read (fd, buff.data, BUFSIZ);
            ERR (nbytes < 0);

            if (!nbytes)
                return EXIT_SUCCESS;

            buff.size = 0;
        
            while (buff.size < (size_t) nbytes * 8)
            {
                if (buff.data[buff.size / 8] & (1 << (buff.size % 8) ) )
                    kill (pid, SIGUSR1);
                else
                    kill (pid, SIGUSR2); 

                if (sigtimedwait (&set, NULL, &timeout) == -1)
                    return EXIT_SUCCESS;

                buff.size++;
            } 
        }
    }
    else /* receiver */
    {
        ssize_t nbytes = 0;
        pid = getppid ();

        while (1)
        {
            int signum = sigtimedwait (&set, NULL, &timeout);

            if (signum == -1)
            {
                nbytes = write (STDOUT_FILENO, buff.data, buff.size / 8);
                ERR (nbytes == -1);

                return EXIT_SUCCESS;
            }

            if (signum == SIGUSR1)
                buff.data[buff.size / 8] |= 1 << (buff.size % 8);
            else if (signum == SIGUSR2)
                buff.data[buff.size / 8] &= ~(1 << (buff.size % 8) ); 
            else
                ERR(1);

            buff.size++;

            if (buff.size == BUFSIZ * 8)
            {
                nbytes = write (STDOUT_FILENO, buff.data, BUFSIZ);
                ERR (nbytes == -1);

                buff.size = 0;
            }

            kill (pid, SIGUSR1);
        }
    }
}

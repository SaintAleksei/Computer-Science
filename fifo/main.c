#include <unistd.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>

#define BUFSIZE PIPE_BUF
#define NAMESIZE 0x40
#define TIMEOUT 3

#define ERR(condition)\
    if (condition)\
    {\
        fprintf (stderr, "ERROR: %s:%u:%s: %s\n", __FILE__, __LINE__,\
                 __PRETTY_FUNCTION__, strerror (errno) );\
        exit (EXIT_FAILURE);\
    }

char fifo_name[NAMESIZE] = "";

char buf[BUFSIZE] = "";

int find_sender ();
int find_reciever ();

int main (int argc, char *argv[])
{
    if (argc > 2)
    {
        fprintf (stderr, "Usage: %s <name_of_file> or %s\n", argv[0], argv[0]);
        exit (EXIT_FAILURE);
    }      

    if (argc == 1) /* Reciever */
    {
        int fd_fifo = find_sender ();

        ssize_t bytes_count = 0;
        uint8_t no_sender   = 1;

        while (1)
        {
            bytes_count = read (fd_fifo, buf, BUFSIZE);
            ERR (bytes_count == -1);

            if (!bytes_count)
            {
                if (no_sender)
                    fprintf (stderr, "No sender\n");
                ERR (unlink (fifo_name) == -1);
                exit (EXIT_SUCCESS);
            }

            bytes_count = write (STDOUT_FILENO, buf, bytes_count);
            ERR (bytes_count == -1);

            no_sender = 0;
        }
    }
    else /* Sender */
    {
        int fd_input = open (argv[1], O_RDONLY);
        ERR (fd_input == -1);

        int fd_fifo = find_reciever ();
        
        ssize_t bytes_count = 0;

        while (1)
        {
            bytes_count = read (fd_input, buf, BUFSIZE);
            ERR (bytes_count == -1);

            if (!bytes_count)
                exit (EXIT_SUCCESS);

            bytes_count = write (fd_fifo, buf, bytes_count);
            ERR (bytes_count == -1);
        }
    }
}

int find_reciever () /* Sender */
{
    ERR (mkfifo (".file.fifo", S_IWUSR | S_IRUSR) && errno != EEXIST);

    ssize_t nbytes = 0;

    while (nbytes == 0) 
    {
        int fd_common_fifo = open (".file.fifo", O_RDONLY);
        ERR (fd_common_fifo == -1);

        nbytes = read (fd_common_fifo, fifo_name, NAMESIZE);
        ERR (nbytes == -1);

        close (fd_common_fifo);
    }

    int fd_fifo = open (fifo_name, O_WRONLY | O_NONBLOCK);
    ERR (fd_fifo == -1);

    ERR (fcntl (fd_fifo, F_SETFL, O_WRONLY) == -1);

    return fd_fifo;
}

int find_sender () /* Reciever */
{
    ERR (mkfifo (".file.fifo", S_IWUSR | S_IRUSR) && errno != EEXIST);

    pid_t fifo_pid = getpid ();
    snprintf (fifo_name, NAMESIZE, ".%u.fifo", fifo_pid);

    ERR (mkfifo (fifo_name, S_IWUSR | S_IRUSR) && errno != EEXIST);

    int fd_fifo = open (fifo_name, O_RDONLY | O_NONBLOCK);
    ERR (fd_fifo == -1);

    ERR (fcntl (fd_fifo, F_SETFL, O_RDONLY) == -1);

    int fd_common_fifo = open (".file.fifo", O_WRONLY);
    ERR (fd_common_fifo == -1);

    ssize_t bytes_count = write (fd_common_fifo, fifo_name, NAMESIZE);
    ERR (bytes_count == -1);

    close (fd_common_fifo);

    sleep (TIMEOUT);

    return fd_fifo;
}

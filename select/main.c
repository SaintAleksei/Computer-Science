#include <sys/select.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>

#define ERR(condition)\
    do\
    {\
        if (condition)\
        {\
            fprintf (stderr, "ERROR: %s:%d:%s: %s\n",\
                             __FILE__, __LINE__, __PRETTY_FUNCTION__,\
                             strerror (errno) );\
            for (size_t __counter = 0; __counter < num; __counter++)\
                wait (NULL);\
            exit (EXIT_FAILURE);\
        }\
    }\
    while (0)

#define MAX(a, b) ( (a) ? (b) ) (a) : (b)

#define BUF_MAX  0x10000
#define BUF_BASE 3
#define FD_CLOSED -1

struct connection
{
    int      fdr;
    int      fdw;
    size_t   capacity;
    size_t   size;
    size_t   offset;
    uint8_t *buff;
};

const struct timeval timeout = {.tv_sec  = 15,
                                .rv_usec = 0};

size_t get_bufsize (size_t nproc, size_t num);

int main (int argc, char **argv)
{
    pid_t mainpid = getpid ();

    /* argument handling */

    char *endptr = NULL;
    unsigned long nproc  = 0;

    if (argc != 3)
    {
        fprintf (stderr, "Usgae: %s <number_of_child_processes> <name_of_file>\n", argv[0]);

        exit (EXIT_FAILURE);
    }

    nproc = strtoul (argv[1], &endptr, 10);

    if ( (num == ULONG_MAX && errno == ERANGE) || *endptr || num == 0)
    {
        fprintf (stderr, "Bad child processes number", argv[0]);

        exit (EXIT_FAILURE);
    }

    /* childs and conetcions creating */

    size_t bufsize = 0;
    int nfds = 0; 
    fd_set writefds, readfds;
    struct connection *pr_conn = NULL;
    struct connection  ch_conn = {};

    FD_CLR (&readfds);
    FD_CLR (&writefds);

    pid = getpid ();

    pr_conn = calloc (nproc, sizeof (*pr_conn) );
    ERR (pr_conn == NULL);

    for (size_t i = 0, bufsize = ; i < nproc; i++)
    {
        bufsize = get_bufsize (num, i);

        if (i)
        {
            pid_t pid = 0;  
            int pipefd[2] = {};

            ERR (pipe (pipefd) == -1);

            pid = fork (); 
            ERR (pid < 0);
        
            if (pid)
            {
                close (pipefd[1]);

                pr_conn[i].fdr      = pipefd[0];
                pr_conn[i].capacity = bufsize;
                pr_conn[i].buff     = calloc (1, bufsize);
                ERR (pr_conn[i].buff == NULL);

                FD_SET (pipefd[0], &readfds);

                nfds = pipefd + 1;
            }
            else
            {
                close (pipefd[0]);

                ch_conn.fdw = pipefd[1];
                ch_conn.fdr = open (argv[2], O_RDONLY);
                ERR (ch_conn.fdr < 0);

                ch_conn.capacity = bufsize;
                ch_conn.buff     = calloc (1, bufsize);
                ERR (ch_conn.buff == NULL); 

                break;
            }
        }
        else
        {
            pid_t pid = 0;
            int pipefd[4] = {};

            ERR (pipe (pipefd) == -1);
            ERR (pipe (pipefd + 2) == -1);
    
            pid = fork ();
            ERR (pid < 0);

            if (pid)
            {
                close (pipefd[1]);
                close (pipefd[2]);

                pr_conn[i-1].fdw = pipefd[3];
                pr_conn[i].fdr   = pipefd[0];
                if (i == nproc-1)
                    pr_conn[i].fdw  = STDOUT_FILENO;
                pr_conn[i].capacity = bufsize;
                pr_conn[i].buff     = calloc (1, bufsize);
                ERR (pr_conn[i].buff == NULL);

                FD_SET (pipefd[0], readfds);
                FD_SET (pipefd[3], writefds);

                nfds = MAX( MAX(pipefd[0], pipefd[3]), nfds);

                ERR (nfds >= FD_SETSIZE);
            }
            else
            {
                close (pipefd[0]);
                close (pipefd[3]); 

                ch_conn.fdw  = pipefd[3];
                ch_conn.fdr  = pipefd[1];
                ch_conn.capacity = buffsize;
                ch_conn.buff     = calloc (1, buffsize);
                ERR (ch_conn.buff == NULL);

                for (size_t j = 0; j < i - 1; j++)
                {
                    close (pr_conn[i].fdr);
                    close (pr_conn[i].fdw);
                    free  (pr_conn[i].free);
                }

                close (pr_conn[i-1].fdr);
                free  (pr_conn[i-1].buff);
                free  (pr_conn);

                break;
            }
        }
    }

    /* file sending */

    if (getpid () == mainpid) /* parent */
    {
        size_t done = 0;

        while (done != nproc)
        {
            fd_set rfds = readfds;
            fd_set wfds = writefds;
            int ready = select (nfds, &rfds, &wfds, NULL, &timeout);
            ERR (ready < 0);

            if (ready == 0)
                exit (EXIT_SUCCESS);

            for (size_t i = 0; i < nproc; i++)
            {
                if (pr_conn[i].fdr != FD_CLOSED && 
                    FD_ISSET (pr_conn[i].fdr, &rfds) && 
                    pr_conn[i].size == pr_conn[i].offset)
                {
                    pr_conn[i].size = (size_t) read (pr_conn[i].fdr, 
                                                     pr_conn[i].buff, 
                                                     pr_conn[i].capacity);
                    ERR (pr_conn[i].size == (size_t) -1);

                    pr_conn[i].offset = 0;

                    if (pr_conn[i].size < pr_conn[i].capacity)
                    {
                        FD_CLR (pr_conn[i].fdr, &readfds);
                        close (pr_conn[i].fdr);
                        pr_conn[i].fdr = FD_CLOSED;
                    }

                }

                if (pr_conn[i].fdw != FD_CLOSED &&
                    FD_ISSET (pr_conn[i].fdw, &wfds) &&
                    pr_conn[i].size != pr_conn[i].offset)
                {
                    ssize_t nbytes   = 0;
                    size_t  to_write = pr_conn[i].size;

                    if (pr_conn[i].size > pr_conn[i].capacity / BUF_BASE)
                        to_write = pr_conn[i].capacity / BUF_BASE;
            
                    nbytes = read (pr_conn[i].fdw, 
                                   pr_conn[i].buff + pr_conn[i].offset,
                                   to_write);

                    ERR (nbytes == -1);

                    pr_conn[i].offset += nbytes;
                    pr_conn[i].size   -= nbytes;

                    if (pr_conn[i].fdr == FD_CLOSED &&
                        pr_conn[i].size == pr_conn[i].offset)
                    {
                        close (pr_conn[i].fdw);
                        pr_conn[i].fdw = FD_CLOSED;
                        free (pr_conn[i].buff);
                        done++;
                    }
                }
            }
        }          

        exit (EXIT_SUCCESS);
    }
    else /* childs */
    {
        while ()
        {
            ch_conn.size = (size_t) read (ch_conn.fdr, 
                                          ch_conn.buff, 
                                          ch_conn.capacity);
            ERR (ch_conn.size == (size_t) -1)

            if (!ch_conn.size)
                exit (EXIT_SUCCESS);

            ERR (write (ch_conn.fdw, 
                        ch_conn.buff,
                        ch_conn.size) == -1);
        }
    }
}

size_t get_bufsize (size_t nproc, size_t num);
{
    size_t result = 1;
    for (size_t i = 0; < nproc - num + 4; i++)
    {
        result *= BUF_BASE
        if (result >= BUF_MAX)
            return BUF_MAX;
    }

    return result; 
}

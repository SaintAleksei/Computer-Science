#include <sys/select.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>

#define ERR(condition)\
    do\
    {\
        if (condition)\
        {\
            fprintf (stderr, "ERROR: %s:%d:%s: %s\n",\
                             __FILE__, __LINE__, __PRETTY_FUNCTION__,\
                             strerror (errno) );\
            exit (EXIT_FAILURE);\
        }\
    }\
    while (0)

#define MAX(a, b) ( (a) > (b) ) ? (a) : (b)

#define BUF_MAX  0x20000
#define BUF_BASE 3
#define FD_CLOSED -1
#define TIMEOUT 15

struct connection
{
    int      fdr; /* descriptor for reading */
    int      fdw; /* descriptor for writing */
    size_t   capacity; /* Capacity of buffer */
    size_t   size; /* Count of bytes to write */
    size_t   offset; /* Current position in buff */
    uint8_t *buff; 
};

size_t get_bufsize (size_t nproc, size_t num);

int main (int argc, char **argv)
{
    pid_t mainpid = getpid ();

    /* argument handling */

    char *endptr = NULL;
    unsigned long nproc  = 0;

    if (argc != 3)
    {
        fprintf (stderr, "Usgae: %s <number_of_child_processes> "
                         "<name_of_file>\n", argv[0]);

        exit (EXIT_FAILURE);
    }

    nproc = strtoul (argv[1], &endptr, 10);

    if ( (nproc == ULONG_MAX && errno == ERANGE) || *endptr || nproc == 0)
    {
        fprintf (stderr, "Bad child processes number");

        exit (EXIT_FAILURE);
    }

    /* Creating childs and connections */

    int nfds = 0; 
    fd_set writefds,
           readfds; 
    struct connection *pr_conn = NULL; /* array with parrent connections */
    struct connection  ch_conn = {};   /* child connection */

    FD_ZERO (&readfds);
    FD_ZERO (&writefds);

    pr_conn = calloc (nproc, sizeof (*pr_conn) );
    ERR (pr_conn == NULL);
    
    for (size_t i = 0; i < nproc; i++)
    {
        size_t bufsize = get_bufsize (nproc, i);
        int pipefd[4] = {};
        pid_t pid = 0;

        if (!i)
        {
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

                nfds = pipefd[0] + 1;
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

                free (pr_conn);

                break;
            }
        }
        else
        {
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
                pr_conn[i].capacity = bufsize;
                pr_conn[i].buff     = calloc (1, bufsize);
                ERR (pr_conn[i].buff == NULL);

                FD_SET (pipefd[0], &readfds);
                FD_SET (pipefd[3], &writefds);

                nfds = MAX( MAX(pipefd[0] + 1, pipefd[3] + 1), nfds);

                ERR (nfds >= FD_SETSIZE);
            }
            else
            {
                close (pipefd[0]);
                close (pipefd[3]); 

                ch_conn.fdw  = pipefd[1];
                ch_conn.fdr  = pipefd[2];
                ch_conn.capacity = bufsize;
                ch_conn.buff     = calloc (1, bufsize);
                ERR (ch_conn.buff == NULL);

                for (size_t j = 0; j < i; j++)
                {
                    close (pr_conn[j].fdr);
                    close (pr_conn[j].fdw);
                    free  (pr_conn[j].buff);
                }

                FD_ZERO (&readfds);
                FD_ZERO (&writefds);

                free  (pr_conn);

                pr_conn = NULL;

                break;
            }
        }
    }

    if (pr_conn)
    {
        pr_conn[nproc-1].fdw = STDOUT_FILENO;
        FD_SET (STDOUT_FILENO, &writefds);

        nfds = MAX (STDOUT_FILENO + 1, nfds); 
    }

    /* file sending */

    if (getpid () == mainpid) /* parent */
    {
        size_t done = 0;

        for (size_t i = 0; i < nproc; i++)
        {
            ERR (fcntl (pr_conn[i].fdr, F_SETFL, O_RDONLY | O_NONBLOCK) == -1);
            ERR (fcntl (pr_conn[i].fdw, F_SETFL, O_WRONLY | O_NONBLOCK) == -1);
        }

        while (done != nproc)
        {
            struct timeval timeout = {.tv_sec = TIMEOUT};
            fd_set rfds = readfds;
            fd_set wfds = writefds;

            /* Waiting for connections, that isn't blocked */
            int ready = select (nfds, &rfds, &wfds, NULL, &timeout);
            ERR (ready < 0);

            /* True if waiting time is out */
            if (ready == 0)
                exit (EXIT_SUCCESS);

            for (size_t i = 0; i < nproc; i++)
            {
                if (/* True if connection wasn't closed */
                    pr_conn[i].fdr != FD_CLOSED &&      
                    /* True if reading isn't blocked */
                    FD_ISSET (pr_conn[i].fdr, &rfds) && 
                    /* True if buffer is empty (previous data was wrote) */
                    !pr_conn[i].size)                   
                {
                    pr_conn[i].size = (size_t) read (pr_conn[i].fdr, 
                                                     pr_conn[i].buff, 
                                                     pr_conn[i].capacity);
                    ERR (pr_conn[i].size == (size_t) -1);

                    pr_conn[i].offset = 0;

                    if (!pr_conn[i].size)
                    {
                        /* Closing connection */
                        FD_CLR (pr_conn[i].fdr, &readfds);
                        FD_CLR (pr_conn[i].fdw, &writefds);
                        close (pr_conn[i].fdr);
                        close (pr_conn[i].fdw);
                        pr_conn[i].fdr = FD_CLOSED;
                        pr_conn[i].fdw = FD_CLOSED;
                        free (pr_conn[i].buff);

                        /* Increasing closed connections count */ 
                        done++;
                    }
                }

                if (/* True if connections isn't closed */
                    pr_conn[i].fdw != FD_CLOSED &&
                    /* True if writing isn't blocked */
                    FD_ISSET (pr_conn[i].fdw, &wfds) &&
                    /* True if buffer isn't empty*/
                    pr_conn[i].size)
                {
                    ssize_t nbytes = write (pr_conn[i].fdw, 
                                            pr_conn[i].buff + pr_conn[i].offset,
                                            pr_conn[i].size);
                    ERR (nbytes < 0);
    
                    pr_conn[i].offset += nbytes;
                    pr_conn[i].size   -= nbytes;
                }
            }
        }          

        free (pr_conn);
        exit (EXIT_SUCCESS);
    }
    else /* childs */
    {
        while (1)
        {
            ch_conn.size = (size_t) read (ch_conn.fdr, 
                                          ch_conn.buff, 
                                          ch_conn.capacity);
            ERR (ch_conn.size == (size_t) -1);

            if (!ch_conn.size)
                exit (EXIT_SUCCESS);

            ERR (write (ch_conn.fdw, 
                        ch_conn.buff,
                        ch_conn.size) == -1);
        }
    }
}

size_t get_bufsize (size_t nproc, size_t num)
{
    size_t result = 1;
    for (size_t i = 0; i < nproc - num + 4; i++)
    {
        result *= BUF_BASE;
        if (result >= BUF_MAX)
            return BUF_MAX;
    }

    return result; 
}

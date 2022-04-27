#define _GNU_SOURCE
#include <sys/sysinfo.h>
#include <fcntl.h>
#include <error.h>
#include <unistd.h>
#include <sched.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>
#include <errno.h>
#include <math.h>

#define INTEGRAL_MAXTHREADS CPU_SETSIZE
#define INTEGRAL_START 0.0
#define INTEGRAL_END 500.0
#define INTEGRAL_DX 1e-6

#define MAX(A, B) (( (A) > (B) ) ? (A) : (B))

struct thread_info
{
    double start;
    double end;
    double result;
};

void *thread_cb(void *data);

int main(int argc, char **argv)
{
    double result = 0.0;
    int retval = 0;

    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <number of threads>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *endptr = NULL;
    errno = 0;
    unsigned long nthreads = strtoul(argv[1], &endptr, 10);

    if (argv[1][0] == '-' || errno == ERANGE || *endptr != '\0')
    {
        fprintf(stderr, "Bad number of threads\n"); 
        exit(EXIT_FAILURE);
    }

    unsigned long nprocs = get_nprocs();

    pthread_t *thread = calloc(MAX(nprocs, nthreads), sizeof(*thread));
    if (!thread)
        error(EXIT_FAILURE, errno, "calloc");

    pthread_attr_t attr;

    retval = pthread_attr_init(&attr);
    if (retval != 0)
        error(EXIT_FAILURE, errno, "pthread_attr_init");

    size_t alignment = sysconf(_SC_PAGESIZE);
    uint8_t *buf = NULL;
    retval = posix_memalign((void **) &buf, alignment, alignment * MAX(nprocs, nthreads));
    if (retval != 0)
        error(EXIT_FAILURE, errno, "posix_memalign");

    double step = (INTEGRAL_END - INTEGRAL_START) / (double) nthreads;
    double start = 0.0;
    for (unsigned long i = 0; i < MAX(nthreads, nprocs); i++, start += step)
    {
        cpu_set_t set;

        CPU_ZERO(&set); 
        if ((i / nprocs) % 2)
            CPU_SET(((i * 2) + 1) % nprocs, &set);
        else
            CPU_SET((i * 2) % nprocs, &set);

        retval = pthread_attr_setaffinity_np(&attr, sizeof(set), &set);
        if (retval != 0)
            error(EXIT_FAILURE, errno, "pthread_attr_setaffinity_np");

        struct thread_info *tinfo = (struct thread_info *)(buf + i * alignment);
        tinfo->start = start;
        tinfo->end = start + step;

        retval = pthread_create(thread + i, &attr, thread_cb, tinfo);
        if (retval != 0)
            error(EXIT_FAILURE, errno, "pthread_create");
    }

    retval = pthread_attr_destroy(&attr);
    if (retval != 0)
        error(EXIT_FAILURE, errno, "pthread_attr_destroy");

    for (unsigned long i = 0; i < MAX(nthreads, nprocs); i++)
    {
        int retval = pthread_join(thread[i], NULL);
        if (retval != 0)
            error(EXIT_FAILURE, retval, "pthread_join");

        if (i < nthreads)
            result += ((struct thread_info *) (buf + i * alignment))->result;
    }

    printf("Result: %lg\n", result);

    free(buf);
    free(thread);
}

double func(double x)
{
    return x * cos(x);
}

void *thread_cb(void *data)
{
    struct thread_info *tinfo = data;

    for (double x = tinfo->start; x < tinfo->end; x += INTEGRAL_DX)
        tinfo->result += func(x) * INTEGRAL_DX;

    return NULL;
}

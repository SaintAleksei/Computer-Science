#define _GNU_SOURCE
#include <sys/sysinfo.h>
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

struct thread_info
{
    double start;
    double end;
    double result;
};

void *thread_cb(void *data);

int main(int argc, char **argv)
{
    pthread_t thread[INTEGRAL_MAXTHREADS];
    double result = 0.0;
    int retval = 0;

    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <number of threads>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *endptr = NULL;
    unsigned long nthreads = strtoul(argv[1], &endptr, 10);

    if (argv[1][0] == '-' || (nthreads == ULONG_MAX && errno == ERANGE) || *endptr != '\0')
    {
        fprintf(stderr, "Bad number of threads\n"); 
        exit(EXIT_FAILURE);
    }

    unsigned long nprocs = get_nprocs();

    if (nthreads > nprocs)
    {
        fprintf(stderr, "Too much threads\n");
        exit(EXIT_FAILURE);
    }

    pthread_attr_t attr;

    retval = pthread_attr_init(&attr);
    assert(retval == 0);

    size_t alignment = sysconf(_SC_PAGESIZE);
    uint8_t *buf = NULL;
    retval = posix_memalign((void **) &buf, alignment, alignment * nprocs);
    assert(retval == 0);

    double step = (INTEGRAL_END - INTEGRAL_START) / (double) nthreads;
    double start = 0.0;
    for (unsigned long i = 0; i < nprocs; i++, start += step)
    {
        cpu_set_t set;

        CPU_ZERO(&set); 
        CPU_SET(i, &set);

        retval = pthread_attr_setaffinity_np(&attr, sizeof(set), &set);
        assert(retval == 0);

        struct thread_info *tinfo = (struct thread_info *)(buf + i * alignment);
        tinfo->start = start;
        tinfo->end = start + step;

        retval = pthread_create(thread + i, &attr, thread_cb, tinfo);
        assert(retval == 0);
    }

    retval = pthread_attr_destroy(&attr);
    assert(retval == 0);

    for (unsigned long i = 0; i < nprocs; i++)
    {
        int retval = pthread_join(thread[i], NULL);
        if (retval != 0)
            error(EXIT_FAILURE, retval, "pthread_join");

        if (i < nthreads)
            result += ((struct thread_info *) (buf + i * alignment))->result;
    }

    printf("Result is %lg\n", result);

    free(buf);
}

double func(double x)
{
    return cos(x);
}

void *thread_cb(void *data)
{
    struct thread_info *tinfo = data;

    for (double x = tinfo->start; x < tinfo->end; x += INTEGRAL_DX)
        tinfo->result += func(x) * INTEGRAL_DX;

    return NULL;
}

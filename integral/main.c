#define _GNU_SOURCE
#include <sys/sysinfo.h>
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
#define INTEGRAL_END 100.0
#define INTEGRAL_DX 1e-6

struct thread_info
{
    double start;
    double end;
    double result;
};

void *callback(void *data);

int main(int argc, char **argv)
{
    pthread_t thread[INTEGRAL_MAXTHREADS];
    struct thread_info tinfo[INTEGRAL_MAXTHREADS];
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

    if (nthreads > INTEGRAL_MAXTHREADS)
    {
        fprintf(stderr, "Too much threads\n");
        exit(EXIT_FAILURE);
    }

    unsigned long nprocs = get_nprocs();
    pthread_attr_t attr;

    retval = pthread_attr_init(&attr);
    assert(retval == 0);

    double step = (INTEGRAL_END - INTEGRAL_START) / (double) nthreads;
    double start = 0.0;
    for (unsigned long i = 0; i < nthreads; i++, start += step)
    {
        cpu_set_t set;

        CPU_ZERO(&set); 
        CPU_SET(i % nprocs, &set);

        retval = pthread_attr_setaffinity_np(&attr, sizeof(set), &set);
        assert(retval == 0);

        tinfo[i].start = start;
        tinfo[i].end = start + step;

        retval = pthread_create(thread + i, &attr, callback, tinfo + i);
        assert(retval == 0);

    }

    retval = pthread_attr_destroy(&attr);
    assert(retval == 0);

    for (unsigned long i = 0; i < nthreads; i++)
    {
        int retval = pthread_join(thread[i], NULL);
        assert(retval == 0);

        result += tinfo[i].result;
    }

    fprintf(stderr, "Result is %lg\n", result);
}

double func(double x)
{
    return x*x + x*cos(x);
}

void *callback(void *data)
{
    struct thread_info *tinfo = data;

    for (double x = tinfo->start; x < tinfo->end; x += INTEGRAL_DX)
        tinfo->result += func(x) * INTEGRAL_DX;

    return NULL;
}

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

#define MAX(A, B) (( (A) > (B) ) ? (A) : (B))

struct threadCxt
{
    double start;
    double end;
    double dx;
    double result;
    double (*func) (double x);
};

void *threadCb(void *cxt);

int threads_integrate(size_t nthreads, double rangeStart, double rangeEnd, 
                      double dx, double (*func)(double x), double *result)
{
    assert(rangeStart < rangeEnd);
    assert(func);
    assert(result);

    size_t nprocs = get_nprocs();
    size_t nworkers = MAX(nprocs, nthreads);

    pthread_t *thread = calloc(nworkers, sizeof(*thread));
    if (!thread)
        error(EXIT_FAILURE, errno, "calloc");

    pthread_attr_t attr;

    int retval = pthread_attr_init(&attr);
    if (retval != 0)
        error(EXIT_FAILURE, errno, "pthread_attr_init");

    size_t alignment = sysconf(_SC_PAGESIZE);
    uint8_t *buf = NULL;
    retval = posix_memalign((void **) &buf, alignment, alignment * nworkers);
    if (retval != 0)
        error(EXIT_FAILURE, errno, "posix_memalign");

    double step = (rangeEnd - rangeStart) / (double) nthreads;
    double start = 0.0;
    for (unsigned long i = 0; i < nworkers; i++, start += step)
    {
        cpu_set_t set;

        CPU_ZERO(&set); 
        if (((i * 2) / nprocs) % 2)
            CPU_SET(((i * 2) + 1) % nprocs, &set);
        else
            CPU_SET((i * 2) % nprocs, &set);

        retval = pthread_attr_setaffinity_np(&attr, sizeof(set), &set);
        if (retval != 0)
            error(EXIT_FAILURE, errno, "pthread_attr_setaffinity_np");

        struct threadCxt *tcxt = (struct threadCxt *)(buf + i * alignment);
        tcxt->start = start;
        tcxt->end = start + step;
        tcxt->dx = dx;
        tcxt->func = func;

        retval = pthread_create(thread + i, &attr, threadCb, tcxt);
        if (retval != 0)
            error(EXIT_FAILURE, errno, "pthread_create");
    }

    retval = pthread_attr_destroy(&attr);
    if (retval != 0)
        error(EXIT_FAILURE, errno, "pthread_attr_destroy");

    *result = 0;
    for (unsigned long i = 0; i < nworkers; i++)
    {
        int retval = pthread_join(thread[i], NULL);
        if (retval != 0)
            error(EXIT_FAILURE, retval, "pthread_join");

        if (i < nthreads)
            *result += ((struct threadCxt *) (buf + i * alignment))->result;
    }

    free(buf);
    free(thread);

    return 0;
}

void *threadCb(void *cxt)
{
    assert(cxt);

    struct threadCxt *tcxt = cxt;

    assert(tcxt->func); 
    for (double x = tcxt->start; x < tcxt->end; x += tcxt->dx)
        tcxt->result += tcxt->func(x) * tcxt->dx;

    return NULL;
}

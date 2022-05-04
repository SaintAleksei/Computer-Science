#ifndef THREADS_H_INCLUDED
#define THREADS_H_INCLUDED

#include <stddef.h>

int threads_integrate(size_t nthreads, double rangeStart, double rangeEnd,
                      double dx, double (*func)(double x), double *result);

#endif

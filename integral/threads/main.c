#include "config.h"
#include "threads.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

double func(double x) {return INTEGRAL_FUNC(x);}

int main(int argc, char **argv)
{
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

    double result = 0.0;
    threads_integrate(nthreads, INTEGRAL_RANGE_START, INTEGRAL_RANGE_END,
                      INTEGRAL_DX, func, &result);

    printf("Result: %lg\n", result);

    return 0;
}

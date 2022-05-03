#ifndef LOG_H_INCLUDED
#define LOG_H_INCLUDED

#include <stdio.h>
#include <unistd.h>
#include <assert.h>

extern FILE *__log_stream;

#ifdef INTEGRAL_OPTLOG
    
    #define LOG_WRITE(...)\
        fprintf(__log_stream, __VA_ARGS__)

    #define LOG_INIT(name)\
        void __attribute__((constructor)) _log_open()\
        {\
            assert(name);\
            char path[0x100];\
            snprintf(path, 0x100, "/tmp/%s_%u.log", name, getpid());\
            __log_stream = fopen(path, "w");\
            assert(__log_stream);\
        }\
        void __attribute__((destructor))__log_close()\
        {\
            fclose(__log_stream);\
        }

#else

    #define LOG_WRITE
    #define LOG_INIT

#endif

#endif

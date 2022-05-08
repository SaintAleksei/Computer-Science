#ifndef LOG_H_INCLUDED
#define LOG_H_INCLUDED

#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <time.h>

extern FILE *__log_stream;

#ifdef INTEGRAL_LOG

    #define LOG_WRITE(...)\
        do\
        {\
            fprintf(__log_stream, __VA_ARGS__);\
        } while(0)

    #define LOG_ERROR(...)\
        do\
        {\
            fprintf(__log_stream, "%s:%d:%s: ERROR: ", __FILE__, __LINE__, __PRETTY_FUNCTION__);\
            fprintf(__log_stream, __VA_ARGS__);\
            fprintf(__log_stream, ": %s\n", strerror(errno));\
        } while(0)
    

    #define LOG_INIT(name)\
        void __attribute__((constructor))__log_open()\
        {\
            char path[0x100];\
            snprintf(path, 0x100, "/tmp/%s.log", #name);\
            __log_stream = fopen(path, "w");\
            assert(__log_stream);\
            setvbuf(__log_stream, NULL, _IONBF, 0);\
        }\
        void __attribute__((destructor))__log_close()\
        {\
            fclose(__log_stream);\
        }

#else

    #define LOG_WRITE(...)
    #define LOG_INIT(name)

    #define LOG_ERROR(...)\
        do\
        {\
            fprintf(stderr, "%s:%d:%s: ERROR: ", __FILE__, __LINE__, __PRETTY_FUNCTION__);\
            fprintf(stderr, __VA_ARGS__);\
            fprintf(stderr, ": %s\n", strerror(errno));\
        } while(0)

#endif

#endif

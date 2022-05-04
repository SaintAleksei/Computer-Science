#include "client.h"
#include "config.h"
#include "threads.h"
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <error.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>

int client_init(struct Client *cl, int argc, char **argv)
{
    assert(cl);
    
    if (argc != 2)
    {
        LOG_WRITE("Bad usage: not enough arguments\n");
        return 1;
    }

    char *endptr = NULL;
    errno = 0;
    long nthreads = strtol(argv[1], &endptr, 10);
    if (errno || *endptr != '\0' || nthreads < 1)
    {
        LOG_WRITE("Bad usage: bad number of threads\n");
        return 1;
    }

    cl->nthreads = (size_t) nthreads;

    cl->epollfd = epoll_create1(0);
    if (cl->epollfd == -1)
    {
        LOG_ERROR("epoll_create1");
        return -1;
    }

    cl->rangeStart = 0;
    cl->rangeEnd = 0;

    return 0;
}

void client_usage(const char *progname)
{
    assert(progname);
    fprintf(stderr, "Usage: %s <number_of_threads>\n", progname);
}

int client_connectServer(struct Client *cl, struct sockaddr_in *addr, uint16_t port)
{
    assert(cl);
    assert(addr);

    addr->sin_port = htons(port);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        LOG_ERROR("socket(AF_INET, SOCK_STREAM, 0)");
        return -1;
    }

    int retval = connect(sockfd, (struct sockaddr *) addr, sizeof(*addr));
    if (retval == -1)
    {
        LOG_ERROR("connect(%d, [AF_INET:%s:%hu], %lu)",
                  sockfd, inet_ntoa(addr->sin_addr),
                  ntohs(addr->sin_port), sizeof(*addr));
        return -1;
    }

    LOG_WRITE("Connected to server %s:%hu\n", inet_ntoa(addr->sin_addr),
              ntohs(addr->sin_port));

    retval = fcntl(sockfd, F_SETFL, O_NONBLOCK);
    if (retval == -1)
    {
        LOG_ERROR("fcntl(%d, F_SETFL, O_NONBLOCK)", sockfd);
        return -1;
    }

    struct epoll_event ev; 
    ev.events = EPOLLIN | EPOLLRDHUP;
    ev.data.fd = sockfd;
    retval = epoll_ctl(cl->epollfd, EPOLL_CTL_ADD, sockfd, &ev);
    if (retval == -1)
    {
        LOG_ERROR("epoll_ctl(%d, EPOLL_CTL_ADD, %d, "
                  "[%d:EPOLLIN | EPOLLRDHUP])", 
                  cl->epollfd, sockfd, sockfd);
        return -1;
    }

    return 0;
}

int client_recvTask(struct Client *cl)
{
    assert(cl);

    struct MsgTask msg; 
    struct epoll_event ev;
    size_t offset = 0;
    int retval = 0;

    while (offset < sizeof(msg))
    {
        int serverFailure = 0;
        retval = epoll_wait(cl->epollfd, &ev, 1, -1);
        if (retval != 1)
        {
            LOG_ERROR("epoll_wait(%d, - , 1, -1)", cl->epollfd); 
            return -1;
        }

        if (ev.events & EPOLLIN)
        {
            errno = 0;
            retval = recv(ev.data.fd, (uint8_t *) &msg + offset, sizeof(msg), 0);
            if (retval == -1 && errno != ECONNREFUSED)
            {
                LOG_ERROR("read(%d, - , %lu)", ev.data.fd, sizeof(msg));

                return -1;
            }

            if (errno == ECONNREFUSED)
                serverFailure = 1;
            
            offset += retval;
        } 

        if (ev.events & EPOLLRDHUP || serverFailure)
        {
            retval = epoll_ctl(cl->epollfd, EPOLL_CTL_DEL, ev.data.fd, &ev);        
            if (retval == -1)
            {
                LOG_ERROR("epoll_ctl((%d, EPOLL_CTL_DEL, %d, -)", 
                          cl->epollfd, ev.data.fd);
                return -1;  
            }

            close(ev.data.fd);

            LOG_WRITE("Connection to server lost\n");

            return 1;
        }
    }

    LOG_WRITE("New task is received:\n"
              "\trangeStart: \"%s\";\n"
              "\trangeEnd: \"%s\";\n", msg.rangeStart, msg.rangeEnd);

    if (sscanf(msg.rangeStart, "%lg", &cl->rangeStart) != 1)
    {
        LOG_WRITE("Can't parse rangeStart from new task\n");
          
        return -1;
    }

    if (sscanf(msg.rangeEnd, "%lg", &cl->rangeEnd) != 1)
    {
        LOG_WRITE("Can't parse rangeEnd from new task\n");
          
        return -1;
    }

    LOG_WRITE("New task is parsed:\n"
              "\trangeStart: \"%lg\"\n"
              "\trangeEnd: \"%lg\"\n", cl->rangeStart, cl->rangeEnd);

    ev.events = EPOLLOUT | EPOLLRDHUP;
    retval = epoll_ctl(cl->epollfd, EPOLL_CTL_MOD, ev.data.fd, &ev);
    if (retval == -1)
    {
        LOG_ERROR("epoll_ct(%d, EPOLL_CTL_MOD, %d, "
                  "[%d:EPOLLOUT | EPOLLRDHUP])", cl->epollfd,
                   ev.data.fd, ev.data.fd);
        error(EXIT_FAILURE, errno, "epoll_ctl");
    }

    return 0;
}

static double funcToIntegrate(double x)
{
    return INTEGRAL_FUNC(x);
}

int client_processTask(struct Client *cl)
{
    assert(cl);
    int retval = threads_integrate(cl->nthreads, cl->rangeStart, cl->rangeEnd,
                                   INTEGRAL_DX, funcToIntegrate, &cl->result);

    if (!retval)
        LOG_WRITE("Task completed: result: %lg\n", cl->result);
    else
        LOG_WRITE("Task failed: error occurred\n");

    return retval;
}

int client_sendResult(struct Client *cl)
{
    assert(cl);

    struct MsgResult msg;
    snprintf(msg.result, INTEGRAL_STRSIZE, "%*lg", 
             INTEGRAL_STRSIZE - 1, cl->result);
    struct epoll_event ev;
    size_t offset = 0;
    int retval = 0;

    while (offset < sizeof(msg))
    {
        int serverFailure = 0;
        retval = epoll_wait(cl->epollfd, &ev, 1, -1);
        if (retval != 1)
        {
            LOG_ERROR("epoll_wait(%d, -, 1, -1)", cl->epollfd);

            return -1;
        }

        if (ev.events & EPOLLOUT)
        {
            errno = 0;
            retval = send(ev.data.fd, (uint8_t *) &msg + offset, sizeof(msg), 0);
            if (retval == -1 && errno != ECONNRESET)
            {
                LOG_ERROR("write(%d, - , %lu)", ev.data.fd, sizeof(msg));

                return -1;
            }

            if (errno == ECONNRESET)
                serverFailure = 1;

            offset += retval;
        }

        if (ev.events & EPOLLRDHUP || serverFailure)
        {
            retval = epoll_ctl(cl->epollfd, EPOLL_CTL_DEL, ev.data.fd, &ev);
            if (retval == -1)
            {
                LOG_ERROR("epoll_ctl(%d, EPOLL_CTL_DEL, %d, -)",
                          cl->epollfd, ev.data.fd);

                return -1;
            }
            
            close(ev.data.fd);

            LOG_WRITE("Connection to server is lost\n");

            return 1;
        }
    }

    LOG_WRITE("Result is sent to server: result = \"%s\"\n", msg.result);

    ev.events = EPOLLIN | EPOLLRDHUP;
    retval = epoll_ctl(cl->epollfd, EPOLL_CTL_MOD, ev.data.fd, &ev);
    if (retval == -1)
    {
        LOG_ERROR("epoll_ctl(%d, EPOLL_CTL_MOD, %d, "
                  "[%d:EPOLLIN | EPOLLRDHUP", cl->epollfd,
                  ev.data.fd, ev.data.fd);

        return -1;
    }

    return 0;
}

int client_recvBroadcast(struct sockaddr_in *addr, uint16_t port, const char *msg, size_t msgSize)
{
    assert(addr);
    assert(msg);

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1)
    {
        LOG_ERROR("socket(AF_INET, SOCK_DGRAM, 0)\n");
        
        return -1;
    }
    
    struct sockaddr_in bindAddr = 
    {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = INADDR_ANY,
    };

    int retval = bind(sockfd, (struct sockaddr *) &bindAddr, sizeof(bindAddr));
    if (retval == -1)
    {
        LOG_ERROR("bind(%d, [AF_INET:%s:%hu], %lu)\n",
                  sockfd, inet_ntoa(bindAddr.sin_addr), 
                  ntohs(bindAddr.sin_port), sizeof(bindAddr));

        return -1;
    }

    char *recvMsg = calloc(1, msgSize);
    if (!recvMsg)
    {
        LOG_ERROR("calloc(1, %lu)\n", msgSize);

        return -1;
    }

    socklen_t sockLen = sizeof(*addr);
    do
    {
        LOG_WRITE("Waiting for broadcast...\n");

        int retval = recvfrom(sockfd, recvMsg, msgSize, 0,
                              (struct sockaddr *) addr, &sockLen);
        if (retval == -1)
        {
            LOG_ERROR("recvfrom(%d, -, %lu, -, -)",
                      sockfd, msgSize);

            return -1;
        }

        LOG_WRITE("Recieved broadcast from %s:%hu:\n"
                  "\tExpected: \"%s\";\n"
                  "\tRecieved: \"%s\";\n",
                  inet_ntoa(addr->sin_addr), ntohs(addr->sin_port), msg, recvMsg);

    } while (strncmp(recvMsg, msg, msgSize));

    free(recvMsg);
    close(sockfd);

    return 0;
}

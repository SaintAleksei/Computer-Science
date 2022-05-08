#define _GNU_SOURCE
#include "client.h"
#include "config.h"
#include "task.h"
#include "tcpmsg.h"
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#include <fcntl.h>
#include <error.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <time.h>

#define CLIENT_MAX_EPOLL_EVENTS 0x100U
#define CLIENT_EPOLL_TIMEOUT 1000U

static int client_sendResult(struct Client *cl);
static int client_recvTask(struct Client *cl);
static int client_recvBroadcast(struct Client *cl);
static int client_sendResponse(struct Client *cl);
static int client_sendThreadTask(struct Client *cl);
static int client_recvThreadResult(struct Client *cl);

static void *threadCb(void *cxt);

struct ThreadCxt
{
    double result;    
    int readFd;
    int writeFd;
};

int client_init(struct Client *cl, int argc, char **argv)
{
    assert(cl);

    LOG_WRITE("Starting client initialization...\n");
    
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

    LOG_WRITE("Command line args are successfully parsed\n");

    cl->threads = NULL;
    cl->alignedBuf = NULL;
    pthread_attr_t attr;
    cl->threadRead = -1;
    cl->threadWrite = -1;
    int readFd = -1;
    int writeFd = -1;
    size_t i = 0;

    int pipefd[2];
    if (pipe(pipefd) == -1)
    {
        LOG_ERROR("pipe(...)");

        goto cleanup;
    }

    cl->threadRead = pipefd[0];
    writeFd = pipefd[1];

    if (fcntl(cl->threadRead, F_SETFL, O_NONBLOCK) == -1)
    {
        LOG_ERROR("fcntl(%d, F_SETFL, O_NONBLOCK)", cl->threadRead);

        goto cleanup;
    }

    if (pipe(pipefd) == -1)
    {
        LOG_ERROR("pipe(...)");

        goto cleanup;
    }

    readFd = pipefd[0];
    cl->threadWrite = pipefd[1];

    if (fcntl(cl->threadWrite, F_SETFL, O_NONBLOCK) == -1)
    {
        LOG_ERROR("fcntl(%d, F_SETFL, O_NONBLOCK)", cl->threadRead);

        goto cleanup;
    }

    LOG_WRITE("Thread read is ready: %d\n", cl->threadRead);
    LOG_WRITE("Thread write is ready: %d\n", cl->threadWrite);
    
    cl->nthreads = (size_t) nthreads;
    size_t nprocs = get_nprocs();

    cl->threads = calloc(cl->nthreads, sizeof(*cl->threads));
    if (!cl->threads)
    {
        LOG_ERROR("calloc(%lu, %lu)", cl->nthreads,
                  sizeof(*cl->threads));

        goto cleanup; 
    }

    if (pthread_attr_init(&attr) != 0)
    {
        LOG_ERROR("pthread_attr_init");

        goto cleanup;
    }

    cl->alignment = sysconf(_SC_PAGESIZE);
    if (cl->alignment == (size_t) -1)
    {
        LOG_ERROR("sysconf(_SC_PAGESIZE)");

        goto cleanup;
    }

    if (posix_memalign((void **) &cl->alignedBuf, cl->alignment, 
                       cl->alignment * cl->nthreads) != 0)
    {
        LOG_ERROR("posix_memalign");

        goto cleanup;
    }

    for (i = 0; i < cl->nthreads; i++)
    {
        cpu_set_t set;
        size_t cpu = 0;

        CPU_ZERO(&set); 
        if (((i * 2) / nprocs) % 2)
            cpu = ((i * 2) + 1) % nprocs;
        else
            cpu = (i * 2) % nprocs;

        CPU_SET(cpu, &set);

        if (pthread_attr_setaffinity_np(&attr, sizeof(set), &set) != 0)
        {
            LOG_ERROR("pthread_attr_setaffinity_np");

            goto cleanup;
        }
           
        struct ThreadCxt *cxt = (struct ThreadCxt *) 
                                (cl->alignedBuf + i * cl->alignment);
        cxt->result = 0.0;
        cxt->writeFd = writeFd;
        cxt->readFd = readFd;

        if (pthread_create(cl->threads + i, &attr, 
                           threadCb, cxt) != 0)
        {
            LOG_ERROR("pthread_create");

            goto cleanup;
        }

        LOG_WRITE("Thread [%lu] is created on CPU [%lu]\n", i, cpu);
    }

    pthread_attr_destroy(&attr);

    LOG_WRITE("Threads are successfully initialized\n");

    cl->epollfd = epoll_create1(0);
    if (cl->epollfd == -1)
    {
        LOG_ERROR("epoll_create1");

        goto cleanup;
    }

    LOG_WRITE("Epoll is ready\n");

    cl->tsk = NULL;
    cl->connectedSock = -1; 
    cl->responseSock = -1;
    cl->msg.offset = 0;

    LOG_WRITE("Client is successfully initialized\n");

    printf("Client is successfully initialized\n");

    return 0;

cleanup:

    LOG_WRITE("Client initialization is failed\n");

    printf("Client initialization is failed\n");
      
    free(cl->threads);
    free(cl->alignedBuf); 
    pthread_attr_destroy(&attr);

    close(cl->threadRead);
    close(cl->threadWrite);
    close(cl->responseSock);

    size_t j = 0;
    for (; j < i; j++)
        pthread_join(cl->threads[i], NULL);

    close(readFd);
    close(writeFd);

    return -1;
}

void client_free(struct Client *cl)
{
    if (!cl)
        return;

    close(cl->threadRead);
    close(cl->threadWrite);

    for (size_t i = 0; i < cl->nthreads; i++)
        pthread_join(cl->threads[i], NULL);

    free(cl->threads); 

    struct ThreadCxt *cxt = (struct ThreadCxt *) cl->alignedBuf;
    close (cxt->writeFd);
    close (cxt->readFd);

    free(cl->alignedBuf);

    close(cl->epollfd);
    close(cl->connectedSock);
    close(cl->responseSock);
}

void client_usage(const char *progname)
{
    assert(progname);
    fprintf(stderr, "Usage: %s <number_of_threads>\n", progname);
}

int client_connectServer(struct Client *cl)
{
    assert(cl);

    close(cl->responseSock);
    close(cl->connectedSock);

    cl->responseSock = -1;
    cl->connectedSock = -1;

    cl->responseSock = socket(AF_INET, SOCK_DGRAM, 0);
    if (cl->responseSock == -1)
    {
        LOG_ERROR("socket(AF_INET, SOCK_DGRAM, 0)");

        goto cleanup;
    }

    struct sockaddr_in responseAddr =
    {
        .sin_family = AF_INET,
        .sin_port = htons(INTEGRAL_BROADCAST_PORT),
        .sin_addr.s_addr = INADDR_ANY,
    };

    if (bind(cl->responseSock, &responseAddr, sizeof(responseAddr)) == -1)
    {
        LOG_ERROR("bind");

        goto cleanup;
    }

    LOG_WRITE("Waiting for broadcast...\n");

    printf("Waiting for server...\n");

    char msg[INTEGRAL_BROADCAST_MAX_SIZE];
    assert(INTEGRAL_BROADCAST_MAX_SIZE >= sizeof(INTEGRAL_BROADCAST_MSG));
    do
    {
        socklen_t socklen = sizeof(cl->broadcastAddr);
        int retval = recvfrom(cl->responseSock, msg, 
                              sizeof(INTEGRAL_BROADCAST_MSG),
                              0, (struct sockaddr *) &cl->broadcastAddr,
                              &socklen);     
        if (retval == -1)
        {
            LOG_ERROR("recvfrom");

            goto cleanup;
        }

    } while(strncmp(msg, INTEGRAL_BROADCAST_MSG,
                    sizeof(INTEGRAL_BROADCAST_MSG)));

    LOG_WRITE("Received broadcast from %s:%hu\n",
              inet_ntoa(cl->broadcastAddr.sin_addr), 
              ntohs(cl->broadcastAddr.sin_port));

    if (fcntl(cl->responseSock, F_SETFL, O_NONBLOCK) == -1)
    {
        LOG_ERROR("fcntl(%d, F_SETFL, O_NONBLOCK)", cl->responseSock); 

        goto cleanup;
    }

    LOG_WRITE("Response socket is ready: %d\n",
              cl->responseSock);

    cl->listeningAddr.sin_family = AF_INET,
    cl->listeningAddr.sin_port = htons(INTEGRAL_COMMUNICATION_PORT);
    cl->listeningAddr.sin_addr.s_addr = cl->broadcastAddr.sin_addr.s_addr;

    cl->connectedSock = socket(AF_INET, SOCK_STREAM, 0);
    if (cl->connectedSock == -1)
    {
        LOG_ERROR("socket(AF_INET, SOCK_STREAM, 0)");

        goto cleanup;
    }

    if (connect(cl->connectedSock, (struct sockaddr *) &cl->listeningAddr,
                sizeof(cl->listeningAddr)) == -1)
    {
        LOG_ERROR("connect");

        goto cleanup;
    }

    if (fcntl(cl->connectedSock, F_SETFL, O_NONBLOCK) == -1)
    {
        LOG_ERROR("fcntl(F_SETFL, O_NONBLOCK)");

        goto cleanup;
    }

    LOG_WRITE("Successfully connected to server %s:%hu\n",
              inet_ntoa(cl->listeningAddr.sin_addr),
              ntohs(cl->listeningAddr.sin_port));

    printf("Successfully connected to server %s:%hu\n",
           inet_ntoa(cl->listeningAddr.sin_addr),
           ntohs(cl->listeningAddr.sin_port));

    LOG_WRITE("Connected socket is ready: %d\n", cl->connectedSock);

    struct epoll_event ev =
    {
        .events = EPOLLOUT | EPOLLRDHUP,
        .data.fd = cl->responseSock,
    };

    if (epoll_ctl(cl->epollfd, EPOLL_CTL_ADD, ev.data.fd, &ev) == -1)
    {
        LOG_ERROR("epoll_ctl");

        goto cleanup;
    }
        
    ev.events = EPOLLIN | EPOLLRDHUP;
    ev.data.fd = cl->connectedSock;

    if (epoll_ctl(cl->epollfd, EPOLL_CTL_ADD, ev.data.fd, &ev) == -1)
    {
        LOG_ERROR("epoll_ctl");

        goto cleanup;
    }

    cl->lastBroadcast = time(NULL);
    cl->state = CLIENT_STATE_WAITING;
    cl->msg.offset = 0;

    return 0;

cleanup:

    close(cl->connectedSock);
    close(cl->responseSock);

    return -1;
}

int client_processTasks(struct Client *cl)
{
    assert(cl);

    int retcode = 0;

    LOG_WRITE("Waiting for epoll events...\n");

    struct epoll_event events[CLIENT_MAX_EPOLL_EVENTS];
    int readyfds = epoll_wait(cl->epollfd, events, 
                              CLIENT_MAX_EPOLL_EVENTS,
                              CLIENT_EPOLL_TIMEOUT);
    if (readyfds < 0)
    {
        LOG_ERROR("epoll_wait");

        return -1;
    }

    for (int i = 0; i < readyfds; i++)
    {
        retcode = 0;

        LOG_WRITE("New epoll event: [%d;0x%x]\n",
                  events[i].data.fd, events[i].events);

        if (events[i].data.fd == cl->connectedSock)
        {
            if (events[i].events == EPOLLIN)
                retcode = client_recvTask(cl);
            else if (events[i].events == EPOLLOUT)
                retcode = client_sendResult(cl);
            else
                retcode = 1;
        }

        if (events[i].data.fd == cl->responseSock)
        {
            if (events[i].events == EPOLLIN)
                retcode = client_recvBroadcast(cl);
            else if (events[i].events == EPOLLOUT)
                retcode = client_sendResponse(cl);
            else
                retcode = 1;
        }

        if (events[i].data.fd == cl->threadRead)
        {
            if (events[i].events == EPOLLIN)
                retcode = client_recvThreadResult(cl);
            else
                retcode = -1;
        }

        if (events[i].data.fd == cl->threadWrite)
        {
            if (events[i].events == EPOLLOUT)
                retcode = client_sendThreadTask(cl);
            else
                retcode = -1;
        }

        if (retcode > 0)
        {
            LOG_WRITE("Connection to server is lost: error is occurred\n");
            
            printf("Connection to server is lost: error is occurred\n");
        }

        if (retcode)
            return retcode;
    }

    if (cl->state == CLIENT_STATE_WAITING)
    {
        time_t curTime = time(NULL);

        if ((curTime - cl->lastBroadcast) 
            > INTEGRAL_RESPONSE_TIMEOUT)
        {
            LOG_WRITE("Connection to server is lost: response time is out\n");

            printf("Connection to server is lost: response time is out\n");

            return 1;
        }
    }

    return 0;
}

static int client_recvTask(struct Client *cl)
{
    assert(cl);

    int ret = tcpmsg_recv(cl->connectedSock, &cl->msg);
    if (ret)
        return ret;

    if (cl->msg.offset != TCPMSG_SIZE)
        return 0;

    cl->msg.offset = 0;

    LOG_WRITE("New task received: \"%s\"\n", cl->msg.buf);

    cl->tsk = task_readStr(cl->msg.buf);
    if (!cl->tsk)
    {
        LOG_WRITE("Can't parse new task\n");

        return 1;
    }

    if (task_split(cl->tsk, cl->nthreads) == -1)
        return -1;

    if (epoll_ctl(cl->epollfd, EPOLL_CTL_DEL, 
                  cl->connectedSock, NULL) == -1)
    {
        LOG_ERROR("epoll_ctl");

        return -1;
    }

    struct epoll_event ev = 
    {
        .events = EPOLLOUT,
        .data.fd = cl->threadWrite,
    };

    if (epoll_ctl(cl->epollfd, EPOLL_CTL_ADD, ev.data.fd, &ev) == -1)
    {
        LOG_ERROR("epoll_ctl");

        return -1;
    }

    cl->state = CLIENT_STATE_WORKING;

    cl->lastBroadcast = time(NULL);
    cl->result = 0.0;

    return 0;
}

static int client_sendResult(struct Client *cl)
{
    assert(cl);

    int ret = tcpmsg_send(cl->connectedSock, &cl->msg);
    if (ret)
        return ret;

    if (cl->msg.offset != TCPMSG_SIZE)
        return 0;

    cl->msg.offset = 0;

    LOG_WRITE("Result is sent: \"%s\"\n", cl->msg.buf);

    struct epoll_event ev = 
    {
        .events = EPOLLIN | EPOLLRDHUP,
        .data.fd = cl->connectedSock,
    };

    if (epoll_ctl(cl->epollfd, EPOLL_CTL_MOD, ev.data.fd, &ev) == -1)
    {
        LOG_ERROR("epoll_ctl");

        return -1;
    }

    cl->state = CLIENT_STATE_WAITING;

    return 0;
}

static int client_sendThreadTask(struct Client *cl)
{
    assert(cl);

    static struct Task *iter = NULL;

    if (!iter)
        iter = cl->tsk;

    for(; iter; iter = task_next(iter))
    {
        errno = 0;
        int retcode = write(cl->threadWrite, &iter, sizeof(iter));

        if (errno == EAGAIN)
            return 0;

        if (retcode < 0)
        {
            LOG_ERROR("write");

            return -1;
        }
        
        assert(retcode == sizeof(iter));
    }

    LOG_WRITE("Tasks are successfully sent to the threads\n");

    if (epoll_ctl(cl->epollfd, EPOLL_CTL_DEL, 
                  cl->threadWrite, NULL) == -1)
    {
        LOG_ERROR("epoll_ctl");

        return -1;
    }
    
    struct epoll_event ev =
    {
        .events = EPOLLIN,
        .data.fd = cl->threadRead, 
    };

    if (epoll_ctl(cl->epollfd, EPOLL_CTL_ADD,
                  ev.data.fd, &ev) == -1)
    {
        LOG_ERROR("epoll_ctl");

        return -1;
    }

    return 0;
}

static int client_recvThreadResult(struct Client *cl)
{
    assert(cl);

    static size_t nresults = 0; 

    for(; nresults < cl->nthreads; nresults++)
    {
        double threadResult = 0.0;
        errno = 0;
        int retcode = read(cl->threadRead, &threadResult, 
                           sizeof(threadResult));

        if (errno == EAGAIN)
            return 0;

        if (retcode < 0)
        {
            LOG_ERROR("read");

            return -1;
        }

        assert(retcode == sizeof(threadResult));

        cl->result += threadResult;
    }

    LOG_WRITE("Results are successfully received from the threads\n");

    nresults = 0;

    task_deleteList(cl->tsk); 
    
    LOG_WRITE("Task is completed. Result is %lg\n", cl->result);

    if(snprintf(cl->msg.buf, TCPMSG_SIZE, "[%lg]", cl->result) >= 
                (int) TCPMSG_SIZE)
    {
        LOG_WRITE("Can't write result\n");

        return -1;
    }

    if (epoll_ctl(cl->epollfd, EPOLL_CTL_DEL, 
                  cl->threadRead, NULL) == -1)
    {
        LOG_ERROR("epoll_ctl(EPOLL_CTL_DEL)");

        return -1;
    }

    struct epoll_event ev = 
    {
        .events = EPOLLOUT | EPOLLRDHUP,
        .data.fd = cl->connectedSock,
    };

    if (epoll_ctl(cl->epollfd, EPOLL_CTL_ADD,
                  ev.data.fd, &ev) == -1)
    {
        LOG_ERROR("epoll_ctl(EPOLL_CTL_ADD)");

        return -1;
    }

    return 0;
}

static int client_recvBroadcast(struct Client *cl)
{
    assert(cl);

    char msg[INTEGRAL_BROADCAST_MAX_SIZE];
    struct sockaddr_in addr;
    socklen_t socklen = sizeof(addr);

    int retval = recvfrom(cl->responseSock, msg,
                          sizeof(INTEGRAL_BROADCAST_MSG), 0,
                          (struct sockaddr *) &addr, &socklen);
    if (retval == -1)
    {
        LOG_ERROR("recvfrom");

        return -1;
    }

    if (addr.sin_addr.s_addr != cl->broadcastAddr.sin_addr.s_addr ||
        addr.sin_port != cl->broadcastAddr.sin_port)
        return 0;

    if (strncmp(msg, INTEGRAL_BROADCAST_MSG,
                sizeof(INTEGRAL_BROADCAST_MSG)))
        return 0;

    LOG_WRITE("Received broadcast from the server\n");

    struct epoll_event ev = 
    {
        .events = EPOLLOUT,
        .data.fd = cl->responseSock,
    };

    if (epoll_ctl(cl->epollfd, EPOLL_CTL_MOD, ev.data.fd, &ev) == -1)
    {
        LOG_ERROR("epoll_ctl");

        return -1;
    }

    cl->lastBroadcast = time(NULL);
               
    return 0;
}

static int client_sendResponse(struct Client *cl)
{
    assert(cl);

    errno = 0;
    int retval = sendto(cl->responseSock, INTEGRAL_BROADCAST_MSG, 
                        sizeof(INTEGRAL_BROADCAST_MSG), 0,
                        (struct sockaddr *) &cl->broadcastAddr, 
                        sizeof(cl->broadcastAddr));

    if (retval == -1)
    {
        LOG_ERROR("send");

        return -1;
    }

    LOG_WRITE("Sent response to the server\n");

    struct epoll_event ev = 
    {
        .events = EPOLLIN,
        .data.fd = cl->responseSock,
    };

    if (epoll_ctl(cl->epollfd, EPOLL_CTL_MOD, ev.data.fd, &ev) == -1)
    {
        LOG_ERROR("epoll_ctl");

        return -1;
    }
               
    return 0;
}

static void *threadCb(void *cxt)
{
    assert(cxt);

    struct ThreadCxt *tcxt = cxt;
    int retcode = 0;
    do
    {
        struct Task *tsk = NULL;
        retcode = read(tcxt->readFd, &tsk, sizeof(tsk));
        if (retcode <= 0)
            break;

        if (!tsk)
            break;

        assert(retcode == sizeof(tsk));

        tcxt->result = 0;
        double start = task_rangeStart(tsk);
        double end = task_rangeEnd(tsk);
        for (double x = start; x < end; x += INTEGRAL_DX)
            tcxt->result += INTEGRAL_FUNC(x) * INTEGRAL_DX;

        retcode = write(tcxt->writeFd, &tcxt->result, sizeof(tcxt->result));
        if (retcode < 0)
            break;

        assert(retcode == sizeof(tcxt->result));
    }
    while (retcode > 0);

    return (void *) (int64_t) retcode;
}

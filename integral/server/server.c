#define _BSD_SOURCE
#define _GNU_SOURCE
#include "server.h"
#include "config.h"
#include "task.h"
#include "log.h"
#include "tcpmsg.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <error.h>
#include <assert.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>

#define SERVER_EPOLL_TIMEOUT 1000U
#define SERVER_START_CLTABLE_SZ 0x1000U
#define SERVER_MAX_EPOLL_EVENTS 0x100U
#define SERVER_LISTEN_BACKLOG 0x10U

struct Client
{
    struct Client *next;
    struct Client *prev;
    struct Task *tsk;
    struct TcpMsg msg;
    struct sockaddr_in addr; 
    time_t lastResponse;
    int state;
#define CLIENT_STATE_WAITING 1
#define CLIENT_STATE_WORKING 2
    int fd;
};

static int server_acceptConnection(struct Server *sv);
static int server_sendMsg(struct Server *sv, struct Client *cl);
static int server_recvMsg(struct Server *sv, struct Client *cl);
static int server_recvResponse(struct Server *sv);
static int server_distributeTasks(struct Server *sv);
void server_clientFailure(struct Server *sv, struct Client *cl);
void server_checkClients(struct Server *sv);

int server_init(struct Server *sv, uint16_t listenPort, uint16_t broadPort,
                double rangeStart, double rangeEnd)
{
    assert(sv);
    assert(rangeStart < rangeEnd);

    sv->listeningSock = -1; 
    sv->broadcastSock = -1;
    sv->epollfd = -1;
    sv->clTable = NULL;
    sv->tskList = NULL;

    LOG_WRITE("Starting server initialization...\n");

    sv->clTable = calloc(SERVER_START_CLTABLE_SZ, sizeof(*sv->clTable));
    if (!sv->clTable)
    {
        LOG_ERROR("calloc");

        goto cleanup;
    }
    
    sv->clTableSize = SERVER_START_CLTABLE_SZ;

    sv->tskList = task_create(rangeStart, rangeEnd);
    if (!sv->tskList)
        goto cleanup;

    sv->nTasks = INTEGRAL_RANGE_SPLIT_FACTOR;
    if (task_split(sv->tskList, sv->nTasks) == -1)
        goto cleanup;

    LOG_WRITE("Task list is ready\n");

    sv->listeningSock = socket(AF_INET, SOCK_STREAM | O_NONBLOCK, 0);
    if (sv->listeningSock == -1)
    {
        LOG_ERROR("socket(AF_INET, SOCK_STREAM)");

        goto cleanup;
    }

    int opt = 1;
    int retval = setsockopt(sv->listeningSock, SOL_SOCKET, SO_REUSEADDR,
                            &opt, sizeof(opt));
    if (retval == -1)
    {
        LOG_ERROR("setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)");

        goto cleanup;
    }

    struct sockaddr_in addr = 
    {
        .sin_family = AF_INET,
        .sin_port = htons(listenPort),
        .sin_addr.s_addr = INADDR_ANY,
    };

    retval = bind(sv->listeningSock, (struct sockaddr *) &addr,
                  sizeof(addr));
    if (retval == -1)
    {
        LOG_ERROR("bind");

        goto cleanup;
    }

    retval = listen(sv->listeningSock, SERVER_LISTEN_BACKLOG);
    if (retval == -1)
    {
        LOG_ERROR("listen");
        
        goto cleanup;
    }

    LOG_WRITE("Listening socket is ready: %d\n", sv->listeningSock);

    sv->broadcastPort = broadPort;
    sv->broadcastSock = socket(AF_INET, SOCK_DGRAM | O_NONBLOCK, 0);
    if (sv->broadcastSock == -1)
    {
        LOG_ERROR("socket(AF_INET, SOCK_DGRAM)");

        goto cleanup;
    }

    opt = 1;
    retval = setsockopt(sv->broadcastSock, SOL_SOCKET, 
                        SO_BROADCAST, &opt, sizeof(opt));
    if (retval == -1)
    {
        LOG_ERROR("setsockopt(SOL_SOCKET, SO_BROADCAST, 1)");

        goto cleanup;
    }

    addr.sin_port = htons(sv->broadcastPort);

    retval = bind(sv->broadcastSock, (struct sockaddr *) &addr, sizeof(addr));
    if (retval == -1)
    {
        LOG_ERROR("bind");

        goto cleanup;
    }

    LOG_WRITE("Broadcast socket is ready: %d\n", sv->broadcastSock);

    sv->epollfd = epoll_create1(0);
    if (sv->epollfd == -1)
    {
        LOG_ERROR("epoll_create");

        goto cleanup;
    }

    struct epoll_event ev =
    {
        .events = EPOLLIN,
        .data.fd = sv->listeningSock,
    };

    if (epoll_ctl(sv->epollfd, EPOLL_CTL_ADD, sv->listeningSock, &ev) == -1)
    {
        LOG_ERROR("epoll_ctl");

        goto cleanup;
    }

    ev.data.fd = sv->broadcastSock;

    if (epoll_ctl(sv->epollfd, EPOLL_CTL_ADD, sv->broadcastSock, &ev) == -1)
    {
        LOG_ERROR("epoll_ctl");

        goto cleanup;
    }

    LOG_WRITE("Epoll is ready\n");

    sv->clList = NULL;
    sv->result = 0.0;
    sv->nClients = 0;

    LOG_WRITE("Server is successfully initialized\n");

    printf("Server is successfully initialized\n");

    return 0;

cleanup:

    LOG_WRITE("Server initialization failed\n");

    printf("Server initialization failed\n");

    close(sv->listeningSock);
    close(sv->broadcastSock);
    close(sv->epollfd);
    free(sv->clTable);
    task_deleteList(sv->tskList);

    return -1;
}

void server_free(struct Server *sv)
{
    if (!sv)
        return;

    task_deleteList(sv->tskList);
    free(sv->clTable);

    struct Client *iter = sv->clList;

    while (iter)
    {
        void *toDelete = iter;
        iter = iter->next;
        free(toDelete);
    }

    close(sv->epollfd);
    close(sv->listeningSock);
    close(sv->broadcastSock);
}

int server_processClients(struct Server *sv)
{
    assert(sv);

    if (server_distributeTasks(sv) == -1)
        return -1;

    LOG_WRITE("Waiting for epoll event...\n");

    int readyfds = 0;
    struct epoll_event events[SERVER_MAX_EPOLL_EVENTS];
    readyfds = epoll_wait(sv->epollfd, events, SERVER_MAX_EPOLL_EVENTS,
                          SERVER_EPOLL_TIMEOUT);
    if (readyfds == -1)
    {
        LOG_ERROR("epoll_wait");

        return -1;
    }
    
    for (int i = 0; i < readyfds; i++)
    {
        int retcode = 0;

        LOG_WRITE("New epoll event: [%d;0x%x]\n",
                  events[i].data.fd, events[i].events);

        if (events[i].data.fd == sv->listeningSock)
        { 
            do
                retcode = server_acceptConnection(sv);
            while(!retcode);

            if (retcode < 0)
                return retcode;
            
            continue;
        }

        if (events[i].data.fd == sv->broadcastSock)
        {
            do
                retcode = server_recvResponse(sv);
            while(!retcode);

            if (retcode < 0)
                return retcode;

            continue;
        }

        assert((size_t) events[i].data.fd < sv->clTableSize);
        struct Client *cl = sv->clTable[events[i].data.fd];
        assert(cl);

        if (events[i].events == EPOLLOUT)
            retcode = server_sendMsg(sv, cl);
        else if (events[i].events == EPOLLIN)
            retcode = server_recvMsg(sv, cl);
        else
            retcode = 1;

        if (retcode > 0)
            server_clientFailure(sv, cl);

        if (retcode < 0)
           return retcode;
    }        

    server_checkClients(sv);

    return readyfds;
}

static int server_distributeTasks(struct Server *sv)
{
    assert(sv);

    LOG_WRITE("Distributing tasks...\n");

    struct Client *iter = sv->clList;
    for (; iter && sv->tskList; iter = iter->next)
    {
        if (iter->state == CLIENT_STATE_WORKING)
            continue;

        iter->tsk = sv->tskList;
        sv->tskList = task_next(sv->tskList);
        task_unlink(iter->tsk);

        if (task_writeStr(iter->tsk, iter->msg.buf, TCPMSG_SIZE) < 0)
            return -1;
        
        iter->msg.offset = 0;

        struct epoll_event ev = 
        {
            .events = EPOLLOUT | EPOLLRDHUP,
            .data.fd = iter->fd,
        };

        if (epoll_ctl(sv->epollfd, EPOLL_CTL_ADD, ev.data.fd, &ev) == -1)
        {
            LOG_ERROR("epoll_ctl");

            return -1;
        }

        iter->state = CLIENT_STATE_WORKING;
    }

    return 0;
}

static int server_acceptConnection(struct Server *sv)
{
    assert(sv);

    int newSock = -1;
    struct Client *newClient = NULL;

    struct sockaddr_in newClientAddr;
    socklen_t socklen = sizeof(newClientAddr);
    errno = 0;
    newSock = accept4(sv->listeningSock,
                     (struct sockaddr *) &newClientAddr,
                     &socklen, SOCK_NONBLOCK);

    if (newSock < 0 && errno == ECONNABORTED)
        return 0;

    if (newSock < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        return 1;

    if (newSock < 0)
    {
        LOG_ERROR("accept");

        goto cleanup;
    }

    if (sv->clTableSize <= (size_t) newSock)
    {
        sv->clTable = realloc(sv->clTable, 
                              ((size_t) newSock + 1) *
                              sizeof(*sv->clTable));
        if (!sv->clTable)
        {
            LOG_ERROR("realloc");
            
            goto cleanup;
        }

        memset((uint8_t *) sv->clTable + sv->clTableSize,
               0, ((size_t) newSock + 1) - sv->clTableSize);
                
        sv->clTableSize = (size_t) newSock;
    }

    newClient = calloc(1, sizeof(*newClient));
    if (!newClient)
    {
        LOG_ERROR("calloc");

        goto cleanup;
    }

    memcpy(&newClient->addr, &newClientAddr, sizeof(newClientAddr));
    newClient->fd = newSock;

    if (sv->clList)
    {
        newClient->next = sv->clList;
        sv->clList->prev = newClient;
    }
    
    sv->clList = newClient;
    sv->clTable[newSock] = newClient;

    newClient->state = CLIENT_STATE_WAITING;

    newClient->msg.offset = 0;

    newClient->lastResponse = time(NULL);

    sv->nClients++; 

    LOG_WRITE("New connection is accepted from %s:%hu: %d\n",
              inet_ntoa(newClientAddr.sin_addr), 
              ntohs(newClientAddr.sin_port),
              newClient->fd);

    printf("New connection is accepted from %s:%hu\n",
           inet_ntoa(newClientAddr.sin_addr), 
           ntohs(newClientAddr.sin_port));

    return 0;

cleanup:

    close(newSock);
    free(newClient);

    return -1;
}

static int server_recvResponse(struct Server *sv)
{
    assert(sv);
    assert(sizeof(INTEGRAL_BROADCAST_MSG) <= 
           INTEGRAL_BROADCAST_MAX_SIZE);

    char ans[INTEGRAL_BROADCAST_MAX_SIZE];
    struct sockaddr_in addr;
    socklen_t socklen = sizeof(addr);

    errno = 0;
    int retval = recvfrom(sv->broadcastSock, ans, 
                          sizeof(INTEGRAL_BROADCAST_MSG), MSG_NOSIGNAL,
                          (struct sockaddr *) &addr, &socklen);
    
    if (retval < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        return 1;

    if (retval < 0)
    {
        LOG_ERROR("recvfrom");

        return -1;
    }

    assert(retval == sizeof(INTEGRAL_BROADCAST_MSG));

    if (strncmp(ans, INTEGRAL_BROADCAST_MSG,
                sizeof(INTEGRAL_BROADCAST_MSG)))
        return 0;

    LOG_WRITE("Response is received from %s:%hu\n",
              inet_ntoa(addr.sin_addr),
              ntohs(addr.sin_port));

    struct Client *iter = sv->clList;
    for (; iter; iter = iter->next)
        if (iter->addr.sin_addr.s_addr == addr.sin_addr.s_addr &&
            addr.sin_port == sv->broadcastPort)
        {
            iter->lastResponse = time(NULL);

            break;
        }

    return 0;
}

static int server_sendMsg(struct Server *sv, struct Client *cl)
{
    assert(sv);
    assert(cl);

    int ret = tcpmsg_send(cl->fd, &cl->msg);
    if (ret)
        return ret;

    if (cl->msg.offset != TCPMSG_SIZE)
        return 0;

    cl->msg.offset = 0;

    LOG_WRITE("Sent message to %s:%hu: \"%s\"\n",
              inet_ntoa(cl->addr.sin_addr), 
              ntohs(cl->addr.sin_port), cl->msg.buf);

    struct epoll_event ev = 
    {
        .events = EPOLLIN | EPOLLRDHUP,
        .data.fd = cl->fd,
    };

    if (epoll_ctl(sv->epollfd, EPOLL_CTL_MOD, ev.data.fd, &ev) == -1)
    {
        LOG_ERROR("epoll_ctl");

        return -1;
    }

    return 0;
}

static int server_recvMsg(struct Server *sv, struct Client *cl)
{
    assert(sv);
    assert(cl);
    assert(cl->state = CLIENT_STATE_WORKING);

    int ret = tcpmsg_recv(cl->fd, &cl->msg);
    if (ret)
        return ret;

    if (cl->msg.offset != TCPMSG_SIZE)
        return 0;

    cl->msg.offset = 0;

    LOG_WRITE("Received message from %s:%hu: \"%s\"\n",
              inet_ntoa(cl->addr.sin_addr), ntohs(cl->addr.sin_port),
              cl->msg.buf);

    double res = 0.0; 
    if (sscanf(cl->msg.buf, "[%lg]", &res) != 1)
    {
        LOG_WRITE("Can't parse result: \"%s\"\n", cl->msg.buf);

        return 1;
    }

    sv->result += res;

    cl->state = CLIENT_STATE_WAITING;

    if (epoll_ctl(sv->epollfd, EPOLL_CTL_DEL, cl->fd, NULL) == -1)
    {
        LOG_ERROR("epoll_ctl");

        return -1;
    }

    task_delete(cl->tsk);
    cl->tsk = NULL;

    sv->nTasks--;

    cl->lastResponse = time(NULL);

    return 0;
}

void server_checkClients(struct Server *sv)
{
    assert(sv);

    time_t curTime = time(NULL);

    LOG_WRITE("Checking clients...\n");

    struct Client *iter = sv->clList;
    while (iter)
    {
        struct Client *toCheck = iter;
        iter = iter->next;
        if ((curTime - toCheck->lastResponse) >
            INTEGRAL_RESPONSE_TIMEOUT)
            server_clientFailure(sv, toCheck);
    }
}

void server_clientFailure(struct Server *sv, struct Client *cl)
{
    assert(sv);
    assert(cl);

    if (sv->clList == cl)
        sv->clList = cl->next;

    if (cl->next)
        cl->next->prev = cl->prev;
    if (cl->prev)
        cl->prev->next = cl->next;

    if (sv->tskList)
        task_linkBefore(sv->tskList, cl->tsk);
        
    sv->tskList = cl->tsk;

    sv->nClients--;

    close(cl->fd);

    assert(cl->fd < (int) sv->clTableSize);
    sv->clTable[cl->fd] = NULL;

    LOG_WRITE("Client connection %s:%hu is lost, task is canceled\n",
              inet_ntoa(cl->addr.sin_addr), ntohs(cl->addr.sin_port));

    printf("Client connection from %s:%hu is lost\n",
           inet_ntoa(cl->addr.sin_addr), ntohs(cl->addr.sin_port));

    free(cl);
}

int server_sendBroadcast(struct Server *sv)
{
    assert(sv);

    int retval = 0;

    struct ifaddrs *ifaddrs = NULL;
    if (getifaddrs(&ifaddrs) == -1)
    {
        LOG_ERROR("getifaddrs");

        return -1;
    }

    for (struct ifaddrs *iterator = ifaddrs; iterator; 
         iterator = iterator->ifa_next)
    {
        if (!iterator->ifa_broadaddr)
            continue;

        if ((iterator->ifa_flags & IFF_BROADCAST) && 
            iterator->ifa_broadaddr->sa_family == AF_INET)
        {
            struct sockaddr_in 
            *broadcastAddr = (struct sockaddr_in *) iterator->ifa_broadaddr;
            broadcastAddr->sin_port = htons(sv->broadcastPort);
            
            LOG_WRITE("Sending broadcat to %s:%hu: \"%s\"\n", 
                      inet_ntoa(broadcastAddr->sin_addr),
                      ntohs(broadcastAddr->sin_port),
                      INTEGRAL_BROADCAST_MSG);

            retval = sendto(sv->broadcastSock, INTEGRAL_BROADCAST_MSG, 
                            sizeof(INTEGRAL_BROADCAST_MSG),
                            0, (struct sockaddr *) broadcastAddr, 
                            sizeof(*broadcastAddr));

            if (retval == -1 && errno != EAGAIN)
            {
                LOG_ERROR("sendto");

                return -1;
            }
        }
    }

    freeifaddrs(ifaddrs); 

    return 0;
}

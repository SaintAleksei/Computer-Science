#include "server.h"
#include "config.h"
#include "task.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <error.h>
#include <assert.h>
#include <stdlib.h>

#define SERVER_EPOLL_TIMEOUT 1000U
#define SERVER_START_TSKTABLE_SZ 0x1000U
#define SERVER_MAX_EPOLL_EVENTS 0x100U

int server_init(struct Server *sv, uint16_t port, double rangeStart, double rangeEnd)
{
    assert(sv);
    assert(rangeStart < rangeEnd);

    sv->clTskTable = calloc(SERVER_START_TSKTABLE_SZ, sizeof(*sv->clTskTable));
    if (!sv->clTskTable)
        error(EXIT_FAILURE, errno, "calloc");

    sv->clTskTableSz = SERVER_START_TSKTABLE_SZ;

    sv->tskList = task_create(rangeStart, rangeEnd);
    if (!sv->tskList)
        error(EXIT_FAILURE, errno, "task_create");

    if (task_split(sv->tskList, INTEGRAL_RANGE_SPLIT_FACTOR) == -1)
        error(EXIT_FAILURE, errno, "task_split");

    sv->listeningSock = socket(AF_INET, SOCK_STREAM, 0);
    if (sv->listeningSock == -1)
        error(EXIT_FAILURE, errno, "socket");

    struct sockaddr_in listeningAddr = 
    {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = INADDR_ANY,
    };

    int retval = bind(sv->listeningSock, (struct sockaddr *) &listeningAddr,
                      sizeof(listeningAddr));
    if (retval == -1)
        error(EXIT_FAILURE, errno, "bind");

    retval = listen(sv->listeningSock, INTEGRAL_MAX_CLIENTS_COUNT);
    if (retval == -1)
        error(EXIT_FAILURE, errno, "listen");

    sv->epollfd = epoll_create1(0);
    if (sv->epollfd == -1)
        error(EXIT_FAILURE, errno, "epoll_create1");

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = sv->listeningSock;
    if (epoll_ctl(sv->epollfd, EPOLL_CTL_ADD, sv->listeningSock, &ev) == -1)
        error(EXIT_FAILURE, errno, "epoll_ctl");

    sv->result = 0.0;
    sv->nClients = 0;

    return 0;
}

int server_processClients(struct Server *sv)
{
    assert(sv);
    assert(sv->listeningSock);

    int readyfds = 0;
    int toReturn = 0;
    struct epoll_event events[SERVER_MAX_EPOLL_EVENTS];
    readyfds = epoll_wait(sv->epollfd, events, SERVER_MAX_EPOLL_EVENTS,
                          SERVER_EPOLL_TIMEOUT);

    toReturn = readyfds;
    for (int i = 0; i < readyfds; i++)
    {
        if (events[i].data.fd == sv->listeningSock)
        { 
            int newsock = accept(sv->listeningSock, NULL, NULL);
            if (!newsock)
            error(EXIT_FAILURE, errno, "accept");    

            if (fcntl(newsock, F_SETFL, O_NONBLOCK) == -1)
                error(EXIT_FAILURE, errno, "fcntl");

            struct epoll_event ev;
            ev.events = EPOLLOUT | EPOLLRDHUP;
            ev.data.fd = newsock;
            if (epoll_ctl(sv->epollfd, EPOLL_CTL_ADD, newsock, &ev) == -1)
                error(EXIT_FAILURE, errno, "epoll_ctl");

            if (sv->clTskTableSz <= (size_t) newsock)
            {
                sv->clTskTable = realloc(sv->clTskTable, 
                                         sv->clTskTableSz * 
                                         sizeof(*sv->clTskTable));
                if (!sv->clTskTable)
                    error(EXIT_FAILURE, errno, "realloc");
            }
                
            sv->nClients++;
            toReturn--;
        }
        else if (events[i].events & EPOLLOUT)
        {
            struct Task *tsk = sv->tskList;

            if (!tsk)
            {
                close(events[i].data.fd);
                sv->nClients--;
                continue;
            }

            sv->tskList = task_next(sv->tskList);
            task_unlink(tsk);

            sv->clTskTable[events[i].data.fd] = tsk;

            struct MsgTask msg = 
            {
                .rangeStart = task_rangeStart(tsk),
                .rangeEnd = task_rangeEnd(tsk),
            };

            int retval = send(events[i].data.fd,
                              &msg, sizeof(msg), 0);  
            if (retval != sizeof(msg))
                error(EXIT_FAILURE, errno, "send");

            struct epoll_event ev =
            {
                .events = EPOLLIN | EPOLLRDHUP,
                .data.fd = events[i].data.fd,
            };

            if(epoll_ctl(sv->epollfd, EPOLL_CTL_MOD, 
                         events[i].data.fd, &ev) == -1)
                error(EXIT_FAILURE, errno, "epoll_ctl");
        }
        else if (events[i].events & EPOLLIN)
        {
            struct MsgResult msg;
            int retval = recv(events[i].data.fd,
                              &msg, sizeof(msg), 0);

            if (retval != sizeof(msg))
                error(EXIT_FAILURE, errno, "recv");

            sv->result += msg.result;

            struct epoll_event ev =
            {
                .events = EPOLLOUT | EPOLLRDHUP,
                .data.fd = events[i].data.fd,
            };

            if(epoll_ctl(sv->epollfd, EPOLL_CTL_MOD, 
                         events[i].data.fd, &ev) == -1)
                error(EXIT_FAILURE, errno, "epoll_ctl");
        }
        else if (events[i].events & EPOLLRDHUP)
        {
            close(events[i].data.fd); 

            task_link(sv->tskList, sv->clTskTable[events[i].data.fd]);

            sv->nClients--;
        }
    }        

    return toReturn;
}

int server_sendBroadcast(uint16_t port, uint64_t msg)
{
    int retval = 0;

    int broadcastSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP); 
    if (broadcastSock == -1)
        error(EXIT_FAILURE, errno, "socket");

    int opt = 1;
    retval = setsockopt(broadcastSock, SOL_SOCKET, SO_BROADCAST,
                        &opt, sizeof(opt));
    if (retval == -1)
        error(EXIT_FAILURE, errno, "setsockopt");

    struct ifaddrs *ifaddrs = NULL;
    if (getifaddrs(&ifaddrs) == -1)
        error(EXIT_FAILURE, errno, "getifaddrs");

    for (struct ifaddrs *iterator = ifaddrs; iterator; 
         iterator = iterator->ifa_next)
    {
        if (!iterator->ifa_broadaddr)
            continue;

        if (iterator->ifa_flags & IFF_BROADCAST && 
            iterator->ifa_broadaddr->sa_family == AF_INET)
        {
            struct sockaddr_in 
            *broadcastAddr = (struct sockaddr_in *) iterator->ifa_broadaddr;
            broadcastAddr->sin_port = htons(port);
            retval = sendto(broadcastSock, &msg, sizeof(msg),
                            0, (struct sockaddr *) broadcastAddr, 
                            sizeof(*broadcastAddr));

            if (retval == -1)
                error(EXIT_FAILURE, errno, "sendto");
        }
    }

    freeifaddrs(ifaddrs); 
    close(broadcastSock);

    return 0;
}

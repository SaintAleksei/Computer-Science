#include "server.h"
#include "config.h"
#include "task.h"
#include "log.h"
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
#define SERVER_LISTEN_BACKLOG 0x10U

static int server_acceptConnection(struct Server *sv);
static int server_sendTask(struct Server *sv, int sockfd);
static int server_recvResult(struct Server *sv, int sockfd);
static int server_clientFailure(struct Server *sv, int sockfd);

int server_init(struct Server *sv, uint16_t port, double rangeStart, double rangeEnd)
{
    assert(sv);
    assert(rangeStart < rangeEnd);

    sv->clTskTable = calloc(SERVER_START_TSKTABLE_SZ, sizeof(*sv->clTskTable));
    if (!sv->clTskTable)
    {
        LOG_ERROR("calloc(%u, %lu)", SERVER_START_TSKTABLE_SZ, sizeof(*sv->clTskTable));

        return -1;
    }

    sv->clTskTableSz = SERVER_START_TSKTABLE_SZ;

    sv->tskList = task_create(rangeStart, rangeEnd);
    if (!sv->tskList)
        return -1;

    if (task_split(sv->tskList, INTEGRAL_RANGE_SPLIT_FACTOR) == -1)
        return -1;

    sv->listeningSock = socket(AF_INET, SOCK_STREAM, 0);
    if (sv->listeningSock == -1)
    {
        LOG_ERROR("socket(AF_INET, SOCK_STREAM, 0)");

        return -1;
    }

    int opt = 1;
    int retval = setsockopt(sv->listeningSock, SOL_SOCKET, SO_REUSEADDR,
                            &opt, sizeof(opt));
    if (retval == -1)
    {
        LOG_ERROR("setsockopt(%d, SOL_SOCKET, SO_REUSEADDR, 1, -)",
                  sv->listeningSock);

        return -1;
    }

    struct sockaddr_in listeningAddr = 
    {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = INADDR_ANY,
    };

    retval = bind(sv->listeningSock, (struct sockaddr *) &listeningAddr,
                  sizeof(listeningAddr));
    if (retval == -1)
    {
        LOG_ERROR("bind(%d, [AF_INET:%s:%hu], %lu", sv->listeningSock,
                  inet_ntoa(listeningAddr.sin_addr), port, sizeof(listeningAddr));

        return -1;
    }

    retval = listen(sv->listeningSock, SERVER_LISTEN_BACKLOG);
    if (retval == -1)
    {
        LOG_ERROR("listen(%d, %u)", sv->listeningSock,
                  SERVER_LISTEN_BACKLOG);
        
        return -1;
    }

    sv->epollfd = epoll_create1(0);
    if (sv->epollfd == -1)
    {
        LOG_ERROR("epoll_create1(0)");

        return -1;
    }

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = sv->listeningSock;
    if (epoll_ctl(sv->epollfd, EPOLL_CTL_ADD, sv->listeningSock, &ev) == -1)
    {
        LOG_ERROR("epoll_ctl(%d, EPOLL_CTL_ADD, %d, [%d:EPOLLIN])",
                   sv->epollfd, sv->listeningSock, ev.data.fd);
        error(EXIT_FAILURE, errno, "epoll_ctl");
    }

    sv->result = 0.0;
    sv->nClients = 0;

    LOG_WRITE("Server is successfully initialized\n");

    return 0;
}

int server_processClients(struct Server *sv)
{
    assert(sv);
    assert(sv->listeningSock);

    int readyfds = 0;
    struct epoll_event events[SERVER_MAX_EPOLL_EVENTS];
    readyfds = epoll_wait(sv->epollfd, events, SERVER_MAX_EPOLL_EVENTS,
                          SERVER_EPOLL_TIMEOUT);
    if (readyfds == -1)
    {
        LOG_ERROR("epoll_wait(%d, -, %u, %u)",
                  sv->epollfd, SERVER_MAX_EPOLL_EVENTS,
                  SERVER_EPOLL_TIMEOUT);

        return -1;
    }

    for (int i = 0; i < readyfds; i++)
    {
        int retcode = 0;

        if (events[i].data.fd == sv->listeningSock)
        { 
            int ret = server_acceptConnection(sv);
            if (ret < 0)
                return ret;

            if (ret == 0)
                sv->nClients++;
        }
        else if (events[i].events & EPOLLOUT)
            retcode = server_sendTask(sv, events[i].data.fd);
        else if (events[i].events & EPOLLIN)
            retcode = server_recvResult(sv, events[i].data.fd);

        if (retcode < 0)
            return retcode;

        if (events[i].events & EPOLLRDHUP || retcode)
        {
            retcode = server_clientFailure(sv, events[i].data.fd);

            if (retcode < 0)
                return retcode;
        } 
    }        

    return readyfds;
}

static int server_acceptConnection(struct Server *sv)
{
    assert(sv);

    struct sockaddr_in newClient;
    socklen_t socklen;
    errno = 0;
    int newsock = accept(sv->listeningSock,
                         (struct sockaddr *) &newClient,
                         &socklen);
    if (newsock == -1 && errno != ECONNABORTED)
    {
        LOG_ERROR("accept(%d, NULL, NULL)",
                  sv->listeningSock);

        return -1;
    }

    if (errno == ECONNABORTED)
        return 1; 

    LOG_WRITE("New connection is accetped from %s:%hu: %d\n",
              inet_ntoa(newClient.sin_addr), 
              ntohs(newClient.sin_port),
              newsock);

    if (fcntl(newsock, F_SETFL, O_NONBLOCK) == -1)
    {
        LOG_ERROR("fcntl(%d, F_SETFL, O_NONBLOCK)", newsock);

        return -1;
    }

    struct epoll_event ev;
    ev.events = EPOLLOUT | EPOLLRDHUP;
    ev.data.fd = newsock;
    if (epoll_ctl(sv->epollfd, EPOLL_CTL_ADD, newsock, &ev) == -1)
    {
        LOG_ERROR("epoll_ctl(%d, EPOLL_CTL_ADD, %d, "
                  "[%d:EPOLLOUT | EPOLLRDHUP])", 
        sv->epollfd, newsock, ev.data.fd); 

        return -1;
    }

    if (sv->clTskTableSz <= (size_t) newsock)
    {
        sv->clTskTable = realloc(sv->clTskTable, 
                                 sv->clTskTableSz * 
                                 sizeof(*sv->clTskTable));
        if (!sv->clTskTable)
        {
            LOG_ERROR("realloc(-, %lu)",
                      sv->clTskTableSz * sizeof(*sv->clTskTable));

            return -1;
        }
    }

    return 0;
}

static int server_sendTask(struct Server *sv, int sockfd)
{
    assert(sv);
    assert(sockfd >= 0);

    struct Task *tsk = sv->tskList;
    struct epoll_event ev = 
    {
        .data.fd = sockfd,
    };

    if (!tsk)
    {
        int retval = epoll_ctl(sv->epollfd, EPOLL_CTL_DEL,
                               ev.data.fd, &ev);
        if (retval == -1)
        {
            LOG_ERROR("epoll_ctl(%d, EPOLL_CTL_DEL, %d, -)",
                      sv->epollfd, ev.data.fd);

            return -1;
        }

        close(ev.data.fd);

        assert(sv->nClients);
        sv->nClients--;
            
        LOG_WRITE("Client connection %d closed: no available tasks\n",
                  ev.data.fd);

        return 0;
    }

    sv->tskList = task_next(sv->tskList);
    task_unlink(tsk);

    assert((size_t) sockfd < sv->clTskTableSz);
    sv->clTskTable[sockfd] = tsk;

    struct MsgTask msg;
    snprintf(msg.rangeStart, INTEGRAL_STRSIZE, "%*lg",
             INTEGRAL_STRSIZE - 1, task_rangeStart(tsk));
    snprintf(msg.rangeEnd, INTEGRAL_STRSIZE, "%*lg",
             INTEGRAL_STRSIZE - 1, task_rangeEnd(tsk));

    errno = 0;
    int retval = send(ev.data.fd, &msg, sizeof(msg), 0);  
    if (retval == -1 && errno != ECONNRESET)
    {
        LOG_ERROR("send(%d, -, %lu, 0)", ev.data.fd,
                  sizeof(msg));

        return -1;
    }

    if (errno == ECONNRESET || retval != sizeof(msg))
        return 1;

    LOG_WRITE("Task is sent to client %d:\n"
              "\trangeStart: \"%s\";\n"
              "\trangeEnd: \"%s\";\n", 
              ev.data.fd, msg.rangeStart, msg.rangeEnd);
        
    ev.events = EPOLLIN | EPOLLRDHUP;

    if(epoll_ctl(sv->epollfd, EPOLL_CTL_MOD, 
                 ev.data.fd, &ev) == -1)
    {
        LOG_ERROR("epoll_ctl(%d, EPOLL_CTL_MOD, %d, "
                  "[%d:EPOLLIN | EPOLLRDHUP])",
                  sv->epollfd, ev.data.fd, ev.data.fd);

        return -1;
    }

    return 0;
}

static int server_recvResult(struct Server *sv, int sockfd)
{
    assert(sv);
    assert(sockfd >= 0);
    
    struct MsgResult msg;
    errno = 0;
    int retval = recv(sockfd, &msg, sizeof(msg), 0);
    if (retval == -1 && errno != ECONNREFUSED)
    {
        LOG_ERROR("recv(%d, -, %lu, 0)",
                  sockfd, sizeof(msg));

        return -1;
    }

    if (errno == ECONNREFUSED || retval != sizeof(msg))
        return 1;

    LOG_WRITE("Result recieved from client %d: \"%s\"\n", 
              sockfd, msg.result);

    double resultVal = 0.0;
    retval = sscanf(msg.result, "%lg", &resultVal);
    if (retval != 1)
    {
        LOG_WRITE("Can't parse result: \"%s\"\n", msg.result);

        return -1;
    }

    sv->result += resultVal;

    assert((size_t) sockfd < sv->clTskTableSz);
    task_delete(sv->clTskTable[sockfd]);

    LOG_WRITE("Result successfully parsed: %lg\n"
              "Summary result: %lg\n", resultVal, sv->result);

    struct epoll_event ev =
    {
        .events = EPOLLOUT | EPOLLRDHUP,
        .data.fd = sockfd,
    };

    if(epoll_ctl(sv->epollfd, EPOLL_CTL_MOD, 
                 ev.data.fd, &ev) == -1)
    {
        LOG_ERROR("epoll_ctl(%d, EPOLL_CTL_MOD, %d, "
                  "[%d:EPOLLOUT | EPOLLRDHUP])", 
                  sv->epollfd, ev.data.fd, ev.data.fd);

        return -1;
    }

    return 0;
}

static int server_clientFailure(struct Server *sv, int sockfd)
{
    assert(sv);
    assert(sockfd);

    struct epoll_event ev =
    {
        .data.fd = sockfd,
    };

    int retval = epoll_ctl(sv->epollfd, EPOLL_CTL_DEL,
                           sockfd, &ev);
    if (retval == -1)
    {
        LOG_ERROR("epoll_ctl(%d, EPOLL_CTL_DEL, %d, -)",
                  sv->epollfd, sockfd);

        return -1;
    }

    close(sockfd); 

    if (sv->clTskTable[sockfd])
    {
        task_link_before(sv->tskList, sv->clTskTable[sockfd]);
        sv->clTskTable[sockfd] = NULL;
    }

    LOG_WRITE("Client connection %d is lost, task is canceled\n", sockfd);

    sv->nClients--;

    return 0;
}

int server_sendBroadcast(uint16_t port, const char *msg, size_t msgSize)
{
    int retval = 0;

    int broadcastSock = socket(AF_INET, SOCK_DGRAM, 0); 
    if (broadcastSock == -1)
    {
        LOG_ERROR("socket(AF_INET, SOCK_DGRAM, 0)");

        return -1;
    }

    int opt = 1;
    retval = setsockopt(broadcastSock, SOL_SOCKET, SO_BROADCAST,
                        &opt, sizeof(opt));
    if (retval == -1)
    {
        LOG_ERROR("setsockopt(%d, SOL_SOCKET, SO_BROADCAST, 1, %lu)",
                   broadcastSock, sizeof(opt));

        return -1;
    }

    struct ifaddrs *ifaddrs = NULL;
    if (getifaddrs(&ifaddrs) == -1)
    {
        LOG_ERROR("getifaddrs(-)");

        return -1;
    }

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
            
            LOG_WRITE("Sending broadcat \"%s\" to %s:%hu\n", 
                      msg, inet_ntoa(broadcastAddr->sin_addr),
                      ntohs(broadcastAddr->sin_port));

            retval = sendto(broadcastSock, msg, msgSize,
                            0, (struct sockaddr *) broadcastAddr, 
                            sizeof(*broadcastAddr));

            if (retval == -1)
            {
                LOG_ERROR("sendto(%d, \"%s\", %lu, 0, "
                          "[AF_INET:%s:%hu], %lu)", broadcastSock,
                          msg, msgSize, inet_ntoa(broadcastAddr->sin_addr),
                          ntohs(broadcastAddr->sin_port), 
                          sizeof(*broadcastAddr));

                return -1;
            }
        }
    }

    freeifaddrs(ifaddrs); 
    close(broadcastSock);

    return 0;
}

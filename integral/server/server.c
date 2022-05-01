#include "server.h"
#include "config.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <errno.h>
#include <error.h>
#include <assert.h>
#include <stdlib.h>

#define SERVER_BROADCAST_TIMEOUT 1

int server_sendBroadcast(struct sockaddr_in **result, size_t *count)
{
    static struct sockaddr_in addrArr[INTEGRAL_MAX_CLIENTS_COUNT];
    int retval = 0;

    int broadcastSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP); 
    if (broadcastSock == -1)
        error(EXIT_FAILURE, errno, "socket");

    int opt = 1;
    retval = setsockopt(broadcastSock, SOL_SOCKET, SO_BROADCAST,
                        &opt, sizeof(opt));
    if (retval == -1)
        error(EXIT_FAILURE, errno, "setsockopt");

    int answerSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (answerSock == -1)
        error (EXIT_FAILURE, errno, "socket");

    struct sockaddr_in answerAddr = 
    {
        .sin_family = AF_INET,
        .sin_port = htons(INTEGRAL_BROADCAST_PORT),
        .sin_addr.s_addr = INADDR_ANY,
    };

    retval = bind(answerSock, (struct sockaddr *) &answerAddr,
                  sizeof(answerAddr));
    if (retval == -1)
        error(EXIT_FAILURE, errno, "bind"); 

    struct ifaddrs *ifaddrs = NULL;
    if (getifaddrs(&ifaddrs) == -1)
        error(EXIT_FAILURE, errno, "getifaddrs");

    uint64_t broadcastMsg = INTEGRAL_BROADCAST_MSG;
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
            broadcastAddr->sin_port = htons(INTEGRAL_BROADCAST_PORT);
            retval = sendto(broadcastSock, &broadcastMsg, sizeof(broadcastMsg),
                            0, (struct sockaddr *) broadcastAddr, 
                            sizeof(*broadcastAddr));

            if (retval == -1)
                error(EXIT_FAILURE, errno, "sendto");
        }
    }

    freeifaddrs(ifaddrs); 
    close(broadcastSock);

    size_t nanswers = 0;
    do
    {
        if (*count >= INTEGRAL_MAX_CLIENTS_COUNT)
            break;

        struct timeval timeout =
        {
            .tv_sec = SERVER_BROADCAST_TIMEOUT,
        };
        int nfds = answerSock + 1;
        fd_set readfds; 
        FD_ZERO(&readfds);
        FD_SET(answerSock, &readfds);

        retval = select(nfds, &readfds, NULL, NULL, &timeout);
        if (retval == -1)
            error(EXIT_FAILURE, errno, "select");

        if (FD_ISSET(answerSock, &readfds))
        {
            socklen_t addrlen = sizeof(addrArr[0]);
            retval = recvfrom(answerSock, &broadcastMsg, sizeof(broadcastMsg), 0,
                              (struct sockaddr *) (addrArr + nanswers), &addrlen);
            if (retval == -1)
                error(EXIT_FAILURE, errno, "recvfrom");

            if (broadcastMsg == INTEGRAL_BROADCAST_MSG &&
                addrArr[nanswers].sin_port == answerAddr.sin_port)
                nanswers++;
        } 
    }
    while(retval);

    close(answerSock);

    if (result)
        *result = addrArr;    
    if (count)
        *count = nanswers;

    return 0;
}

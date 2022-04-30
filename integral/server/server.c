#include "server.h"
#include "config.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <errno.h>
#include <error.h>
#include <assert.h>
#include <stdlib.h>

#define SERVER_BROADCAST_TIMEOUT 1

int server_sendBroadcast(struct sockaddr_in **result, size_t *count)
{
    assert(result); 
    assert(count);

    static struct sockaddr_in addr_arr[INTEGRAL_MAX_CLIENTS_COUNT];
    int retval = 0;

    int broadcastSock = socket(AF_INET, SOCK_DGRAM, 0); 
    if (broadcastSock == -1)
        error(EXIT_FAILURE, errno, "socket");

    int opt = 1;
    retval = setsockopt(broadcastSock, SOL_SOCKET, SO_BROADCAST,
                        &opt, sizeof(opt));
    if (retval == -1)
        error(EXIT_FAILURE, errno, "setsockopt");

    struct sockaddr_in broadcastAddr =
    {
        .sin_family = AF_INET,
        .sin_port = htons(INTEGRAL_BROADCAST_PORT),
        .sin_addr.s_addr = INADDR_ANY, 
    };

    retval = bind(broadcastSock, (struct sockaddr *) &broadcastAddr, 
                  sizeof(broadcastAddr));
    if (retval == -1)
        error(EXIT_FAILURE, errno, "bind");

    uint64_t broadcastMsg = INTEGRAL_BROADCAST_MSG;
    retval = sendto(broadcastSock, &broadcastMsg, sizeof(broadcastMsg),
                    0, (struct sockaddr *) &broadcastAddr, sizeof(broadcastAddr));
    if (retval == -1)
        error(EXIT_FAILURE, errno, "sendto");

    *count = 0;
    do
    {
        if (*count >= INTEGRAL_MAX_CLIENTS_COUNT)
            break;

        struct timeval timeout =
        {
            .tv_sec = SERVER_BROADCAST_TIMEOUT,
        };
        int nfds = broadcastSock + 1;
        fd_set readfds; 
        FD_ZERO(&readfds);
        FD_SET(broadcastSock, &readfds);

        retval = select(nfds, &readfds, NULL, NULL, &timeout);
        if (retval == -1)
            error(EXIT_FAILURE, errno, "select");

        if (FD_ISSET(broadcastSock, &readfds))
        {
            socklen_t addrlen = sizeof(addr_arr[0]);
            retval = recvfrom(broadcastSock, &broadcastMsg, sizeof(broadcastMsg), 0,
                              (struct sockaddr *) (addr_arr + *count), &addrlen);
            if (retval == -1)
                error(EXIT_FAILURE, errno, "accept");

            if (broadcastMsg == INTEGRAL_BROADCAST_MSG)
                *count++;
        } 
    }
    while(retval);

    close(broadcastSock);

    *result = addr_arr;    

    return 0;
}

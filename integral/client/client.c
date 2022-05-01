#include "client.h"
#include "config.h"
#include <error.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <assert.h>

int client_recvBroadcast(struct sockaddr_in *addr)
{
    assert(addr); 

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1)
        error(EXIT_FAILURE, errno, "socket");
    
    struct sockaddr_in bindAddr = 
    {
        .sin_family = AF_INET,
        .sin_port = htons(INTEGRAL_BROADCAST_PORT),
        .sin_addr.s_addr = INADDR_ANY,
    };

    int retval = bind(sockfd, (struct sockaddr *) &bindAddr, sizeof(bindAddr));
    if (retval == -1)
        error(EXIT_FAILURE, errno, "bind");

    uint64_t msg = 0;
    socklen_t addrlen = sizeof(*addr);
    while (msg != INTEGRAL_BROADCAST_MSG)
    {
        int retval = recvfrom(sockfd, &msg, sizeof(msg), 0,
                              (struct sockaddr *) addr, &addrlen);
        if (retval == -1)
            error(EXIT_FAILURE, errno, "recvfrom");
    } 

    struct sockaddr_in answerAddr = 
    {
        .sin_family = AF_INET,
        .sin_port = htons(INTEGRAL_BROADCAST_PORT),
        .sin_addr.s_addr = addr->sin_addr.s_addr, 
    };

    retval = sendto(sockfd, &msg, sizeof(msg), 0,
                    (struct sockaddr *) &answerAddr, sizeof(answerAddr));
    if (retval == -1)
        error(EXIT_FAILURE, errno, "sendto");

    return 0;
}

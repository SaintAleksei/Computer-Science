#include "client.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>

int main()
{
    struct sockaddr_in addr; 
    client_recvBroadcast(&addr);

    struct in_addr ipAddr = 
    {
        .s_addr = addr.sin_addr.s_addr,
    };

    printf("%s:%hu\n", 
            inet_ntoa(ipAddr), ntohs(addr.sin_port));
}

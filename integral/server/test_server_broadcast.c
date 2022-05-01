#include "server.h"
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>

int main()
{
    struct sockaddr_in *addr = NULL;
    size_t naddrs = 0;
    server_sendBroadcast(&addr, &naddrs);
    
    struct in_addr ipAddr;

    if (naddrs == 0)
        printf("No answer\n");

    for (size_t i = 0; i < naddrs; i++)
    {
        ipAddr.s_addr = addr[i].sin_addr.s_addr;
        printf("%s:%hu\n", inet_ntoa(ipAddr), ntohs((addr + i)->sin_port));
    }
}

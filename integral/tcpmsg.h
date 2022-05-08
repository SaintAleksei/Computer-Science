#ifndef TCPMSG_H_INCLUDED
#define TCPMSG_H_INCLUDED

#include <stddef.h>
#include <stdint.h>

#define TCPMSG_SIZE 0x100U

struct TcpMsg 
{
    char buf[TCPMSG_SIZE];
    size_t offset;
};

int tcpmsg_send(int sockfd, struct TcpMsg *msg);
int tcpmsg_recv(int sockfd, struct TcpMsg *msg);

#endif

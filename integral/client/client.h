#ifndef CLIENT_CLIENT_H_INCLUDED
#define CLIENT_CLIENT_H_INCLUDED

#include <netinet/ip.h>
#include "config.h"
#include "log.h"

struct Client
{
    double rangeStart;
    double rangeEnd;
    double result;
    size_t nthreads;
    int epollfd;
    int connected;
};

int client_init(struct Client *cl, int argc, char **argv);
void client_usage();
int client_recvBroadcast(struct sockaddr_in *servAddr, uint16_t port,
                         const char *msg, size_t msgSize);
int client_connectServer(struct Client *cl, struct sockaddr_in *servAddr, uint16_t port);
int client_recvTask(struct Client *cl);
int client_processTask(struct Client *cl);
int client_sendResult(struct Client *cl);

#endif /* CLIENT_CLIENT_H_INCLUDED */

#ifndef SERVER_SERVER_H_INCLUDED
#define SERVER_SERVER_H_INCLUDED

#include <stdint.h>
#include <stddef.h>
#include <netinet/ip.h>
#include "config.h"
#include "log.h"

struct Server
{
    struct Task *tskList;
    struct Client *clList;
    struct Client **clTable;
    double result;
    size_t nTasks;
    size_t nClients;
    size_t clTableSize;
    int epollfd;
    int broadcastSock;
    int listeningSock;
    uint16_t broadcastPort;
};

int server_init(struct Server *sv, uint16_t listenPort, uint16_t broadPort,
                double rangeStart, double rangeEnd);
void server_free(struct Server *sv);
int server_sendBroadcast(struct Server *sv);
int server_processClients(struct Server *sv);


#endif /* SERVER_SERVER_H_INCLUDED */

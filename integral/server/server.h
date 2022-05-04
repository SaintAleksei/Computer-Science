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
    struct Task **clTskTable;
    double result;
    size_t clTskTableSz;
    size_t nClients;
    int epollfd;
    int listeningSock;
    uint16_t port;
};

int server_init(struct Server *sv, uint16_t port, double rangeStart, double rangeEnd);
int server_sendBroadcast(uint16_t port, const char *msg, size_t msgSize);
int server_startListening(struct Server *sv, uint16_t port);
int server_processClients(struct Server *sv);

#endif /* SERVER_SERVER_H_INCLUDED */

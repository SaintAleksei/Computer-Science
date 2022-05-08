#ifndef CLIENT_CLIENT_H_INCLUDED
#define CLIENT_CLIENT_H_INCLUDED

#include <netinet/ip.h>
#include <pthread.h>
#include "tcpmsg.h"
#include "config.h"
#include "log.h"

struct Client
{
    pthread_t *threads;
    uint8_t *alignedBuf;
    struct Task *tsk;
    struct sockaddr_in listeningAddr;
    struct sockaddr_in broadcastAddr;
    struct TcpMsg msg;
    time_t lastBroadcast;
    double result;
    size_t nthreads;
    size_t alignment;
    int epollfd;
    int responseSock;
    int connectedSock;
    int threadRead;
    int threadWrite;
    int state;
#define CLIENT_STATE_WAITING 1
#define CLIENT_STATE_WORKING 2
};

int client_init(struct Client *cl, int argc, char **argv);
void client_free(struct Client *cl);
void client_usage();
int client_connectServer(struct Client *cl);
int client_processTasks(struct Client *cl);

#endif /* CLIENT_CLIENT_H_INCLUDED */

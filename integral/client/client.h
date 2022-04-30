#ifndef CLIENT_CLIENT_H_INCLUDED
#define CLIENT_CLIENT_H_INCLUDED

#include <netinet/in.h>

int client_recvBroadcast(struct sockaddr_in *addr);

#endif /* CLIENT_CLIENT_H_INCLUDED */

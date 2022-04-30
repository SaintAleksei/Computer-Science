#ifndef SERVER_SERVER_H_INCLUDED
#define SERVER_SERVER_H_INCLUDED

#include <netinet/in.h>

int server_sendBroadcast(struct sockaddr_in **result, size_t *count);

#endif /* SERVER_SERVER_H_INCLUDED */

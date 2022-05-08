#include "tcpmsg.h"
#include "log.h"
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>

int tcpmsg_send(int sockfd, struct TcpMsg *msg)
{
    assert(msg);

    errno = 0;
    int retcode = send(sockfd, msg->buf + msg->offset, 
                       TCPMSG_SIZE - msg->offset, MSG_NOSIGNAL);
    if (errno == ECONNREFUSED || errno == ECONNRESET || errno == EPIPE)
    {
        LOG_ERROR("Can't send message\n");

        return 1;
    }

    if (retcode < 0)
    {
        LOG_ERROR("send");

        return -1;
    }

    msg->offset +=  retcode;

    LOG_WRITE("Sent %d bytes from message \"%s\"\n",
              retcode, msg->buf);

    return 0;
}

int tcpmsg_recv(int sockfd, struct TcpMsg *msg)
{
    assert(msg);

    errno = 0;
    int retcode = recv(sockfd, msg->buf + msg->offset, 
                       TCPMSG_SIZE - msg->offset, 0);
    if (errno == ECONNREFUSED || errno == ECONNRESET)
    {
        LOG_ERROR("Can't receive message\n");

        return 1;
    }

    if (retcode < 0)
    {
        LOG_ERROR("recv");

        return -1;
    }
    
    msg->offset += retcode;

    LOG_WRITE("Recieved %d bytes from message \"%s\"\n",
              retcode, msg->buf);

    return 0;
}

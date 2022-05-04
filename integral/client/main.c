#include "client.h"

LOG_INIT(integral_client);

int main(int argc, char **argv)
{
    struct Client cl;

    int retcode = client_init(&cl, argc, argv);

    if (retcode > 0)
    {
        client_usage(argv[0]);
        
        return retcode;
    }
    else if (retcode < 0)
    {
        fprintf(stderr, "Initialization error. See logs for details\n");

        return retcode;
    }

    while(1)
    {
        struct sockaddr_in svAddr;
        retcode = client_recvBroadcast(&svAddr, INTEGRAL_BROADCAST_PORT,
                                       INTEGRAL_BROADCAST_MSG,
                                       sizeof(INTEGRAL_BROADCAST_MSG));
        if (retcode)
            break;

        retcode = client_connectServer(&cl, &svAddr, 
                                       INTEGRAL_COMMUNICATION_PORT);
        if (retcode)
            break;

        do
        {
            retcode = client_recvTask(&cl);
            if (retcode)
                break;
            
            retcode = client_processTask(&cl);
            if (retcode)
                break;

            retcode = client_sendResult(&cl);
            if (retcode)
                break;
        } while (1);

        if (retcode < 0)
            break;

        while(1);
    }

    fprintf(stderr, "System error is occurred. See logs for details\n");
    
    return retcode;
}

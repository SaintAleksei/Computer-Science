#include "server.h"

LOG_INIT(integral_server);

int main()
{
    struct Server sv;
    int retcode = server_init(&sv, INTEGRAL_COMMUNICATION_PORT,
                              INTEGRAL_RANGE_START,
                              INTEGRAL_RANGE_END);
    if (retcode)
    {
        fprintf(stderr, "Initialization error. See logs for details\n");

        return retcode;
    }

    do
    {
        retcode = server_sendBroadcast(INTEGRAL_BROADCAST_PORT, 
                                       INTEGRAL_BROADCAST_MSG,
                                       sizeof(INTEGRAL_BROADCAST_MSG));
        if (retcode)
        {
            fprintf(stderr, "Can't send broadcast. See logs for details\n");

            return retcode;
        }

        int clProcessed = 0;
        do
            clProcessed = server_processClients(&sv);
        while(clProcessed > 0);

        if (clProcessed < 0)
        {
            fprintf(stderr, "Client processing error. See logs for details\n");

            return retcode;
        }
    }
    while(sv.nClients);
        
    if (!sv.tskList)
        printf("Result: %lg\n", sv.result);
    else
        printf("No clients for computing\n");

    return 0;
}

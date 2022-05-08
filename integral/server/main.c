#include "server.h"

LOG_INIT(integral_server);

int main()
{
    struct Server sv;
    int retcode = server_init(&sv, INTEGRAL_COMMUNICATION_PORT,
                              INTEGRAL_BROADCAST_PORT,
                              INTEGRAL_RANGE_START,
                              INTEGRAL_RANGE_END);
    if (retcode)
        return retcode;

    do
    {
        retcode = server_sendBroadcast(&sv);

        if (retcode)
            break;

        int clProcessed = 0;
        do
            clProcessed = server_processClients(&sv);
        while(clProcessed > 0 && sv.nTasks);

        if (clProcessed < 0)
            break;

        if(!sv.nTasks)
        {
            printf("Result: %lg\n", sv.result);
            break;
        }
        else if (!sv.nClients)
        {
            printf("No clients for computing\n");
            break;
        } 
    }
    while(1);

    server_free(&sv);

    return 0;
}

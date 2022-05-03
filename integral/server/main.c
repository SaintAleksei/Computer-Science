#include "server.h"

LOG_INIT;

int main()
{
    struct Server sv;
    server_init(&sv, INTEGRAL_COMMUNICATION_PORT,
                INTEGRAL_RANGE_START,
                INTEGRAL_RANGE_END);

    do
    {
        server_sendBroadcast(INTEGRAL_BROADCAST_PORT, 
                             INTEGRAL_BROADCAST_MSG);

        int clProcessed = 0;
        do
            clProcessed = server_processClients(&sv);
        while(clProcessed);
    }
    while(sv.nClients);
        
    if (!sv.tskList)
        printf("Result: %lg\n", sv.result);
    else
        printf("No clients for computing\n");

    return 0;
}

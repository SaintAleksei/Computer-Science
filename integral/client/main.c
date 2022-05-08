#include "client.h"

LOG_INIT(integral_client);

int main(int argc, char **argv)
{
    struct Client cl;

    int retcode = client_init(&cl, argc, argv);

    if (retcode > 0)
        client_usage(argv[0]);

    if (retcode)
        return retcode;

    while(1)
    {
        retcode = client_connectServer(&cl);
        if (retcode)
            break;

        do
            retcode = client_processTasks(&cl);
        while(!retcode);

        if (retcode < 0)
            break;
    }

    fprintf(stderr, "System error is occurred. See logs for details\n");

    client_free(&cl);
    
    return retcode;
}

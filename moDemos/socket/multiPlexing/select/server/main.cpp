#include <stdio.h>
#include <signal.h>
#include <string.h>

#include "server.h"

int main(int argc, char **argv)
{
    if(argc != 2)
    {
        fprintf(stderr, "Usage : %s TCP/UDP\n", argv[0]);
        return -1;
    }
    
    //set SIGPIPE TO IGNORE
    signal(SIGPIPE, SIG_IGN);

    //init server
    string servIp("127.0.0.1");
    unsigned int servPort(8082);
    SOCKET_MODE mode = SOCKET_MODE_TCP;
    if(0 != strcmp(argv[1], "TCP"))
    {
        mode = SOCKET_MODE_UDP;
    }
    Server serv(servIp, servPort, mode);
    int ret = serv.initSocket();
    if(0 != ret)
    {
        fprintf(stderr, "init socket for server failed! ret = %d\n", ret);
        return -2;
    }

    serv.run();

    return 0;
}

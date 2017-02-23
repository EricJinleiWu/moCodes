#include <stdio.h>
#include <signal.h>
#include <string.h>

#include "server.h"

#define SERVER_IP   "127.0.0.1"
#define SERVER_PORT 8082

/*
    1.Init an object;
    2.send msg;
    3.free memory;
*/
int main(int argc, char **argv)
{
    if(argc != 2)
    {
        fprintf(stderr, "Usage : %s runMode\n", argv[0]);
        fprintf(stderr, "Usage : Currently, we support runModes : [TCP, UDP, UNIX]\n");
        return -1;
    }

    RUNNING_MODE runMode = TCP_MODE;
    if(0 == strcmp(argv[1], "TCP"))
        runMode = TCP_MODE;
    else if(0 == strcmp(argv[1], "UDP"))
        runMode = UDP_MODE;
    else if(0 == strcmp(argv[1], "UNIX"))
        runMode = UNIX_MODE;
    else
    {
        fprintf(stderr, "Input runMode = [%s], donot in our runMode range [TCP, UDP, UNIX]!\n",
            argv[1]);
        return -2;
    }
    
    //signal SIGPIPE will be set to SIG_IGN, 
    //If donot do this operation, when server donot run, client do send will return SIGPIPE, and this signal
    //  will kill our process directly.
    signal(SIGPIPE, SIG_IGN);

    string ip(SERVER_IP);
    unsigned int port = SERVER_PORT;
    Server *pServer = new Server(runMode, ip, port);

    pServer->dump();

    pServer->run();

    delete pServer;

    return 0;
}

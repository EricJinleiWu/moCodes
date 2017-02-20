#include <stdio.h>
#include <signal.h>

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
    //signal SIGPIPE will be set to SIG_IGN, 
    //If donot do this operation, when server donot run, client do send will return SIGPIPE, and this signal
    //  will kill our process directly.
    signal(SIGPIPE, SIG_IGN);

    string ip(SERVER_IP);
    unsigned int port = SERVER_PORT;
    Server *pServer = new Server(ip, port);

    pServer->dump();

    pServer->run();

    delete pServer;

    return 0;
}

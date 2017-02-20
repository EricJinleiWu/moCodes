#include <stdio.h>
#include <signal.h>

#include "client.h"

#define CLIENT_IP   "127.0.0.1"
#define CLIENT_PORT 8081

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

    string ip(CLIENT_IP);
    unsigned int port = CLIENT_PORT;
    Client *pClient = new Client(ip, port);

    pClient->dump();

    string reqMsg("testFromClient");
    string respMsg;
    int ret = pClient->sendMsg(reqMsg, respMsg);
    if(0 != ret)
    {
        printf("pClient->sendMsg failed! ret = %d, reqMsg = [%s]\n", ret, reqMsg.c_str());
    }
    else
    {
        printf("pClient->sendMsg succeed! reqMsg = [%s], respMsg = [%s]\n", reqMsg.c_str(), respMsg.c_str());
    }

    delete pClient;

    return 0;
}

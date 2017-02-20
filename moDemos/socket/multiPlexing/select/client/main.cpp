#include <stdio.h>
#include <signal.h>
#include <string.h>

#include "client.h"

int main(int argc, char **argv)
{
    if(argc != 2)
    {
        printf("Usage : %s TCP/UDP\n", argv[0]);
        return -1;
    }

    signal(SIGPIPE, SIG_IGN);

    string clientIp("127.0.0.1");
    SOCKET_MODE mode = SOCKET_MODE_TCP;
    if(0 != strcmp(argv[1], "TCP"))
        mode = SOCKET_MODE_UDP;
    Client client(clientIp, mode);

    client.dump();

    int ret = client.initSocket();
    if(ret != 0)
    {
        fprintf(stderr, "initSocket failed! ret = %d\n", ret);
        return 0;
    }

    sleep(1);
    
#if 1
    int round = 1;
    while(true)
    {
        printf("Round %d...\n", round);

        string msg0("Send to server.");
        ret = client.sendMsg(msg0);
        if(0 != ret)
        {
            fprintf(stderr, "Round %d, sendMsg failed! ret = %d. will exit now.\n",
                round, ret);
            break;
        }
        printf("sendMsg succeed!\n");
//        sleep(10);
//        
//        if(mode == SOCKET_MODE_TCP)
//        {
//            string msg1;
//            ret = client.recvMsg(msg1);
//            if(0 != ret)
//            {
//                fprintf(stderr, "Round %d, recvMsg failed! ret = %d, will exit now.\n",
//                    round, ret);
//                break;
//            }
//            printf("recvMsg succeed! msg is [%s]\n", msg1.c_str());
//        }
        
        round++;
        sleep(1);
    }
#endif
    return 0;
}

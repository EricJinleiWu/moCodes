#include <stdio.h>
#include <signal.h>

#include "server.h"

int main(int argc, char **argv)
{
    signal(SIGPIPE, SIG_IGN);

    Server serv;
    serv.Run();

    return 0;
}

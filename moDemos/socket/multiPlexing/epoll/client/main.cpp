#include <stdio.h>
#include <signal.h>

#include "client.h"

int main(int argc, char **argv)
{
    signal(SIGPIPE, SIG_IGN);

    Client client;
    client.Run();

    return 0;
}

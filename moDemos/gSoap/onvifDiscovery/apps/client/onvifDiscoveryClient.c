#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

#include "calc.nsmap"
#include "soapH.h"
#include "soapStub.h"

#define dbgDebug(format, ...) printf("[%s, %d--debug] : "format, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define dbgError(format, ...) printf("[%s, %d--error] : "format, __FUNCTION__, __LINE__, ##__VA_ARGS__)

int main(int argc, char** argv)
{
    if(argc != 3)
    {
        dbgError("Usage : %s serverIp serverPort\n", argv[0]);
        return -1;
    }
    srand((unsigned int)time(NULL));

    struct soap calcSoap;
    soap_init(&calcSoap);
    soap_set_namespaces(&calcSoap, namespaces);
    char serverUrl[32] = {0x00};
    sprintf(serverUrl, "%s:%s", argv[1], argv[2]);
    double a = rand() % 1000;
    double b = rand() % 1000;
    double c = 0;
    int ret = soap_call_ns2__add(&calcSoap, serverUrl, NULL, a, b, &c);
    dbgDebug("soap_call_ns2__add: ret=%d, serverUrl=[%s], a=%f, b=%f, c=%f\n", ret, serverUrl, a, b, c);

    soap_end(&calcSoap);
    soap_done(&calcSoap);
    
    return 0;
}

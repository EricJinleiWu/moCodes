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

int ns2__add(struct soap* pSoap, double a, double b, double * result)
{
    *result = a + b;
    return 0;
}

int ns2__sub(struct soap* pSoap, double a, double b, double * result)
{
    *result = a - b;
    return 0;
}

int ns2__mul(struct soap* pSoap, double a, double b, double * result)
{
    *result = a * b;
    return 0;
}

int ns2__div(struct soap* pSoap, double a, double b, double * result)
{
    *result = a / b;
    return 0;
}

int ns2__pow(struct soap* pSoap, double a, double b, double * result)
{
    *result = (int)a % (int)b;
    return 0;
}


int main(int argc, char** argv)
{
    if(argc != 2)
    {
        dbgError("Usage : %s serverPort\n", argv[0]);
        return -1;
    }

    struct soap calcSoap;
    soap_init(&calcSoap);
    soap_set_namespaces(&calcSoap, namespaces);

    int ret = soap_bind(&calcSoap, NULL, atoi(argv[1]), 100);
    if(ret < 0)
    {
        soap_print_fault(&calcSoap, stderr);
        return -2;
    }
    dbgDebug("soap_bind return %d\n", ret);

    while(1)
    {
        ret = soap_accept(&calcSoap);
        if(ret < 0)
        {
            soap_print_fault(&calcSoap, stderr);
            break;
        }
        dbgDebug("accept a client.\n");
        soap_serve(&calcSoap);
        soap_end(&calcSoap);
    }
    
    soap_done(&calcSoap);
    
    return 0;
}


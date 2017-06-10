#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "moLogger.h"
#include "moUtils.h"
#include "cftProt.h"

static void tst_CftProt(void)
{
#define REQUESTS_NUM    1

    cftProtInit();

    CFT_REQUEST_INFO request[REQUESTS_NUM];
    
    int i = 0;
    for(; i < REQUESTS_NUM; i++)
    {
        memset(&request[i], 0x00, sizeof(CFT_REQUEST_INFO));
    }

    CFT_REQUEST_INFO requestEncrypt;
    memset(&requestEncrypt, 0x00, sizeof(CFT_REQUEST_INFO));
    requestEncrypt.algoNo = MO_CFT_ALGO_RC4;
    requestEncrypt.cryptMethod = MOCRYPT_METHOD_ENCRYPT;
    strcpy(requestEncrypt.srcFilepath, "./tst.txt");
    strcpy(requestEncrypt.passwd, "abcd");
    cftProtAddRequest(&requestEncrypt, 1);

    sleep(5);
    
    CFT_REQUEST_INFO requestDecrypt;
    memset(&requestDecrypt, 0x00, sizeof(CFT_REQUEST_INFO));
    requestDecrypt.algoNo = MO_CFT_ALGO_RC4;
    requestDecrypt.cryptMethod = MOCRYPT_METHOD_DECRYPT;
    strcpy(requestDecrypt.srcFilepath, "./tst.txt.cftCipher");
    strcpy(requestDecrypt.passwd, "abcd");
    cftProtAddRequest(&requestDecrypt, 1);

    sleep(5);

    cftProtUnInit();
    

}

int main(int argc, char **argv)
{
    moLoggerInit("./");

    tst_CftProt();

    moLoggerUnInit();

    return 0;
}







































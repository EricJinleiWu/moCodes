#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include "moLogger.h"
#include "moUtils.h"
#include "moCrypt.h"
#include "moNvrClient.h"

static int pFuncCallback(const int length, const unsigned char * data)
{
    return 0;
}

int main(int argc, char **argv)
{
    moLoggerInit("./");

    char * confFilepath = "./moNvrCli.ini";
    int ret = moNvrCli_init(confFilepath, pFuncCallback);
    if(ret != 0)
    {
        printf("moNvrCli_init failed, ret = %d\n", ret);
        goto EXIT;
    }
    printf("moNvrCli_init succeed\n");

    ret = moNvrCli_startPlay();
    if(ret != 0)
    {
        printf("moNvrCli_startPlay failed, ret = %d\n", ret);
        moNvrCli_unInit();
        goto EXIT;
    }
    printf("moNvrCli_startPlay succeed\n");

    sleep(3);
    
    ret = moNvrCli_stopPlay();
    if(ret != 0)
    {
        printf("moNvrCli_stopPlay failed, ret = %d\n", ret);
        moNvrCli_unInit();
        goto EXIT;
    }
    printf("moNvrCli_stopPlay succeed\n");
    
    moNvrCli_unInit();

EXIT:
    moLoggerUnInit();
    
    return 0;
}



































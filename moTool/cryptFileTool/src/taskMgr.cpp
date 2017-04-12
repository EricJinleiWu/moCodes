#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "taskMgr.h"


TaskMgr::TaskMgr()
{
    ;
}

TaskMgr::TaskMgr(const TaskMgr & other)
{
}

TaskMgr::~TaskMgr()
{
    ;
}

int TaskMgr::doCrypt(const MOCFT_TASKINFO & info)
{
    //dump the @info
    moLoggerDebug(MOCFT_LOGGER_MODULE_NAME, "info.algo=%d, info.method=%d, info.srcfilepath=[%s], info.dstfilepath=[%s]\n", 
        info.algo, info.method, info.srcfilepath, info.dstfilepath);

    int ret = 0;
    //start do crypt
    switch(info.algo)
    {
        case MOCFT_ALGO_RC4:
            MOCRYPT_RC4_FILEINFO cryptInfo;
            memset(&cryptInfo, 0x00, sizeof(MOCRYPT_RC4_FILEINFO));
            strcpy(cryptInfo.pSrcFilepath, info.srcfilepath);
            strcpy(cryptInfo.pDstFilepath, info.dstfilepath);
            memcpy(cryptInfo.pKey, info.key, MOCRYPT_RC4_KEY_MAX_LEN);
            cryptInfo.keyLen = info.keyLen;
            cryptInfo.pCallback = cpServerSetProg;
            ret = moCrypt_RC4_cryptFile(&cryptInfo);
            break;
            
        case MOCFT_ALGO_BASE64:
            ret = moCrypt_BASE64_File(info.method, info.srcfilepath, info.dstfilepath, cpServerSetProg);
            break;
            
        default:
            moLoggerError(MOCFT_LOGGER_MODULE_NAME, "input algo=%d, donot a valid one!\n", info.algo);
            ret = -2;
            break;
    }

    return ret;
}


//TaskMgrSingleton::TaskMgrSingleton()
//{}

//TaskMgrSingleton::TaskMgrSingleton(CpServerSocket  * pServ, pSetProgCallbackFunc pFunc)
//{
//    instance = new TaskMgr(pServ, pFunc);
//}

//TaskMgrSingleton::~TaskMgrSingleton()
//{}


//TaskMgr * TaskMgrSingleton::GetInstance()
//{
//    return instance;
//}



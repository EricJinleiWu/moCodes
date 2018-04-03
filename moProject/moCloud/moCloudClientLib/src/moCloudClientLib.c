#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <semaphore.h>

#include "moCloudUtilsTypes.h"
#include "moCloudUtils.h"
#include "moCloudUtilsCheck.h"
#include "moCloudUtilsCrypt.h"
#include "moCloudClientLib.h"
#include "cliUtils.h"
#include "cliData.h"

/*
    The infomation being read from cfg file;
*/
typedef struct
{
    char clientIp[MOCLOUD_IP_ADDR_MAXLEN];
    int clientCtrlPort;
    int clientDataPort;
    
    char serverIp[MOCLOUD_IP_ADDR_MAXLEN];
    int serverPort;

    char dwldInfoFile[MOCLOUD_FILEPATH_MAXLEN];

    //Other params being added here;
}CFG_INFO;

/*
    dwldTasks.file
*/
typedef struct
{
    char mark[MOCLOUD_MARK_MAXLEN]; //"MOCLOUD_DWLD"

    char isUsing;   //0, not used; 1, used now;
    
    MOCLOUDCLIENT_DWLD_INFO dwldInfo;

    char res[512];  //reserved

    unsigned char checkSum;
}DWLD_FILE_UNIT_INFO;

typedef struct
{
    MOCLOUDCLIENT_DWLD_INFO dwldInfo;
    int offset; //the offset in dwldTasks.file;
    int fileId;
}DWLD_TASK_INFO;

typedef struct __DWLD_TASK_INFO_NODE
{
    DWLD_TASK_INFO dwldTaskInfo;
    struct __DWLD_TASK_INFO_NODE * next;
}DWLD_TASK_INFO_NODE;

typedef enum
{
    INIT_STATE_NO,  //Donot init
    INIT_STATE_YES  //inited yet;
}INIT_STATE;

typedef enum
{
    CLI_STATE_IDLE, //program init failed, or cannot connect to server;
    CLI_STATE_NOT_LOGIN,    //program running, but donot logIn
    CLI_STATE_LOGIN //USER has login, can get filelist, and do other operations
}CLI_STATE;

static CFG_INFO gCfgInfo;
static pthread_mutex_t gMutex = PTHREAD_MUTEX_INITIALIZER;
static INIT_STATE gIsInited = INIT_STATE_NO;
static int gCtrlSockId = MOCLOUD_INVALID_SOCKID;
static MOCLOUD_CRYPT_INFO gCryptInfo;
static pthread_t gHeartbeatThrId = MOCLOUD_INVALID_THR_ID;

static pthread_mutex_t gFilelistMutex = PTHREAD_MUTEX_INITIALIZER;
static CLI_STATE gCliState = CLI_STATE_IDLE;
static MOCLOUD_BASIC_FILEINFO_NODE * gpFilelist[MOCLOUD_FILETYPE_ALL];
static char gCurLoginUsername[MOCLOUD_USERNAME_MAXLEN] = {0x00};
static char gCurLoginPassword[MOCLOUD_PASSWD_MAXLEN] = {0x00};
static sem_t gRefreshFilelistSem;
static pthread_t gRefreshFilelistThId = MOCLOUD_INVALID_THR_ID;
static int gStopRefreshFilelistThr = 0; //when this value set to 1, then stop refresh filelist thread
//To assure, which type of filelist should be refreshed this time
//being set by heartbeat thread
static MOCLOUD_FILETYPE gRefreshFilelistType = MOCLOUD_FILETYPE_MAX;    

static FILE * gDwldInfofileFd = NULL;
static DWLD_TASK_INFO_NODE * gDwldTasksInfoListHead = NULL;
static pthread_mutex_t gDwldTasksMutex = PTHREAD_MUTEX_INITIALIZER;

#define RSA_PRIV_KEYLEN 128
static const char RSA_PRIV_KEY[RSA_PRIV_KEYLEN] = {
    //TODO, real value needed
    0x00
};

#define CFG_ITEM_SECTION_SERVER         "servInfo"
#define CFG_ITEM_ATTR_SERVIP            "servIp"
#define CFG_ITEM_ATTR_SERVPORT          "servPort"

#define CFG_ITEM_SECTION_CLIENT         "cliInfo"
#define CFG_ITEM_ATTR_CLIIP             "cliIp"
#define CFG_ITEM_ATTR_CLI_DATA_PORT     "cliDataPort"
#define CFG_ITEM_ATTR_CLI_CTRL_PORT     "cliCtrlPort"

#define CFG_ITEM_SECTION_DWLDINFO       "dwldInfo"
#define CFG_ITEM_ATTR_DWLDINFOFILE      "infoFile"

#define CTRL_CMD_TIMEOUT    3   //To each ctrl request, timeout in 3 seconds

/*
    1.read cfg file @pCfgFilepath, parse it;
    2.save params to @pCfgInfo;

    config file in format : 
        ;server info
        [servInfo]
        servIp = 127.0.0.1
        ;servIp = 192.168.88.226
        servPort = 4111
        ;client info
        [cliInfo]
        cliIp = 127.0.0.1
        cliDataPort = 3720
        cliCtrlPort = 3721
*/
static int getCfgInfo(CFG_INFO * pCfgInfo, const char * pCfgFilepath)
{
    if(NULL == pCfgFilepath || NULL == pCfgInfo)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "Input param is NULL!\n");
        return -1;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "config file path is [%s]\n", pCfgFilepath);

    MOUTILS_INI_SECTION_INFO_NODE * pIniParser = moUtils_Ini_Init(pCfgFilepath);
    if(NULL == pIniParser)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "moUtils_Ini_Init failed! cfgFilepath=[%s]\n", pCfgFilepath);
        return -2;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "moUtils_Ini_Init succeed.\n");
    moUtils_Ini_DumpAllInfo(pIniParser);

    memset(pCfgInfo, 0x00, sizeof(CFG_INFO));
    
    moUtils_Ini_GetAttrValue(CFG_ITEM_SECTION_SERVER, CFG_ITEM_ATTR_SERVIP, pCfgInfo->serverIp, pIniParser);
    if(!isValidIpAddr(pCfgInfo->serverIp))
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "pCfgInfo->serverIp=[%s], donot a valid ip address\n", pCfgInfo->serverIp);
        moUtils_Ini_UnInit(pIniParser);
        return -3;
    }
    
    moUtils_Ini_GetAttrValue(CFG_ITEM_SECTION_CLIENT, CFG_ITEM_ATTR_CLIIP, pCfgInfo->clientIp, pIniParser);
    if(!isValidIpAddr(pCfgInfo->clientIp))
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "pCfgInfo->clientIp=[%s], donot a valid ip address\n", pCfgInfo->clientIp);
        moUtils_Ini_UnInit(pIniParser);
        return -4;
    }
    
    char tmp[16] = {0x00};
    memset(tmp, 0x00, 16);
    moUtils_Ini_GetAttrValue(CFG_ITEM_SECTION_SERVER, CFG_ITEM_ATTR_SERVPORT, tmp, pIniParser);
    pCfgInfo->serverPort = atoi(tmp);
    if(!isValidUserPort(pCfgInfo->serverPort))
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "pCfgInfo->serverPort=%d, donot a valid user process port!\n", pCfgInfo->serverPort);
        moUtils_Ini_UnInit(pIniParser);
        return -5;
    }
    
    memset(tmp, 0x00, 16);
    moUtils_Ini_GetAttrValue(CFG_ITEM_SECTION_CLIENT, CFG_ITEM_ATTR_CLI_CTRL_PORT, tmp, pIniParser);
    pCfgInfo->clientCtrlPort = atoi(tmp);
    if(!isValidUserPort(pCfgInfo->clientCtrlPort))
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "pCfgInfo->clientCtrlPort=%d, donot a valid user process port!\n", pCfgInfo->clientCtrlPort);
        moUtils_Ini_UnInit(pIniParser);
        return -6;
    }

    memset(tmp, 0x00, 16);
    moUtils_Ini_GetAttrValue(CFG_ITEM_SECTION_CLIENT, CFG_ITEM_ATTR_CLI_DATA_PORT, tmp, pIniParser);
    pCfgInfo->clientDataPort = atoi(tmp);
    if(!isValidUserPort(pCfgInfo->clientDataPort))
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "pCfgInfo->clientDataPort=%d, donot a valid user process port!\n", pCfgInfo->clientDataPort);
        moUtils_Ini_UnInit(pIniParser);
        return -7;
    }

    memset(tmp, 0x00, 16);
    moUtils_Ini_GetAttrValue(CFG_ITEM_SECTION_DWLDINFO, CFG_ITEM_ATTR_DWLDINFOFILE, tmp, pIniParser);
    strncpy(pCfgInfo->dwldInfoFile, tmp, MOCLOUD_FILEPATH_MAXLEN);
    pCfgInfo->dwldInfoFile[MOCLOUD_FILEPATH_MAXLEN - 1] = 0x00;

    moUtils_Ini_UnInit(pIniParser);
    return 0;
}

static void dumpCfgInfo(const CFG_INFO cfgInfo)
{
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "Start dump the config info now.\n");

    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
        "Server info : ip=[%s], port=%d\n", cfgInfo.serverIp, cfgInfo.serverPort);
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
        "Client info : ip=[%s], ctrlPort=%d, dataPort=%d\n",
        cfgInfo.clientIp, cfgInfo.clientCtrlPort, cfgInfo.clientDataPort);
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
        "Dwld info : dwldInfoFile=[%s]\n", 
        cfgInfo.dwldInfoFile);
    
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "Dump the config info end now.\n");
}

static int freeLocalFilelist(const MOCLOUD_FILETYPE type)
{
    if(type >= MOCLOUD_FILETYPE_MAX)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "Input type=%d, invalid!\n", type);
        return -1;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "type=%d\n", type);
    
    int curType = 0;
    for(curType = (type == MOCLOUD_FILETYPE_ALL ? MOCLOUD_FILETYPE_VIDEO : type);
        curType < (type == MOCLOUD_FILETYPE_ALL ? MOCLOUD_FILETYPE_ALL : type + 1);
        curType *= 2)
    {
        MOCLOUD_BASIC_FILEINFO_NODE * pCurNode = gpFilelist[curType];
        while(pCurNode != NULL)
        {
            MOCLOUD_BASIC_FILEINFO_NODE * pNextNode = pCurNode->next;
            free(pCurNode);
            pCurNode = pNextNode;
        }
        gpFilelist[curType] = NULL;
    }

    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "Free resource ok.\n");
    return 0;
}

static int genRequest(MOCLOUD_CTRL_REQUEST * pReq, 
    const MOCLOUD_REQUEST_TYPE type, const MOCLOUD_CMDID cmdId, const int bodyLen)
{
    if(NULL == pReq)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "Input param is NULL.\n");
        return -1;
    }
    
    memset(pReq, 0x00, sizeof(MOCLOUD_CTRL_REQUEST));
    strncpy(pReq->mark, MOCLOUD_MARK_CLIENT, MOCLOUD_MARK_MAXLEN);
    pReq->mark[MOCLOUD_MARK_MAXLEN - 1] = 0x00;
    pReq->isNeedResp = type;
    pReq->cmdId = cmdId;
    pReq->bodyLen = bodyLen;
    moCloudUtilsCheck_crcGetValue(
        MOUTILS_CHECK_CRCMETHOD_32, (char * )pReq, 
        sizeof(MOCLOUD_CTRL_REQUEST) - sizeof(MOUTILS_CHECK_CRCVALUE),
        &pReq->crc32);
    return 0;
}

/*
    To the request @pReq we input, get its cipher value;
    The crypt info will be get from @gCryptInfo;
    @pCipherReq will be malloced some memory in this function! 
        its original value must be NULL!
*/
static int getCipherRequestInfo(const MOCLOUD_CTRL_REQUEST * pReq, 
    char ** ppCipherReq, int * pCipherLen)
{
    if(pReq == NULL || *ppCipherReq != NULL || pCipherLen == NULL)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "Input param is invalid!\n");
        return -1;
    }
    
    int ret = moCloudUtilsCrypt_getCipherTxtLen(
        gCryptInfo.cryptAlgoNo, sizeof(MOCLOUD_CTRL_REQUEST), pCipherLen);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "moCloudUtilsCrypt_getCipherTxtLen failed! ret=%d\n", ret);
        return -2;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "get cipher text length succeed.\n");
    
    *ppCipherReq = (char *)malloc(sizeof(char ) * (*pCipherLen));
    if(NULL == *ppCipherReq)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "malloc for cipher request failed! length=%d, errno=%d, desc=[%s]\n",
            (*pCipherLen), errno, strerror(errno));
        return -3;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
        "plainLen=%d, cipherLen=%d, malloc for cipher reqeust ok.\n", 
        sizeof(MOCLOUD_CTRL_REQUEST), (*pCipherLen));
    ret = moCloudUtilsCrypt_doCrypt(MOCRYPT_METHOD_ENCRYPT,
        gCryptInfo, (char *)pReq, sizeof(MOCLOUD_CTRL_REQUEST),
        *ppCipherReq, pCipherLen);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "moCloudUtilsCrypt_doCrypt failed! ret=%d\n", ret);
        free(*ppCipherReq);
        *ppCipherReq = NULL;
        return -4;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "moCloudUtilsCrypt_doCrypt succeed.\n");
    return 0;
}

/*
    1.Read a response header from server which in plain text format;
    2.do decrypt;
    3.do check;
    
    @pRespHeader is the header value;
    @cmdId is the cmd id of this request;

    return : 
        0 : succeed;
        -1: moCloudUtilsCrypt_getCipherTxtLen failed;
        -2: malloc failed;
        -3: In timeout, donot get right response;
        -4: Function "select" failed;
*/
static int getRespHeader(MOCLOUD_CTRL_RESPONSE *pRespHeader, const MOCLOUD_CMDID cmdId)
{
    if(pRespHeader == NULL)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "Input param is NULL!\n");
        return -1;
    }
    
    int cipherLength = 0;
    int ret = moCloudUtilsCrypt_getCipherTxtLen(
        gCryptInfo.cryptAlgoNo, sizeof(MOCLOUD_CTRL_RESPONSE), &cipherLength);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "moCloudUtilsCrypt_getCipherTxtLen failed! ret=%d\n", ret);
        return -2;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
        "get cipher text length succeed, length=%d.\n", cipherLength);

    char * pCipherRespHeader = NULL;
    pCipherRespHeader = (char *)malloc(sizeof(char) * cipherLength);
    if(pCipherRespHeader == NULL)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "Malloc failed!" \
            "length=%d, errno=%d, desc=[%s]\n", cipherLength, errno, strerror(errno));
        return -3;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "malloc succeed.\n");

    int cnt = 0;
    while(1)
    {
        if(cnt >= CTRL_CMD_TIMEOUT)
        {
            moLoggerWarn(MOCLOUD_MODULE_LOGGER_NAME, 
                "cnt=%d, CTRL_CMD_TIMEOUT=%d, should break now.\n", cnt, CTRL_CMD_TIMEOUT);
            ret = -4;
            break;
        }
        
        fd_set rFds;
        FD_ZERO(&rFds);
        FD_SET(gCtrlSockId, &rFds);
        int loopInterval = 1;   //seconds
        struct timeval timeout;
        timeout.tv_sec = loopInterval;
        timeout.tv_usec = 0;
        ret = select(gCtrlSockId + 1, &rFds, NULL, NULL, &timeout);
        if(ret < 0)
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "select failed! " \
                "errno=%d, desc=[%s]\n", errno, strerror(errno));
            ret = -4;
            break;
        }
        else if(ret == 0)
        {
            moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "Select timeout.\n");
            cnt += loopInterval;
            continue;
        }
        else
        {
            moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "start recv a response now.\n");
            ret = readn(gCtrlSockId, pCipherRespHeader, cipherLength);
            if(ret != cipherLength)
            {
                moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
                    "read response header in cipher failed! readLen=%d, realLength=%d\n",
                    ret, cipherLength);
                cnt += loopInterval;
                continue;
            }
            moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "read response header in cipher succeed.\n");

            MOCLOUD_CTRL_RESPONSE tmp;
            int tmpLen = 0;
            memset(&tmp, 0x00, sizeof(MOCLOUD_CTRL_RESPONSE));
            ret = moCloudUtilsCrypt_doCrypt(MOCRYPT_METHOD_DECRYPT, gCryptInfo, pCipherRespHeader,
                cipherLength, (char *)&tmp, &tmpLen);
            if(ret < 0)
            {
                moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "moCloudUtilsCrypt_doCrypt failed! ret=%d\n", ret);
                cnt += loopInterval;
                continue;
            }
            moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "moCloudUtilsCrypt_doCrypt succeed.\n");

            if(tmp.cmdId != cmdId || 0 != strcmp(tmp.mark, MOCLOUD_MARK_SERVER) || 
                0 != moCloudUtilsCheck_crcCheckValue(MOUTILS_CHECK_CRCMETHOD_32,
                    (char *)&tmp, sizeof(MOCLOUD_CTRL_RESPONSE) - sizeof(MOUTILS_CHECK_CRCVALUE),
                    tmp.crc32))
            {
                moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "Check this response header failed! " \
                    "cmdId=%d, MOCLOUD_CMDID_X=%d, mark=[%s], MOCLOUD_MARK_SERVER=[%s]\n", 
                    tmp.cmdId, cmdId, tmp.mark, MOCLOUD_MARK_SERVER);
                cnt += loopInterval;
                continue;
            }
            moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "Check this response header succeed!\n");

            memcpy(pRespHeader, &tmp, sizeof(MOCLOUD_CTRL_RESPONSE));
            
            ret = 0;
            break;
        }
    }

    free(pCipherRespHeader);
    pCipherRespHeader = NULL;
    
    return ret;
}

/*
    disconnect to server;
    just send byebye to server;
    
    1.generate request;
    2.do encrypt;
    3.send to server, wait for response header;
        3.1.tiemout;
        3.2.cmdId;
        3.3.do decrypt, then check it;
*/
static int disConnectToServer()
{
    //1.generate request
    MOCLOUD_CTRL_REQUEST request;
    genRequest(&request, MOCLOUD_REQUEST_TYPE_NEED_RESPONSE, MOCLOUD_CMDID_BYEBYE, 0);
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "genRequest succeed.\n");

    //2.do encrypt
    int cipherLen = 0;
    char * pCipherReq = NULL;
    int ret = getCipherRequestInfo(&request, &pCipherReq, &cipherLen);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "getCipherRequestInfo failed! ret=%d\n", ret);
        return -1;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "getCipherRequestInfo succeed.\n");

    //3.1.send to server
    ret = writen(gCtrlSockId, pCipherReq, cipherLen);
    if(ret != cipherLen)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "Write request to server failed!\n");
        free(pCipherReq);
        pCipherReq = NULL;
        return -2;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "Write request to server succeed.\n");
    free(pCipherReq);
    pCipherReq = NULL;
    //3.2.wait for response header, decrypt, check response
    MOCLOUD_CTRL_RESPONSE resp;
    memset(&resp, 0x00, sizeof(MOCLOUD_CTRL_RESPONSE));
    ret = getRespHeader(&resp, MOCLOUD_CMDID_BYEBYE);
    if(ret != 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "getRespHeader failed! ret=%d\n", ret);
        return -3;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "getRespHeader succeed.\n");

    moLoggerInfo(MOCLOUD_MODULE_LOGGER_NAME, "disConnectToServer succeed.\n");
    return 0;
}


static int genKeyAgreeRequest(MOCLOUD_KEYAGREE_REQUEST * pReq)
{
    if(NULL == pReq)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "Input param is NULL.\n");
        return -1;
    }

    strncpy(pReq->mark, MOCLOUD_MARK_CLIENT, MOCLOUD_MARK_MAXLEN);
    pReq->mark[MOCLOUD_MARK_MAXLEN - 1] = 0x00;
    pReq->cmdId = MOCLOUD_CMDID_KEYAGREE;
    moCloudUtilsCheck_crcGetValue(
        MOUTILS_CHECK_CRCMETHOD_32, (char *)pReq, 
        sizeof(MOCLOUD_KEYAGREE_REQUEST) - sizeof(MOUTILS_CHECK_CRCVALUE), 
        &pReq->crc32);
    
    return 0;
}

static int getCipherKeyAgreeReq(const MOCLOUD_KEYAGREE_REQUEST * pReq, char ** ppCipherReq,
    int * pCipherLen)
{
    if(pReq == NULL || *ppCipherReq != NULL || pCipherLen == NULL)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "Input param is invalid!\n");
        return -1;
    }
    
    int ret = moCloudUtilsCrypt_getCipherTxtLen(
        MOCLOUD_CRYPT_ALGO_RSA, sizeof(MOCLOUD_KEYAGREE_REQUEST), pCipherLen);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "moCloudUtilsCrypt_getCipherTxtLen failed! ret=%d\n", ret);
        return -2;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "get cipher text length succeed.\n");
    
    *ppCipherReq = (char *)malloc(sizeof(char ) * (*pCipherLen));
    if(NULL == *ppCipherReq)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "malloc for cipher request failed! length=%d, errno=%d, desc=[%s]\n",
            (*pCipherLen), errno, strerror(errno));
        return -3;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
        "plainLen=%d, cipherLen=%d, malloc for cipher reqeust ok.\n", 
        sizeof(MOCLOUD_KEYAGREE_REQUEST), (*pCipherLen));

    MOCLOUD_CRYPT_INFO cryptInfo;
    memset(&cryptInfo, 0x00, sizeof(MOCLOUD_CRYPT_INFO));
    cryptInfo.cryptAlgoNo = MOCLOUD_CRYPT_ALGO_RSA;
    memcpy(cryptInfo.cryptKey.rsaKey, RSA_PRIV_KEY, RSA_PRIV_KEYLEN);
    cryptInfo.keyLen = RSA_PRIV_KEYLEN;
    ret = moCloudUtilsCrypt_doCrypt(MOCRYPT_METHOD_ENCRYPT,
        cryptInfo, (char *)pReq, sizeof(MOCLOUD_KEYAGREE_REQUEST),
        *ppCipherReq, pCipherLen);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "moCloudUtilsCrypt_doCrypt failed! ret=%d\n", ret);
        free(*ppCipherReq);
        *ppCipherReq = NULL;
        return -4;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "moCloudUtilsCrypt_doCrypt succeed.\n");
    return 0;
}

static int getKeyAgreeResp(MOCLOUD_KEYAGREE_RESPONSE * pResp)
{
    if(pResp == NULL)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "Input param is NULL!\n");
        return -1;
    }
    
    int cipherLength = 0;
    int ret = moCloudUtilsCrypt_getCipherTxtLen(
        gCryptInfo.cryptAlgoNo, sizeof(MOCLOUD_KEYAGREE_RESPONSE), &cipherLength);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "moCloudUtilsCrypt_getCipherTxtLen failed! ret=%d\n", ret);
        return -2;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
        "get cipher text length succeed, length=%d.\n", cipherLength);

    char * pCipherResp = NULL;
    pCipherResp = (char *)malloc(sizeof(char) * cipherLength);
    if(pCipherResp == NULL)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "Malloc failed!" \
            "length=%d, errno=%d, desc=[%s]\n", cipherLength, errno, strerror(errno));
        return -3;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "malloc succeed.\n");

    int cnt = 0;
    while(1)
    {
        if(cnt >= CTRL_CMD_TIMEOUT)
        {
            moLoggerWarn(MOCLOUD_MODULE_LOGGER_NAME, 
                "cnt=%d, CTRL_CMD_TIMEOUT=%d, should break now.\n", cnt, CTRL_CMD_TIMEOUT);
            ret = -4;
            break;
        }
        
        fd_set rFds;
        FD_ZERO(&rFds);
        FD_SET(gCtrlSockId, &rFds);
        int loopInterval = 1;   //seconds
        struct timeval timeout;
        timeout.tv_sec = loopInterval;
        timeout.tv_usec = 0;
        ret = select(gCtrlSockId + 1, &rFds, NULL, NULL, &timeout);
        if(ret < 0)
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "select failed! " \
                "errno=%d, desc=[%s]\n", errno, strerror(errno));
            ret = -4;
            break;
        }
        else if(ret == 0)
        {
            moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "Select timeout.\n");
            cnt += loopInterval;
            continue;
        }
        else
        {
            moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "start recv a response now.\n");
            ret = readn(gCtrlSockId, pCipherResp, cipherLength);
            if(ret != cipherLength)
            {
                moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
                    "read response header in cipher failed! readLen=%d, realLength=%d\n",
                    ret, cipherLength);
                cnt += loopInterval;
                continue;
            }
            moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "read response header in cipher succeed.\n");

            MOCLOUD_KEYAGREE_RESPONSE tmp;
            int tmpLen = 0;
            memset(&tmp, 0x00, sizeof(MOCLOUD_CTRL_RESPONSE));
            ret = moCloudUtilsCrypt_doCrypt(MOCRYPT_METHOD_DECRYPT, gCryptInfo, pCipherResp,
                cipherLength, (char *)&tmp, &tmpLen);
            if(ret < 0)
            {
                moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "moCloudUtilsCrypt_doCrypt failed! ret=%d\n", ret);
                cnt += loopInterval;
                continue;
            }
            moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "moCloudUtilsCrypt_doCrypt succeed.\n");

            if(tmp.cmdId != MOCLOUD_CMDID_KEYAGREE || 0 != strcmp(tmp.mark, MOCLOUD_MARK_SERVER) || 
                0 != moCloudUtilsCheck_crcCheckValue(MOUTILS_CHECK_CRCMETHOD_32,
                    (char *)&tmp, sizeof(MOCLOUD_CTRL_RESPONSE) - sizeof(MOUTILS_CHECK_CRCVALUE),
                    tmp.crc32))
            {
                moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "Check this response header failed! " \
                    "cmdId=%d, MOCLOUD_CMDID_KEYAGREE=%d, mark=[%s], MOCLOUD_MARK_SERVER=[%s],", 
                    tmp.cmdId, MOCLOUD_CMDID_KEYAGREE, tmp.mark, MOCLOUD_MARK_SERVER);
                cnt += loopInterval;
                continue;
            }
            moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "Check this response header succeed!\n");

            memcpy(pResp, &tmp, sizeof(MOCLOUD_CTRL_RESPONSE));
            
            ret = 0;
            break;
        }
    }

    free(pCipherResp);
    pCipherResp = NULL;
    
    return ret;

}

/*
    do Key agree to server;

    1.generate request;
    2.do encrypt;
    3.send to server, wait for response;
        3.1.timeout;
        3.2.cmdId;
    4.get response, do decrypt;
    5.get keyInfo, save it to @gCryptInfo;
*/
static int doKeyAgree()
{
    //1.gen key agree request
    MOCLOUD_KEYAGREE_REQUEST req;
    memset(&req, 0x00, sizeof(MOCLOUD_KEYAGREE_REQUEST));
    int ret = genKeyAgreeRequest(&req);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "genKeyAgreeRequest failed! ret=%d\n", ret);
        return -1;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "genKeyAgreeRequest succeed.\n");

    //2.do encrypt
    char * pCipherReq = NULL;
    int cipherLen = 0;
    ret = getCipherKeyAgreeReq(&req, &pCipherReq, &cipherLen);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "getCipherKeyAgreeReq failed! ret=%d\n", ret);
        return -2;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "getCipherKeyAgreeReq succeed, cipherLen=%d.\n", cipherLen);

    //3.send to server
    ret = writen(gCtrlSockId, pCipherReq, cipherLen);
    if(ret != cipherLen)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "send keyagree request failed! writeLen=%d, cipherLen=%d\n", 
            ret, cipherLen);
        free(pCipherReq);
        pCipherReq = NULL;
        return -3;
    }
    free(pCipherReq);
    pCipherReq = NULL;
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "send keyagree request succeed.\n");

    //4.get response from server for keyagree
    MOCLOUD_KEYAGREE_RESPONSE resp;
    memset(&resp, 0x00, sizeof(MOCLOUD_KEYAGREE_RESPONSE));
    ret = getKeyAgreeResp(&resp);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "getKeyAgreeResp failed! ret=%d\n", ret);
        return -3;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "getKeyAgreeResp succeed.\n");

    //4.set Crypt info to local
    memcpy(&gCryptInfo, &resp.info, sizeof(MOCLOUD_CRYPT_INFO));
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "save crypt info to local memory succeed.\n");
    
    return 0;
}

/*
    Get the response body of a request;
    Body donot in cipher, so just read it is OK;
    @pRespBody is NULL when input, and it will be malloced in this function, 
        so its important to you to free it after using;
*/
static int getRespBody(char ** ppRespBody, const int bodyLen)
{
    if(*ppRespBody != NULL)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "Input param is invalid.\n");
        return -1;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "bodyLen=%d\n", bodyLen);

    fd_set rFds;
    FD_ZERO(&rFds);
    FD_SET(gCtrlSockId, &rFds);
    struct timeval timeout;
    timeout.tv_sec = CTRL_CMD_TIMEOUT;
    timeout.tv_usec = 0;
    int ret = select(gCtrlSockId + 1, &rFds, NULL, NULL, &timeout);
    if(ret <= 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "select ret=%d, donot get any response body! errno=%d, desc=[%s]\n", 
            ret, errno, strerror(errno));
        return -1;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "select OK, get response now.\n");

    *ppRespBody = (char * )malloc(sizeof(char) * bodyLen);
    if(NULL == *ppRespBody)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "malloc failed! errno=%d, desc=[%s]\n",
            errno, strerror(errno));
        return -2;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "malloc OK, get value now.\n");

    int readLen = readn(gCtrlSockId, *ppRespBody, bodyLen);
    if(readLen != bodyLen)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "readn failed! " \
            "readLen=%d, bodyLen=%d\n", readLen, bodyLen);
        free(*ppRespBody);
        *ppRespBody = NULL;
        return -3;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "read response body succeed.\n");

    return 0;
}

/*
    Set the response body to our local memory which being in list;
*/
static int setFilelistValue(const char * pRespBody, 
    const int bodyLen, const MOCLOUD_FILETYPE type)
{
    if(type < MOCLOUD_FILETYPE_VIDEO || type >= MOCLOUD_FILETYPE_MAX || NULL == pRespBody)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "Input param donot valid. type=%d\n", type);
        return -1;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "type=%d\n", type);

    freeLocalFilelist(type);

    int ret = 0;
    MOCLOUD_BASIC_FILEINFO * pCurInfo = NULL;
    int readLen = 0;
    while(readLen + sizeof(MOCLOUD_BASIC_FILEINFO) <= bodyLen)
    {
        pCurInfo = (MOCLOUD_BASIC_FILEINFO *)(pRespBody + readLen);
        if(pCurInfo->key.filetype >= MOCLOUD_FILETYPE_MAX || 
            (type != MOCLOUD_FILETYPE_ALL && pCurInfo->key.filetype != type))
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
                "pCurInfo.filetype=%d, MOCLOUD_FILETYPE_MAX=%d, input type=%d, invalid!!\n", 
                pCurInfo->key.filetype, MOCLOUD_FILETYPE_MAX, type);
        }
        else
        {
            MOCLOUD_BASIC_FILEINFO_NODE * pNewNode = NULL;
            pNewNode = (MOCLOUD_BASIC_FILEINFO_NODE *)malloc(sizeof(MOCLOUD_BASIC_FILEINFO_NODE) * 1);
            if(pNewNode == NULL)
            {
                //malloc failed, donot do last info, just return an error, and clear resources
                moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
                    "malloc failed! errno=%d, desc=[%s]\n", errno, strerror(errno));
                ret = -2;
                break;
            }
            moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "malloc new node succeed.\n");

            memcpy(&pNewNode->info, pCurInfo, sizeof(MOCLOUD_BASIC_FILEINFO));
            pNewNode->next = NULL;

            if(gpFilelist[pCurInfo->key.filetype] == NULL)
            {
                gpFilelist[pCurInfo->key.filetype] = pNewNode;
            }
            else
            {
                pNewNode->next = gpFilelist[pCurInfo->key.filetype];
                gpFilelist[pCurInfo->key.filetype] = pNewNode;
            }
        }

        readLen += sizeof(MOCLOUD_BASIC_FILEINFO);
    }

    if(ret < 0)
    {
        //should free resources being malloced because some error ocurred
        freeLocalFilelist(type);
    }

    return ret;
}

/*
    This function to refresh @gFileList
*/
static int refreshFilelist(const MOCLOUD_FILETYPE type)
{
    if(type >= MOCLOUD_FILETYPE_MAX)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "type=%d, invalid one! MOCLOUD_FILETYPE_MAX=%d\n",
            type, MOCLOUD_FILETYPE_MAX);
        return -1;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "type=%d\n", type);

    pthread_mutex_lock(&gFilelistMutex);

    if(gCliState != CLI_STATE_LOGIN)
    {
        moLoggerInfo(MOCLOUD_MODULE_LOGGER_NAME, 
            "gCliState=%d, donot CLI_STATE_LOGIN(%d), will not refresh file list now.\n",
            gCliState, CLI_STATE_LOGIN);
        pthread_mutex_unlock(&gFilelistMutex);
        return -2;
    }

    MOCLOUD_CTRL_REQUEST req;
    memset(&req, 0x00, sizeof(MOCLOUD_CTRL_REQUEST));
    int ret = genRequest(&req, MOCLOUD_REQUEST_TYPE_NEED_RESPONSE, MOCLOUD_CMDID_GETFILELIST, 0);  
    //when getFileList, additionalInfo.cInfo[0] means the type
    req.addInfo.cInfo[0] = type;
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "genRequest for getFilelist failed! ret=%d\n", ret);
        pthread_mutex_unlock(&gFilelistMutex);
        return -3;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "genRequest for getFilelist succeed.\n");

    char * pCipherReq = NULL;
    int cipherLen = 0;
    ret = getCipherRequestInfo(&req, &pCipherReq, &cipherLen);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "getCipherRequestInfo for getFilelist failed! ret=%d\n", ret);
        pthread_mutex_unlock(&gFilelistMutex);
        return -4;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "getCipherRequestInfo for getFilelist succeed.\n");

    ret = writen(gCtrlSockId, pCipherReq, cipherLen);
    if(ret != cipherLen)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "Send cipher request to server failed. " \
            "ret=%d, cipherLen=%d\n", ret, cipherLen);
        free(pCipherReq);
        pCipherReq = NULL;
        pthread_mutex_unlock(&gFilelistMutex);
        return -5;
    }

    MOCLOUD_CTRL_RESPONSE respHeader;
    memset(&respHeader, 0x00, sizeof(MOCLOUD_CTRL_RESPONSE));
    ret = getRespHeader(&respHeader, MOCLOUD_CMDID_GETFILELIST);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "getRespHeader for getFilelist failed! ret=%d\n", ret);
        free(pCipherReq);
        pCipherReq = NULL;
        pthread_mutex_unlock(&gFilelistMutex);
        return -5;
    }
    free(pCipherReq);
    pCipherReq = NULL;
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
        "getRespHeader for getFilelist succeed, bodyLen=%d.\n",
        respHeader.bodyLen);

    if(respHeader.bodyLen != 0)
    {
        //recv body
        char * pRespBody = NULL;
        ret = getRespBody(&pRespBody, respHeader.bodyLen);
        if(ret < 0)
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "getRespBody for getFilelist failed! ret=%d\n", ret);
            pthread_mutex_unlock(&gFilelistMutex);
            return -6;
        }
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "getRespBody for getFilelist succeed.\n");
        
        //save respBody to local memory @gpFilelist
        ret = setFilelistValue(pRespBody, respHeader.bodyLen, type);
        if(ret < 0)
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "setFilelistValue failed! ret=%d\n", ret);
            free(pRespBody);
            pRespBody = NULL;
            pthread_mutex_unlock(&gFilelistMutex);
            return -7;
        }
        free(pRespBody);
        pRespBody = NULL;
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "setFilelistValue succeed.\n");
    }

    pthread_mutex_unlock(&gFilelistMutex);
    return 0;
}

/*
    To heartbeat response, we should check its value, and do what we should do if needed;
*/
static int doHeartbeatResp(const MOCLOUD_CTRL_RESPONSE resp)
{
    int ret = 0;
    switch(resp.ret)
    {
        case MOCLOUD_HEARTBEAT_RET_OK:
            moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "Heartbeat OK, file list donot change.\n");
            ret = 0;
            break;
        case MOCLOUD_HEARTBEAT_RET_FILELIST_CHANGED:
            moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "Heartbeat ok, and file list being changed!\n");
            gRefreshFilelistType = resp.addInfo.cInfo[0];
            sem_post(&gRefreshFilelistSem);
            break;
        default:
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "heartbeat return %d, its wrong value!\n", resp.ret);
            ret = -2;
            break;
    }
    return ret;
}

static int sendHeartbeat()
{
    MOCLOUD_CTRL_REQUEST request;
    memset(&request, 0x00, sizeof(MOCLOUD_CTRL_REQUEST));
    int ret = genRequest(&request, MOCLOUD_REQUEST_TYPE_NEED_RESPONSE, MOCLOUD_CMDID_HEARTBEAT, 0);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "genRequest for heartbeat request failed! ret=%d\n", ret);
        return -1;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "genRequest for heartbeat request succeed.\n");

    char * pCipherReq = NULL;
    int cipherLen = 0;
    ret = getCipherRequestInfo(&request, &pCipherReq, &cipherLen);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "getCipherRequestInfo for heartbeat request failed! ret=%d\n", ret);
        return -2;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "getCipherRequestInfo for heartbeat request succeed.\n");

    ret = writen(gCtrlSockId, pCipherReq, cipherLen);
    if(ret != cipherLen)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "send heartbeat request to server failed! ret=%d, cipherLen=%d\n", 
            ret, cipherLen);
        free(pCipherReq);
        pCipherReq = NULL;
        return -3;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "send heartbeat request to server succeed.\n");
    free(pCipherReq);
    pCipherReq = NULL;
    return 0;
}

/*
    Thread, send heartbeat to server looply;
*/
static void * heartbeatThr(void * args)
{
    args = args;
    sigset_t set;
    int ret = threadRegisterSignal(&set);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "threadRegisterSignal failed! ret=%d\n", ret);
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "threadRegisterSignal succeed.\n");

    fd_set rFds;
    FD_ZERO(&rFds);
    FD_SET(gCtrlSockId, &rFds);
    
    struct timespec tmInterval;
    tmInterval.tv_sec = MOCLOUD_HEARTBEAT_INTEVAL;
    tmInterval.tv_nsec = 0;  
    while(1)
    {
        ret = pselect(0, NULL, NULL, NULL, &tmInterval, &set);
        if(ret < 0 && errno == EINTR)
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "pselect recv a signal to exit.\n");
            break;
        }
        else if(ret == 0)
        {
            moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "pselect timeout\n");
        }
        else
        {
            moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "pselect return ret=%d, errno=%d, desc=[%s]\n",
                ret, errno, strerror(errno));
        }

        //send heartbeat
        ret = sendHeartbeat();
        if(ret < 0)
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "sendHeartbeat failed! ret=%d\n", ret);
            continue;
        }
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "sendHeartbeat succeed.\n");

        //recv response
        MOCLOUD_CTRL_RESPONSE resp;
        memset(&resp, 0x00, sizeof(MOCLOUD_CTRL_RESPONSE));
        ret = getRespHeader(&resp, MOCLOUD_CMDID_HEARTBEAT);
        if(ret < 0)
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "getResponse for heartbeat failed!ret=%d\n", ret);
            continue;
        }
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "getRespHeader for heartbeat succeed.\n");

        //check heartbeat result, to make sure we need refresh file list or not
        ret = doHeartbeatResp(resp);
        if(ret < 0)
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "doHeartbeatResp failed!ret=%d\n", ret);
            continue;
        }
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "doHeartbeatResp succeed.\n");
    }
    
    pthread_exit(NULL);
}

/*
    Heartbeat thread being started here;
*/
static int startHeartbeatThr()
{
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    int ret = startThread(&gHeartbeatThrId, &attr, heartbeatThr, NULL);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "startThread failed! ret=%d\n", ret);
        gHeartbeatThrId = MOCLOUD_INVALID_THR_ID;
        return -1;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "startThread succeed.\n");
    
    return 0;
}

/*
    stop heartbeat thread here;
*/
static int stopHeartbeatThr()
{
    if(gHeartbeatThrId != MOCLOUD_INVALID_THR_ID)
    {
        int ret = killThread(gHeartbeatThrId);
        if(ret < 0)
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "killThread failed! ret=%d\n", ret);
            return -1;
        }
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "killThread succeed.\n");
    }
    return 0;
}

/*
    This thread will recv a signal, then to refresh @gFilelist automatically;
    Currently, when send heartbeat to server, and server response to us, tell us
        the file list has been changed, then we should refresh our @gFilelist;
*/
static void * refreshFilelistThr(void * args)
{
    args = args;
    while(1)
    {
        sem_wait(&gRefreshFilelistSem);
        
        if(gStopRefreshFilelistThr)
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
                "gStopRefreshFilelistThr = %d, should exit this thread now!\n", 
                gStopRefreshFilelistThr);
            break;
        }

        int curType = gRefreshFilelistType;
        int ret = refreshFilelist(curType);
        if(ret < 0)
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
                "refreshFilelist failed! ret=%d, type=%d\n", ret, curType);
        }
        else
        {
            moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "refreshFilelist succeed.\n");
        }
    }
    
    pthread_exit(NULL);
}

static int startRefreshFilelistThr()
{
    int ret = sem_init(&gRefreshFilelistSem, 0, 0);
    if(ret != 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "sem_init failed! " \
            "ret=%d, errno=%d, desc=[%s]\n", ret, errno, strerror(errno));
        return -1;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "sem_init succeed.\n");

    ret = pthread_create(&gRefreshFilelistThId, NULL, refreshFilelistThr, NULL);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "create thread refreshFilelistThr failed! ret=%d, errno=%d, desc=[%s]\n",
            ret, errno, strerror(errno));
        sem_destroy(&gRefreshFilelistSem);
        gRefreshFilelistThId = MOCLOUD_INVALID_THR_ID;
        return -2;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "create thread refreshFilelistThr succeed.\n");

    gStopRefreshFilelistThr = 0;
    
    return 0;
}

static int stopRefreshFilelistThr()
{
    if(gRefreshFilelistThId != MOCLOUD_INVALID_THR_ID)
    {
        gStopRefreshFilelistThr = 1;
        sem_post(&gRefreshFilelistSem);
        pthread_join(gRefreshFilelistThId, NULL);
    }
    return 0;
}

/*
    1.get dwld tasks info file path from config info;
    2.check file exist or not;
    3.check file format;
    4.check each dwld task looply;
    5.set global memory;
*/
static int initDwldTasksInfo()
{
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "dwldInfoFile=[%s]\n", gCfgInfo.dwldInfoFile);

    pthread_mutex_lock(&gDwldTasksMutex);
    if(NULL != gDwldInfofileFd || NULL != gDwldTasksInfoListHead)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "some error occured! check for why!\n");
        pthread_mutex_unlock(&gDwldTasksMutex);
        return -1;
    }

    //if file donot exist, create it, and set our global var
    int ret = access(gCfgInfo.dwldInfoFile, 0);
    if(ret != 0)
    {
        moLoggerInfo(MOCLOUD_MODULE_LOGGER_NAME, 
            "DwldInfoFile [%s] donot exist! should create it now.\n", gCfgInfo.dwldInfoFile);
        gDwldInfofileFd = fopen(gCfgInfo.dwldInfoFile, "wb+");
        if(NULL == gDwldInfofileFd)
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "Open file [%s] failed! errno=%d, desc=[%s]\n",
                gCfgInfo.dwldInfoFile, errno, strerror(errno));
            pthread_mutex_unlock(&gDwldTasksMutex);
            return -2;
        }
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "Open file [%s] succeed.\n", gCfgInfo.dwldInfoFile);

        gDwldTasksInfoListHead = (DWLD_TASK_INFO_NODE *)malloc(sizeof(DWLD_TASK_INFO_NODE) * 1);
        if(NULL == gDwldTasksInfoListHead)
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "malloc failed! errno=%d, desc=[%s]\n", errno, strerror(errno));
            fclose(gDwldInfofileFd);
            gDwldInfofileFd = NULL;
            pthread_mutex_unlock(&gDwldTasksMutex);
            return -3;
        }
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "malloc succeed.\n");
        gDwldTasksInfoListHead->next = NULL;
        memset(&gDwldTasksInfoListHead->dwldTaskInfo, 0x00, sizeof(DWLD_TASK_INFO));
        pthread_mutex_unlock(&gDwldTasksMutex);
        return 0;
    }

    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "DwldInfoFile [%s] exist.\n", gCfgInfo.dwldInfoFile);

    //1.open file
    gDwldInfofileFd = fopen(gCfgInfo.dwldInfoFile, "ab+");
    if(NULL == gDwldInfofileFd)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "open file [%s] failed! errno=%d, desc=[%s]\n",
            gCfgInfo.dwldInfoFile, errno, strerror(errno));
        pthread_mutex_unlock(&gDwldTasksMutex);
        return -4;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "open file [%s] succeed\n", gCfgInfo.dwldInfoFile);
    rewind(gDwldInfofileFd);
    //2.init head node
    gDwldTasksInfoListHead = (DWLD_TASK_INFO_NODE *)malloc(sizeof(DWLD_TASK_INFO_NODE) * 1);
    if(NULL == gDwldTasksInfoListHead)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "malloc failed! errno=%d, desc=[%s]\n", errno, strerror(errno));
        fclose(gDwldInfofileFd);
        gDwldInfofileFd = NULL;
        pthread_mutex_unlock(&gDwldTasksMutex);
        return -5;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "malloc succeed.\n");
    gDwldTasksInfoListHead->next = NULL;
    memset(&gDwldTasksInfoListHead->dwldTaskInfo, 0x00, sizeof(DWLD_TASK_INFO));    
    //3.read file looply, get all dwld task info 
    int readLen = 0;
    DWLD_FILE_UNIT_INFO unitInfo;
    char isRightFormatFile = 1;
    int offset = 0;
    while(1)
    {
        //read a unit
        readLen = fread((char *)&unitInfo, 1, sizeof(DWLD_FILE_UNIT_INFO), gDwldInfofileFd);
        if(readLen != sizeof(DWLD_FILE_UNIT_INFO))
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
                "read failed! readLen=%d, length=%d, errno=%d, desc=[%s]\n",
                readLen, sizeof(DWLD_FILE_UNIT_INFO), errno, strerror(errno));
            isRightFormatFile = 0;
            break;
        }
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "read succeed.\n");

        //check MARK
        if(0 != strcmp(unitInfo.mark, MOCLOUD_MARK_DWLD))
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "mark check failed! unitInfo.mark=[%s]\n", unitInfo.mark);
            isRightFormatFile = 0;
            break;
        }
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "Mark check succeed.\n");

        //check checkSum
        if(!moCloudUtilsCheck_checksumCheckValue((char *)&unitInfo,
            sizeof(DWLD_FILE_UNIT_INFO) - sizeof(unsigned char),
            unitInfo.checkSum))
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "Check checksum failed!\n");
            isRightFormatFile = 0;
            break;
        }
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "Check checksum succeed.\n");

        //should set isDwlding to 0 if its current value is 1
        if(unitInfo.isUsing && unitInfo.dwldInfo.isDwlding)
        {
            moLoggerInfo(MOCLOUD_MODULE_LOGGER_NAME, "This unitInfo should reset to unUsing state.\n");

            fseek(gDwldInfofileFd, 0 - sizeof(DWLD_FILE_UNIT_INFO), SEEK_CUR);
            DWLD_FILE_UNIT_INFO tmp;
            memcpy(&tmp, &unitInfo, sizeof(DWLD_FILE_UNIT_INFO));
            tmp.dwldInfo.isDwlding = 0;
            moCloudUtilsCheck_checksumGetValue((char *)&tmp,
                sizeof(DWLD_FILE_UNIT_INFO) - sizeof(unsigned char),
                &tmp.checkSum);
            fwrite((char *)&tmp, 1, sizeof(DWLD_FILE_UNIT_INFO), gDwldInfofileFd);
        }

        if(unitInfo.isUsing)
        {
            //malloc new node
            DWLD_TASK_INFO_NODE * pNewNode = NULL;
            pNewNode = (DWLD_TASK_INFO_NODE *)malloc(sizeof(DWLD_TASK_INFO_NODE) * 1);
            if(NULL == pNewNode)
            {
                moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "malloc failed! errno=%d, desc=[%s]\n", errno, strerror(errno));
                isRightFormatFile = 0;
                break;
            }
            moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "malloc new node succeed.\n");
            memset(&pNewNode->dwldTaskInfo, 0x00, sizeof(DWLD_TASK_INFO));

            //set value, then add to gDwldTasksInfoListHead
            memcpy(&pNewNode->dwldTaskInfo.dwldInfo, &unitInfo.dwldInfo, sizeof(MOCLOUDCLIENT_DWLD_INFO));
            pNewNode->dwldTaskInfo.dwldInfo.isDwlding = 0;
            pNewNode->dwldTaskInfo.offset = offset;
            pNewNode->dwldTaskInfo.fileId = -1;
            pNewNode->next = gDwldTasksInfoListHead->next;
            gDwldTasksInfoListHead->next = pNewNode;
        }
        
        offset += sizeof(DWLD_FILE_UNIT_INFO);
    }

    //if in wrong format, should free all resources, and delete this file
    if(!isRightFormatFile)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "dwldInfoFile in wrong format! should clear its contents now.\n");

        //free all nodes except head node firstly
        DWLD_TASK_INFO_NODE * pCurNode = gDwldTasksInfoListHead->next;
        while(pCurNode != NULL)
        {
            gDwldTasksInfoListHead->next = pCurNode->next;
            free(pCurNode);
            pCurNode = NULL;
            pCurNode = gDwldTasksInfoListHead->next;
        }

        //reopen file in write mode, can clear file
        fclose(gDwldInfofileFd);
        gDwldInfofileFd = NULL;
        gDwldInfofileFd = fopen(gCfgInfo.dwldInfoFile, "wb+");
        if(NULL == gDwldInfofileFd)
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "open file [%s] failed! errno=%d, desc=[%s]\n",
                gCfgInfo.dwldInfoFile, errno, strerror(errno));
            free(gDwldTasksInfoListHead);
            gDwldTasksInfoListHead = NULL;
            pthread_mutex_unlock(&gDwldTasksMutex);
            return -6;
        }
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "reset file succeed.\n");
        pthread_mutex_unlock(&gDwldTasksMutex);
        return 0;
    }

    //in right format, just return is ok
    pthread_mutex_unlock(&gDwldTasksMutex);
    return 0;
}

static void dumpDwldTasksInfo()
{
    pthread_mutex_lock(&gDwldTasksMutex);
    
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "Dump dwldTasksInfo now.\n");

    DWLD_TASK_INFO_NODE * pCurNode = gDwldTasksInfoListHead->next;
    while(pCurNode != NULL)
    {
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
            "filetype=%d, filename=[%s], fileLength=%d, isDwlding=%d, localFilepath=[%s], unitId=%d, offset=%d, fileId=%d\n",
            pCurNode->dwldTaskInfo.dwldInfo.fileKey.filetype, pCurNode->dwldTaskInfo.dwldInfo.fileKey.filename,
            pCurNode->dwldTaskInfo.dwldInfo.fileLength, pCurNode->dwldTaskInfo.dwldInfo.isDwlding,
            pCurNode->dwldTaskInfo.dwldInfo.localFilepath, pCurNode->dwldTaskInfo.dwldInfo.unitId,
            pCurNode->dwldTaskInfo.offset, pCurNode->dwldTaskInfo.fileId);
        pCurNode = pCurNode->next;
    }
    
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "Dump dwldTasksInfo end.\n");
    pthread_mutex_unlock(&gDwldTasksMutex);
}

/*
    Free all resources;
*/
static void uninitDwldTasksInfo()
{
    pthread_mutex_lock(&gDwldTasksMutex);

    if(NULL != gDwldInfofileFd)
    {
        fclose(gDwldInfofileFd);
        gDwldInfofileFd = NULL;
    }

    if(NULL != gDwldTasksInfoListHead)
    {
        DWLD_TASK_INFO_NODE * pCurNode = gDwldTasksInfoListHead->next;
        while(pCurNode != NULL)
        {
            gDwldTasksInfoListHead->next = pCurNode->next;
            free(pCurNode);
            pCurNode = NULL;
            pCurNode = gDwldTasksInfoListHead->next;
        }
        free(gDwldTasksInfoListHead);
        gDwldTasksInfoListHead = NULL;
    }
    
    pthread_mutex_unlock(&gDwldTasksMutex);
}

/*
    1.read config file, get client ip(and port, not neccessary), get server ip and port;
    2.create socket;
    3.connect to server;
    4.do key agree;
    5.start heartbeat thread;

    6.create data socket;
    7.connect to server;
    8.start recvDataThread and writeDataThread;
*/
int moCloudClient_init(const char * pCfgFilepath)
{
    if(pCfgFilepath == NULL)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "Input param is NULL!\n");
        return MOCLOUDCLIENT_ERR_INPUT_NULL;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "pCfgFilepath=[%s]\n", pCfgFilepath);

    pthread_mutex_lock(&gMutex);

    if(gIsInited != INIT_STATE_NO)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "gIsInited = %d, means we have inited yet, cannot init again.\n", gIsInited);
        pthread_mutex_unlock(&gMutex);
        return MOCLOUDCLIENT_ERR_INIT_AGAIN;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "Start init moCloudClient now.\n");

    gCliState = CLI_STATE_IDLE;

    //1.parse cfg file;
    memset(&gCfgInfo, 0x00, sizeof(CFG_INFO));
    int ret = getCfgInfo(&gCfgInfo, pCfgFilepath);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "getCfgInfo failed! ret=%d, pCfgFilePath=[%s]\n", 
            ret, pCfgFilepath);
        pthread_mutex_unlock(&gMutex);
        return MOCLOUDCLIENT_ERR_CFGFILE_WRONG_FORMAT;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "getCfgInfo succeed.\n");
    dumpCfgInfo(gCfgInfo);
    //check dwldInfoFile, and refresh all dwld tasks to local memory
    ret = initDwldTasksInfo();
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "initDwldTasksInfo failed! ret=%d, pDwldInfoFilePath=[%s]\n", 
            ret, gCfgInfo.dwldInfoFile);
        pthread_mutex_unlock(&gMutex);
        return MOCLOUDCLIENT_ERR_DWLDTASKS_INIT;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "initDwldTasksInfo succeed.\n");
    dumpDwldTasksInfo();

    //2.create socket, get ip and port from @gCfgInfo, set socketId to @gCtrlSockId;
    ret = createSocket(gCfgInfo.clientIp, gCfgInfo.clientCtrlPort, &gCtrlSockId);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "create socket failed! ret=%d\n", ret);
        uninitDwldTasksInfo();
        memset(&gCfgInfo, 0x00, sizeof(CFG_INFO));
        pthread_mutex_unlock(&gMutex);
        return MOCLOUDCLIENT_ERR_CREATE_SOCKET;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
        "create socket succeed, socketId=%d.\n", gCtrlSockId);

    //3.connect to server, server info being read from @gCfgInfo
    ret = connectToServer(gCtrlSockId, gCfgInfo.serverIp, gCfgInfo.serverPort);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "connect to server failed! ret=%d\n", ret);
        destroySocket(&gCtrlSockId);
        uninitDwldTasksInfo();
        memset(&gCfgInfo, 0x00, sizeof(CFG_INFO));
        pthread_mutex_unlock(&gMutex);
        return MOCLOUDCLIENT_ERR_CONNECT_SERVER;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "connect to server succeed.\n");
    
    //4.do keyagree;
    ret = doKeyAgree();
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "key agree failed! ret=%d\n", ret);
        //Cannot disconnect to server, because we donot have crypt key to do crypt to disconnect request;
        
        destroySocket(&gCtrlSockId);
        uninitDwldTasksInfo();
        memset(&gCfgInfo, 0x00, sizeof(CFG_INFO));
        pthread_mutex_unlock(&gMutex);
        return MOCLOUDCLIENT_ERR_KEYAGREE;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "key agree succeed.\n");

    ret = startRefreshFilelistThr();
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "startHeartbeatThr failed! ret=%d\n", ret);
        disConnectToServer();
        destroySocket(&gCtrlSockId);
        uninitDwldTasksInfo();
        memset(&gCfgInfo, 0x00, sizeof(CFG_INFO));
        memset(&gCryptInfo, 0x00, sizeof(MOCLOUD_CRYPT_INFO));
        pthread_mutex_unlock(&gMutex);
        return MOCLOUDCLIENT_ERR_START_HEARTBEAT_THR;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "key agree succeed.\n");

    //5.start heartbeat thread
    ret = startHeartbeatThr();
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "startHeartbeatThr failed! ret=%d\n", ret);
        stopRefreshFilelistThr();
        disConnectToServer();
        destroySocket(&gCtrlSockId);
        uninitDwldTasksInfo();
        memset(&gCfgInfo, 0x00, sizeof(CFG_INFO));
        memset(&gCryptInfo, 0x00, sizeof(MOCLOUD_CRYPT_INFO));
        pthread_mutex_unlock(&gMutex);
        return MOCLOUDCLIENT_ERR_START_HEARTBEAT_THR;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "key agree succeed.\n");

    ret = cliDataInit(gCfgInfo.clientIp, gCfgInfo.clientDataPort, gCfgInfo.serverIp, gCfgInfo.serverPort);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "cliDataInit failed! ret=%d\n", ret);
        
        disConnectToServer();
        stopHeartbeatThr();
        stopRefreshFilelistThr();
        destroySocket(&gCtrlSockId);
        uninitDwldTasksInfo();
        memset(&gCfgInfo, 0x00, sizeof(CFG_INFO));
        memset(&gCryptInfo, 0x00, sizeof(MOCLOUD_CRYPT_INFO));
        pthread_mutex_unlock(&gMutex);
        return MOCLOUDCLIENT_ERR_CLIDATAINIT_FAILED;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "cliDataInit succeed.\n");

    gCliState = CLI_STATE_NOT_LOGIN;
    gIsInited = INIT_STATE_YES;

    pthread_mutex_unlock(&gMutex);
    
    return MOCLOUDCLIENT_ERR_OK;
}

/*
    1.stop heartbeat thread;
    2.say byebye to server;
    3.destroy socket;
    4.free memory;
*/
void moCloudClient_unInit()
{
    cliDataUnInit();
    
    pthread_mutex_lock(&gMutex);
    
    disConnectToServer();
    stopHeartbeatThr();
    stopRefreshFilelistThr();
    destroySocket(&gCtrlSockId);
    uninitDwldTasksInfo();
    memset(&gCfgInfo, 0x00, sizeof(CFG_INFO));
    memset(&gCryptInfo, 0x00, sizeof(MOCLOUD_CRYPT_INFO));
    gIsInited = INIT_STATE_NO;

    pthread_mutex_lock(&gFilelistMutex);
    freeLocalFilelist(MOCLOUD_FILETYPE_ALL);
    gCliState = CLI_STATE_IDLE;
    pthread_mutex_unlock(&gFilelistMutex);
    
    pthread_mutex_unlock(&gMutex);
}

/*
    Sign up to server;

    1.generate request;
    2.do encrypt to head;
    3.send to server(include head and body, body donot encrypt), wait for response header:
        3.1.timeout;
        3.2.cmdId;
    4.do decrypt;
    5.get result;

    return :
        0 : succeed;
        0-: failed for internal error in client;
        0+: failed from server, like duplicated username, and so on;
*/
int moCloudClient_signUp(const char * pUsrName, const char * pPasswd)
{
    if(NULL == pUsrName || NULL == pPasswd)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "Input param is NULL.\n");
        return MOCLOUDCLIENT_ERR_INPUT_NULL;
    }

    if(strlen(pUsrName) == 0 || strlen(pUsrName) > MOCLOUD_USERNAME_MAXLEN || 
        strlen(pUsrName) < MOCLOUD_PASSWD_MINLEN || strlen(pUsrName) > MOCLOUD_PASSWD_MAXLEN)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "username length=[%d], valid range [0, %d]; passwd length=%d, valid range [%d, %d]\n",
            strlen(pUsrName), MOCLOUD_USERNAME_MAXLEN, strlen(pPasswd),
            MOCLOUD_PASSWD_MINLEN, MOCLOUD_PASSWD_MAXLEN);
        return MOCLOUDCLIENT_ERR_INPUT_INVALID;
    }

    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
        "username=[%s], length=%d; passwd=[%s], length=%d\n",
        pUsrName, strlen(pUsrName), pPasswd, strlen(pPasswd));

    char body[(MOCLOUD_USERNAME_MAXLEN + MOCLOUD_PASSWD_MAXLEN) * 2] = {0x00};
    memset(body, 0x00, (MOCLOUD_USERNAME_MAXLEN + MOCLOUD_PASSWD_MAXLEN) * 2);
    sprintf(body, MOCLOUD_USER_PASSWD_FORMAT, pUsrName, pPasswd);

    MOCLOUD_CTRL_REQUEST request;
    memset(&request, 0x00, sizeof(MOCLOUD_CTRL_REQUEST));
    int bodyLen = strlen(body);
    genRequest(&request, MOCLOUD_REQUEST_TYPE_NEED_RESPONSE, MOCLOUD_CMDID_SIGNUP, bodyLen);

    //get cipher value to request header
    char * pCipherReq = NULL;
    int cipherLen = 0;
    int ret = getCipherRequestInfo(&request, &pCipherReq, &cipherLen);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "getCipherRequestInfo failed! ret=%d\n", ret);
        return MOCLOUDCLIENT_ERR_INTERNAL_ERROR;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "getCipherRequestInfo succeed.\n");

    //send request header in cipher, and request body in plain
    ret = writen(gCtrlSockId, pCipherReq, cipherLen);
    if(ret != cipherLen)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "send cipher request header failed! " \
            "ret=%d, cipherLen=%d\n", ret, cipherLen);
        free(pCipherReq);
        pCipherReq = NULL;
        return MOCLOUDCLIENT_ERR_SEND_REQUEST;
    }
    free(pCipherReq);
    pCipherReq = NULL;
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "send cipher request header succed.\n");
    
    ret = writen(gCtrlSockId, body, bodyLen);
    if(ret != bodyLen)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "send cipher request body failed! " \
            "ret=%d, bodyLen=%d\n", ret, bodyLen);
        return MOCLOUDCLIENT_ERR_SEND_REQUEST;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "send cipher request body succed.\n");

    //wait for response
    MOCLOUD_CTRL_RESPONSE resp;
    memset(&resp, 0x00, sizeof(MOCLOUD_CTRL_RESPONSE));
    ret = getRespHeader(&resp, MOCLOUD_CMDID_SIGNUP);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "getRespHeader failed! ret=%d\n", ret);
        return MOCLOUDCLIENT_ERR_GETRESP_FAILED;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "getRespHeader succeed, signUp ret=%d.\n", resp.ret);

    return resp.ret;
}

/*
    log in to server;

    1.generate request;
    2.do encrypt;
    3.send to server(header in cipher, body in plain), wait for response header;
        3.1.timeout;
        3.2.cmdId;
    4.do decrypt;
    5.get logIn result;

    return : 
        0 : succeed;
        0-: failed for internal error in client;
        0+: failed from server, like user has been login yet, and so on;
*/
int moCloudClient_logIn(const char * pUsrName, const char * pPasswd)
{
    if(NULL == pUsrName || NULL == pPasswd)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "Input param is NULL.\n");
        return MOCLOUDCLIENT_ERR_INPUT_NULL;
    }

    if(strlen(pUsrName) == 0 || strlen(pUsrName) > MOCLOUD_USERNAME_MAXLEN || 
        strlen(pUsrName) < MOCLOUD_PASSWD_MINLEN || strlen(pUsrName) > MOCLOUD_PASSWD_MAXLEN)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "username length=[%d], valid range [0, %d]; passwd length=%d, valid range [%d, %d]\n",
            strlen(pUsrName), MOCLOUD_USERNAME_MAXLEN, strlen(pPasswd),
            MOCLOUD_PASSWD_MINLEN, MOCLOUD_PASSWD_MAXLEN);
        return MOCLOUDCLIENT_ERR_INPUT_INVALID;
    }

    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
        "username=[%s], length=%d; passwd=[%s], length=%d\n",
        pUsrName, strlen(pUsrName), pPasswd, strlen(pPasswd));

    char body[(MOCLOUD_USERNAME_MAXLEN + MOCLOUD_PASSWD_MAXLEN) * 2] = {0x00};
    memset(body, 0x00, (MOCLOUD_USERNAME_MAXLEN + MOCLOUD_PASSWD_MAXLEN) * 2);
    sprintf(body, MOCLOUD_USER_PASSWD_FORMAT, pUsrName, pPasswd);

    MOCLOUD_CTRL_REQUEST request;
    memset(&request, 0x00, sizeof(MOCLOUD_CTRL_REQUEST));
    int bodyLen = strlen(body);
    genRequest(&request, MOCLOUD_REQUEST_TYPE_NEED_RESPONSE, MOCLOUD_CMDID_LOGIN, bodyLen);

    //get cipher value to request header
    char * pCipherReq = NULL;
    int cipherLen = 0;
    int ret = getCipherRequestInfo(&request, &pCipherReq, &cipherLen);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "getCipherRequestInfo failed! ret=%d\n", ret);
        return MOCLOUDCLIENT_ERR_INTERNAL_ERROR;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "getCipherRequestInfo succeed.\n");

    //send request header in cipher, and request body in plain
    ret = writen(gCtrlSockId, pCipherReq, cipherLen);
    if(ret != cipherLen)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "send cipher request header failed! " \
            "ret=%d, cipherLen=%d\n", ret, cipherLen);
        free(pCipherReq);
        pCipherReq = NULL;
        return MOCLOUDCLIENT_ERR_SEND_REQUEST;
    }
    free(pCipherReq);
    pCipherReq = NULL;
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "send cipher request header succed.\n");
    
    ret = writen(gCtrlSockId, body, bodyLen);
    if(ret != bodyLen)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "send cipher request body failed! " \
            "ret=%d, bodyLen=%d\n", ret, bodyLen);
        return MOCLOUDCLIENT_ERR_SEND_REQUEST;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "send cipher request body succed.\n");

    //wait for response
    MOCLOUD_CTRL_RESPONSE resp;
    memset(&resp, 0x00, sizeof(MOCLOUD_CTRL_RESPONSE));
    ret = getRespHeader(&resp, MOCLOUD_CMDID_LOGIN);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "getRespHeader failed! ret=%d\n", ret);
        return MOCLOUDCLIENT_ERR_GETRESP_FAILED;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "getRespHeader succeed, logIn ret=%d.\n", resp.ret);

    if(resp.ret == MOCLOUD_LOGIN_RET_OK)
    {
        memset(gCurLoginUsername, 0x00, MOCLOUD_USERNAME_MAXLEN);
        strcpy(gCurLoginUsername, pUsrName);
        memset(gCurLoginPassword, 0x00, MOCLOUD_PASSWD_MAXLEN);
        strcpy(gCurLoginPassword, pPasswd);

        gCliState = CLI_STATE_LOGIN;

        //should refresh file list info automatically
        ret = refreshFilelist(MOCLOUD_FILETYPE_ALL);
        if(ret < 0)
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "refreshFilelist failed! ret=%d\n", ret);
        }
        else
        {
            moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "refreshFilelist succeed.\n");
        }
    }

    return resp.ret;
}

/*
    logout from server;

    1.get current login info, like username, passwd, and so on;
    2.generate request;
    3.do encrypt;
    4.send to server, wait for response header:
        4.1.timeout;
        4.2.cmdId;
    5.do decrypt;
    6.paser response header, get result;

    return : 
        0 : succeed;
        0-: failed for internal error in client;
        0+: failed from server, like user donot login yet, and so on;
*/
int moCloudClient_logOut()
{
    char body[(MOCLOUD_USERNAME_MAXLEN + MOCLOUD_PASSWD_MAXLEN) * 2] = {0x00};
    memset(body, 0x00, (MOCLOUD_USERNAME_MAXLEN + MOCLOUD_PASSWD_MAXLEN) * 2);
    sprintf(body, MOCLOUD_USER_PASSWD_FORMAT, gCurLoginUsername, gCurLoginPassword);

    MOCLOUD_CTRL_REQUEST request;
    memset(&request, 0x00, sizeof(MOCLOUD_CTRL_REQUEST));
    int bodyLen = strlen(body);
    genRequest(&request, MOCLOUD_REQUEST_TYPE_NEED_RESPONSE, MOCLOUD_CMDID_LOGOUT, bodyLen);

    //get cipher value to request header
    char * pCipherReq = NULL;
    int cipherLen = 0;
    int ret = getCipherRequestInfo(&request, &pCipherReq, &cipherLen);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "getCipherRequestInfo failed! ret=%d\n", ret);
        return MOCLOUDCLIENT_ERR_INTERNAL_ERROR;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "getCipherRequestInfo succeed.\n");

    //send request header in cipher, and request body in plain
    ret = writen(gCtrlSockId, pCipherReq, cipherLen);
    if(ret != cipherLen)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "send cipher request header failed! " \
            "ret=%d, cipherLen=%d\n", ret, cipherLen);
        free(pCipherReq);
        pCipherReq = NULL;
        return MOCLOUDCLIENT_ERR_SEND_REQUEST;
    }
    free(pCipherReq);
    pCipherReq = NULL;
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "send cipher request header succed.\n");
    
    ret = writen(gCtrlSockId, body, bodyLen);
    if(ret != cipherLen)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "send cipher request body failed! " \
            "ret=%d, bodyLen=%d\n", ret, bodyLen);
        return MOCLOUDCLIENT_ERR_SEND_REQUEST;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "send cipher request body succed.\n");

    //wait for response
    MOCLOUD_CTRL_RESPONSE resp;
    memset(&resp, 0x00, sizeof(MOCLOUD_CTRL_RESPONSE));
    ret = getRespHeader(&resp, MOCLOUD_CMDID_LOGOUT);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "getRespHeader failed! ret=%d\n", ret);
        return MOCLOUDCLIENT_ERR_GETRESP_FAILED;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "getRespHeader succeed, signUp ret=%d.\n", resp.ret);

    if(resp.ret == MOCLOUD_LOGOUT_RET_OK)
    {
        memset(gCurLoginUsername, 0x00, MOCLOUD_USERNAME_MAXLEN);
        memset(gCurLoginPassword, 0x00, MOCLOUD_PASSWD_MAXLEN);
        gCliState = CLI_STATE_NOT_LOGIN;
    }

    return resp.ret;

}

/*
    These functions to get files info;

    To decrease the time being used, we will getFilelist in 2 cases : 
        1.When logIn, we do it automatically;
        2.When recv heartbeat response, we find server tell us its filelist being changed,
            we just refresh the filelist this time;
    After getFilelist, we will set it to local param @gFilelistinfo;

    When users call this function, we just set values to @pFilelist, donot get again.
    @pFilelist will malloc memory in this function, and its important to you to free it 
        by calling moCloudClient_freeFilelist(), or memory leakage will appeared;
    
    return : 
        0 : succeed, if @pAllFileInfo == NULL, means donot have any file in server;
        0-: failed;
*/
int moCloudClient_getFilelist(MOCLOUD_BASIC_FILEINFO_NODE ** ppFilelist, const MOCLOUD_FILETYPE type)
{
    if(NULL != *ppFilelist || type >= MOCLOUD_FILETYPE_MAX || type < MOCLOUD_FILETYPE_VIDEO)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "Input param is invalid!\n");
        return MOCLOUDCLIENT_ERR_INPUT_INVALID;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "type=%d\n", type);

    int ret = 0;
    int curType = 0;
    
    pthread_mutex_lock(&gFilelistMutex);

    for(curType = (type == MOCLOUD_FILETYPE_ALL ? MOCLOUD_FILETYPE_VIDEO : type); 
        curType < (type == MOCLOUD_FILETYPE_ALL ? MOCLOUD_FILETYPE_ALL : type + 1); 
        curType *= 2)
    {
        MOCLOUD_BASIC_FILEINFO_NODE * pCurNode = gpFilelist[curType];
        while(pCurNode != NULL)
        {
            MOCLOUD_BASIC_FILEINFO_NODE * pNewNode = NULL;                    
            pNewNode = (MOCLOUD_BASIC_FILEINFO_NODE *)malloc(sizeof(MOCLOUD_BASIC_FILEINFO_NODE) * 1);
            if(pNewNode == NULL)
            {
                moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "malloc failed! " \
                    "errno=%d, errno=[%s]\n", errno, strerror(errno));
                ret = MOCLOUDCLIENT_ERR_MALLOC_FAILED;
                break;
            }
            memcpy(&pNewNode->info, &pCurNode->info, sizeof(MOCLOUD_BASIC_FILEINFO));
            pNewNode->next = NULL;
        
            if(*ppFilelist == NULL)
            {
                //The first node of this list
                *ppFilelist = pNewNode;
            }
            else
            {
                pNewNode->next = *ppFilelist;
                *ppFilelist = pNewNode;
            }
            
            pCurNode = pCurNode->next;
        }        
    }
    
    pthread_mutex_unlock(&gFilelistMutex);
    
    if(ret < 0)
    {
        //We should free all memory being malloced when error occured
        MOCLOUD_BASIC_FILEINFO_NODE * pCurNode = *ppFilelist;
        while(pCurNode != NULL)
        {
            MOCLOUD_BASIC_FILEINFO_NODE * pNextNode = pCurNode->next;
            free(pCurNode);
            pCurNode = pNextNode;
        }
        *ppFilelist = NULL;
    }

    return ret;
}

/*
    free memory being malloced as a list;
*/
void moCloudClient_freeFilelist(MOCLOUD_BASIC_FILEINFO_NODE * pFilelist)
{
    MOCLOUD_BASIC_FILEINFO_NODE * pCurNode = pFilelist;
    while(pCurNode != NULL)
    {
        MOCLOUD_BASIC_FILEINFO_NODE * pNextNode = pCurNode->next;
        free(pCurNode);
        pCurNode = pNextNode;
    }
    pFilelist = NULL;    
}

/*
    Get all tasks, include dwlding and pausing and donot dealing ones;
*/
MOCLOUDCLIENT_DWLD_INFO_NODE * moCloudClient_getAllDwldTasks()
{
    MOCLOUDCLIENT_DWLD_INFO_NODE * pRet = NULL;

    pthread_mutex_lock(&gDwldTasksMutex);

    if(gDwldTasksInfoListHead == NULL)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "gDwldTasksInfoListHead == NULL!\n");
        pthread_mutex_unlock(&gDwldTasksMutex);
        return NULL;
    }
    DWLD_TASK_INFO_NODE * pCurNode = gDwldTasksInfoListHead->next;
    while(pCurNode != NULL)
    {
        if(NULL == pRet)
        {
            //The first node
            pRet = (MOCLOUDCLIENT_DWLD_INFO_NODE *)malloc(sizeof(MOCLOUDCLIENT_DWLD_INFO_NODE) * 1);
            if(NULL == pRet)
            {
                moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "malloc failed! errno=%d, desc=[%s]\n", errno, strerror(errno));
                pthread_mutex_unlock(&gDwldTasksMutex);
                return NULL;
            }
            memset(pRet, 0x00, sizeof(MOCLOUDCLIENT_DWLD_INFO_NODE));
            memcpy(&pRet->dwldInfo, &pCurNode->dwldTaskInfo.dwldInfo, sizeof(MOCLOUDCLIENT_DWLD_INFO_NODE));
            pRet->next = NULL;
        }
        else
        {
            //Add a new node to it
            MOCLOUDCLIENT_DWLD_INFO_NODE * pNewNode = (MOCLOUDCLIENT_DWLD_INFO_NODE *)malloc(sizeof(MOCLOUDCLIENT_DWLD_INFO_NODE) * 1);
            if(NULL == pNewNode)
            {
                moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "malloc failed! errno=%d, desc=[%s]\n", errno, strerror(errno));

                //release all resoure being malloced
                MOCLOUDCLIENT_DWLD_INFO_NODE * pDelNode = pRet->next;
                while(pDelNode != NULL)
                {
                    MOCLOUDCLIENT_DWLD_INFO_NODE * pNextNode = pDelNode->next;
                    free(pDelNode);
                    pDelNode = pNextNode;
                }
                free(pRet);
                pRet = NULL;
                
                pthread_mutex_unlock(&gDwldTasksMutex);
                return NULL;
            }
            memset(pNewNode, 0x00, sizeof(MOCLOUDCLIENT_DWLD_INFO_NODE));
            memcpy(&pNewNode->dwldInfo, &pCurNode->dwldTaskInfo.dwldInfo, sizeof(MOCLOUDCLIENT_DWLD_INFO_NODE));
            pNewNode->next = pRet->next;
            pRet->next = pNewNode;
        }
        pCurNode = pCurNode->next;
    }

    pthread_mutex_unlock(&gDwldTasksMutex);
    
    return pRet;
}

MOCLOUDCLIENT_DWLD_INFO_NODE * moCloudClient_getDwldingTasks()
{
    MOCLOUDCLIENT_DWLD_INFO_NODE * pRet = NULL;

    pthread_mutex_lock(&gDwldTasksMutex);

    if(gDwldTasksInfoListHead == NULL)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "gDwldTasksInfoListHead == NULL!\n");
        pthread_mutex_unlock(&gDwldTasksMutex);
        return NULL;
    }
    DWLD_TASK_INFO_NODE * pCurNode = gDwldTasksInfoListHead->next;
    while(pCurNode != NULL)
    {        
        if(!pCurNode->dwldTaskInfo.dwldInfo.isDwlding)
        {
            pCurNode = pCurNode->next;
            continue;
        }
            
        
        if(NULL == pRet)
        {
            //The first node
            pRet = (MOCLOUDCLIENT_DWLD_INFO_NODE *)malloc(sizeof(MOCLOUDCLIENT_DWLD_INFO_NODE) * 1);
            if(NULL == pRet)
            {
                moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "malloc failed! errno=%d, desc=[%s]\n", errno, strerror(errno));
                pthread_mutex_unlock(&gDwldTasksMutex);
                return NULL;
            }
            memset(pRet, 0x00, sizeof(MOCLOUDCLIENT_DWLD_INFO_NODE));
            memcpy(&pRet->dwldInfo, &pCurNode->dwldTaskInfo.dwldInfo, sizeof(MOCLOUDCLIENT_DWLD_INFO_NODE));
            pRet->next = NULL;
        }
        else
        {
            //Add a new node to it
            MOCLOUDCLIENT_DWLD_INFO_NODE * pNewNode = (MOCLOUDCLIENT_DWLD_INFO_NODE *)malloc(sizeof(MOCLOUDCLIENT_DWLD_INFO_NODE) * 1);
            if(NULL == pNewNode)
            {
                moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "malloc failed! errno=%d, desc=[%s]\n", errno, strerror(errno));

                //release all resoure being malloced
                MOCLOUDCLIENT_DWLD_INFO_NODE * pDelNode = pRet->next;
                while(pDelNode != NULL)
                {
                    MOCLOUDCLIENT_DWLD_INFO_NODE * pNextNode = pDelNode->next;
                    free(pDelNode);
                    pDelNode = pNextNode;
                }
                free(pRet);
                pRet = NULL;
                
                pthread_mutex_unlock(&gDwldTasksMutex);
                return NULL;
            }
            memset(pNewNode, 0x00, sizeof(MOCLOUDCLIENT_DWLD_INFO_NODE));
            memcpy(&pNewNode->dwldInfo, &pCurNode->dwldTaskInfo.dwldInfo, sizeof(MOCLOUDCLIENT_DWLD_INFO_NODE));
            pNewNode->next = pRet->next;
            pRet->next = pNewNode;
        }
        pCurNode = pCurNode->next;
    }

    pthread_mutex_unlock(&gDwldTasksMutex);
    
    return pRet;
}

static int getFileId(const MOCLOUD_FILEINFO_KEY key, int * pFileId)
{
    if(NULL == pFileId)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "Input param is NULL!\n");
        return -1;
    }

    pthread_mutex_lock(&gDwldTasksMutex);
    DWLD_TASK_INFO_NODE * pCurNode = gDwldTasksInfoListHead->next;
    while(pCurNode != NULL)
    {
        if(pCurNode->dwldTaskInfo.dwldInfo.fileKey.filetype == key.filetype && 
            0 == strcmp(pCurNode->dwldTaskInfo.dwldInfo.fileKey.filename, key.filename))
        {
            moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "find this file!\n");
            *pFileId = pCurNode->dwldTaskInfo.fileId;
            pthread_mutex_unlock(&gDwldTasksMutex);
            return 0;
        }
    }
    moLoggerWarn(MOCLOUD_MODULE_LOGGER_NAME, 
        "Donot find this file! filetype=%d, filename=[%s]\n",
        key.filetype, key.filename);
    
    pthread_mutex_unlock(&gDwldTasksMutex);
    return -2;
}

static int getDwldFileInfo(const MOCLOUD_FILEINFO_KEY key, int * pFileId, char * pLocalFilepath)
{
    if(NULL == pFileId)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "Input param is NULL!\n");
        return -1;
    }

    pthread_mutex_lock(&gDwldTasksMutex);
    DWLD_TASK_INFO_NODE * pCurNode = gDwldTasksInfoListHead->next;
    while(pCurNode != NULL)
    {
        if(pCurNode->dwldTaskInfo.dwldInfo.fileKey.filetype == key.filetype && 
            0 == strcmp(pCurNode->dwldTaskInfo.dwldInfo.fileKey.filename, key.filename))
        {
            moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "find this file!\n");
            *pFileId = pCurNode->dwldTaskInfo.fileId;
            strcpy(pLocalFilepath, pCurNode->dwldTaskInfo.dwldInfo.localFilepath);
            pthread_mutex_unlock(&gDwldTasksMutex);
            return 0;
        }
    }
    moLoggerWarn(MOCLOUD_MODULE_LOGGER_NAME, 
        "Donot find this file! filetype=%d, filename=[%s]\n",
        key.filetype, key.filename);
    
    pthread_mutex_unlock(&gDwldTasksMutex);
    return -2;
}

void moCloudClient_freeDwldTasks(MOCLOUDCLIENT_DWLD_INFO_NODE * pTasks)
{
    if(pTasks != NULL)
    {
        MOCLOUDCLIENT_DWLD_INFO_NODE * pNextNode = pTasks->next;
        while(pNextNode != NULL)
        {
            pTasks->next = pNextNode->next;
            free(pNextNode);
            pNextNode = pTasks->next;
        }
        free(pTasks);
        pTasks = NULL;
    }
}

/*
    set progress;
    if needed, send out;
*/
static void setDwldProgress(const int fileId, const int unitId)
{
    pthread_mutex_lock(&gDwldTasksMutex);

    if(NULL == gDwldTasksInfoListHead)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "NULL == gDwldTasksInfoListHead! cannot setDwldProgress!\n");
        pthread_mutex_unlock(&gDwldTasksMutex);
        return ;
    }

    DWLD_TASK_INFO_NODE * pCurNode = gDwldTasksInfoListHead->next;
    while(pCurNode != NULL)
    {
        if(pCurNode->dwldTaskInfo.fileId == fileId)
        {
            moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "Find this file(id=%d)\n", fileId);
            if(!pCurNode->dwldTaskInfo.dwldInfo.isDwlding)
            {
                moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "This file donot in dwlding!\n");
                pthread_mutex_unlock(&gDwldTasksMutex);
                return ;
            }
            pCurNode->dwldTaskInfo.dwldInfo.unitId = unitId;
            //refresh this info to dwldInfoFile
            DWLD_FILE_UNIT_INFO dwldFileUnitInfo;
            memset(&dwldFileUnitInfo, 0x00, sizeof(DWLD_FILE_UNIT_INFO));
            memcpy(&dwldFileUnitInfo.dwldInfo, &pCurNode->dwldTaskInfo.dwldInfo, sizeof(MOCLOUDCLIENT_DWLD_INFO));
            dwldFileUnitInfo.isUsing = 1;
            strcpy(dwldFileUnitInfo.mark, MOCLOUD_MARK_DWLD);
            moCloudUtilsCheck_checksumGetValue((char *)&dwldFileUnitInfo,
                sizeof(DWLD_FILE_UNIT_INFO) - sizeof(unsigned char), &dwldFileUnitInfo.checkSum);
            
            fseek(gDwldInfofileFd, pCurNode->dwldTaskInfo.offset, SEEK_SET);
            fwrite((char *)&dwldFileUnitInfo, 1, sizeof(DWLD_FILE_UNIT_INFO), gDwldInfofileFd);
            fflush(gDwldInfofileFd);
            moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "refresh progress to file succeed.\n");
            //TODO, if needed, send progress out

            pthread_mutex_unlock(&gDwldTasksMutex);
            return ;
        }
        
        pCurNode = pCurNode->next;
    }
    
    pthread_mutex_unlock(&gDwldTasksMutex);
    moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "setDwldProgress failed! fileId=%d\n", fileId);
    return ;
}

/*
    1.check dwld tasks number more than largest value or not;
    2.check task exist or not;
    3.get a new node or get an exist node;
    4.refresh to dwldInfoFile;
*/
static int getOneDwldNode(const MOCLOUD_FILEINFO_KEY key, const size_t filesize, const char * pLocalFilepath, 
    DWLD_TASK_INFO_NODE ** ppDwldTaskInfoNode, int * pStartOffset)
{
    int cnt = 0;
    DWLD_TASK_INFO_NODE * pTask = NULL;
    pthread_mutex_lock(&gDwldTasksMutex);
    DWLD_TASK_INFO_NODE * pCurNode = gDwldTasksInfoListHead->next;
    while(pCurNode != NULL)
    {
        if(pCurNode->dwldTaskInfo.dwldInfo.isDwlding)
            cnt++;
        if(key.filetype == pCurNode->dwldTaskInfo.dwldInfo.fileKey.filetype && 
            0 == strcmp(key.filename, pCurNode->dwldTaskInfo.dwldInfo.fileKey.filename))
        {
            pTask = pCurNode;
        }
        pCurNode = pCurNode->next;
    }
    if(cnt >= DWLD_TASK_MAX_NUM)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "cnt=%d, larger than maxValue(%d), cannot start new dwld task!",
            cnt, DWLD_TASK_MAX_NUM);
        pthread_mutex_unlock(&gDwldTasksMutex);
        return -1;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "cnt=%d.\n", cnt);

    if(NULL == pTask)
    {
        //A new dwld task, should malloc new node for it
        pTask = (DWLD_TASK_INFO_NODE *)malloc(sizeof(DWLD_TASK_INFO_NODE) * 1);
        if(NULL == pTask)
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "malloc failed! errno=%d, desc=[%s]\n", errno, strerror(errno));
            pthread_mutex_unlock(&gDwldTasksMutex);
            return -2;
        }
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "malloc succeed.\n");

        memset(pTask, 0x00, sizeof(DWLD_TASK_INFO_NODE));
        pTask->dwldTaskInfo.fileId = -1;
        pTask->dwldTaskInfo.offset = 0;
        //read dwldInfoFile, find a unit for this task
        rewind(gDwldInfofileFd);
        DWLD_FILE_UNIT_INFO dwldFileUnitInfo;
        memset(&dwldFileUnitInfo, 0x00, sizeof(DWLD_FILE_UNIT_INFO));
        while(fread((char *)&dwldFileUnitInfo, 1, sizeof(DWLD_FILE_UNIT_INFO), gDwldInfofileFd) == sizeof(DWLD_FILE_UNIT_INFO))
        {
            moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "get a unit from file!\n");
            if(dwldFileUnitInfo.isUsing)
            {
                pTask->dwldTaskInfo.offset += sizeof(DWLD_FILE_UNIT_INFO);
                continue;
            }
            else
                break;
        }
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "offset=%d.\n", pTask->dwldTaskInfo.offset);
        dwldFileUnitInfo.isUsing = 1;
        dwldFileUnitInfo.dwldInfo.isDwlding = 1;
        moCloudUtilsCheck_checksumGetValue((char *)&dwldFileUnitInfo,
                sizeof(DWLD_FILE_UNIT_INFO) - sizeof(unsigned char), &dwldFileUnitInfo.checkSum);        
        fseek(gDwldInfofileFd, pTask->dwldTaskInfo.offset, SEEK_SET);
        fwrite((char *)&dwldFileUnitInfo, 1, sizeof(DWLD_FILE_UNIT_INFO), gDwldInfofileFd);
        fflush(gDwldInfofileFd);
        
        memcpy(&pTask->dwldTaskInfo.dwldInfo.fileKey, &key, sizeof(MOCLOUD_FILEINFO_KEY));
        pTask->dwldTaskInfo.dwldInfo.fileLength = filesize;
        pTask->dwldTaskInfo.dwldInfo.isDwlding = 1;
        strcpy(pTask->dwldTaskInfo.dwldInfo.localFilepath, pLocalFilepath);
        pTask->dwldTaskInfo.dwldInfo.unitId = 0;

        pTask->next = gDwldTasksInfoListHead->next;
        gDwldTasksInfoListHead->next = pTask;

        *pStartOffset = 0;
    }
    else
    {
        if(pTask->dwldTaskInfo.dwldInfo.isDwlding)
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "Task dwlding now, cannot start dwld again!\n");
            pthread_mutex_unlock(&gDwldTasksMutex);
            return -3;
        }
        //An exist task, should refresh its info
        pTask->dwldTaskInfo.dwldInfo.isDwlding = 1;

        DWLD_FILE_UNIT_INFO dwldFileUnitInfo;
        memset(&dwldFileUnitInfo, 0x00, sizeof(DWLD_FILE_UNIT_INFO));
        fseek(gDwldInfofileFd, pTask->dwldTaskInfo.offset, SEEK_SET);
        fread((char *)&dwldFileUnitInfo, 1, sizeof(DWLD_FILE_UNIT_INFO), gDwldInfofileFd);
        dwldFileUnitInfo.isUsing = 1;
        dwldFileUnitInfo.dwldInfo.isDwlding = 1;
        fseek(gDwldInfofileFd, pTask->dwldTaskInfo.offset, SEEK_SET);
        fwrite((char *)&dwldFileUnitInfo, 1, sizeof(DWLD_FILE_UNIT_INFO), gDwldInfofileFd);
        fflush(gDwldInfofileFd);

        *pStartOffset = pTask->dwldTaskInfo.dwldInfo.unitId * sizeof(MOCLOUD_DATA_UNIT_LEN);
    }

    *ppDwldTaskInfoNode = pTask;
    pthread_mutex_unlock(&gDwldTasksMutex);
    return 0;
}

/*
    1.check key exist or not;
    2.delete from global memory;
    3.refresh to dwldInfoFile;
*/
static int delDwldNode(const MOCLOUD_FILEINFO_KEY key)
{
    pthread_mutex_lock(&gDwldTasksMutex);
    DWLD_TASK_INFO_NODE * pPreNode = gDwldTasksInfoListHead;
    DWLD_TASK_INFO_NODE * pCurNode = gDwldTasksInfoListHead->next;
    while(pCurNode != NULL)
    {
        if(pCurNode->dwldTaskInfo.dwldInfo.fileKey.filetype == key.filetype && 
            0 == strcmp(pCurNode->dwldTaskInfo.dwldInfo.fileKey.filename, key.filename))
        {
            moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "Find this file.\n");
            break;
        }
        pPreNode = pCurNode;
        pCurNode = pCurNode->next;
    }

    if(NULL == pCurNode)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "Donot find this dwld node! filetype=%d, filename=[%s]\n",
            key.filetype, key.filename);
        pthread_mutex_unlock(&gDwldTasksMutex);
        return -1;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "start delete this node now.\n");

    //find it in dwldInfoFile, and refresh its info to dwldInfoFile
    fseek(gDwldInfofileFd, pCurNode->dwldTaskInfo.offset, SEEK_SET);
    DWLD_FILE_UNIT_INFO dwldFileUnitInfo;
    memset(&dwldFileUnitInfo, 0x00, sizeof(DWLD_FILE_UNIT_INFO));
    fread((char *)&dwldFileUnitInfo, 1, sizeof(DWLD_FILE_UNIT_INFO), gDwldInfofileFd);
    dwldFileUnitInfo.isUsing = 0;
    dwldFileUnitInfo.dwldInfo.isDwlding = 0;
    moCloudUtilsCheck_checksumGetValue((char *)&dwldFileUnitInfo,
        sizeof(DWLD_FILE_UNIT_INFO) - sizeof(unsigned char),
        &dwldFileUnitInfo.checkSum);
    fwrite((char *)&dwldFileUnitInfo, 1, sizeof(DWLD_FILE_UNIT_INFO), gDwldInfofileFd);

    //delete this node then
    pPreNode->next = pCurNode->next;
    free(pCurNode);
    pCurNode = NULL;
    pthread_mutex_unlock(&gDwldTasksMutex);
    return 0;
}

/*
    Generate a request;
    encrypt, then send to server;
    wait for response, and decrypt, then parse result;
*/
static int doStartDwld(MOCLOUD_FILEINFO_KEY key, const int startOffset)
{
    char body[MOCLOUD_DWLDTASKINFO_MAXLEN] = {0x00};
    snprintf(body, MOCLOUD_DWLDTASKINFO_MAXLEN, MOCLOUD_DWLDTASKINFO_FORMAT,
        key.filetype, key.filename, startOffset, 0);
    body[MOCLOUD_DWLDTASKINFO_MAXLEN - 1] = 0x00;
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "startDwld body is [%s]\n", body);

    MOCLOUD_CTRL_REQUEST request;
    genRequest(&request, MOCLOUD_REQUEST_TYPE_NEED_RESPONSE, MOCLOUD_CMDID_DWLD_START, strlen(body));
    int cipherLen = 0;
    char * pCipherRequest = NULL;
    int ret = getCipherRequestInfo(&request, &pCipherRequest, &cipherLen);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "getCipherRequestInfo failed! ret=%d\n", ret);
        return -1;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "getCipherRequestInfo succeed.\n");

    //send request header in cipher, and request body in plain
    ret = writen(gCtrlSockId, pCipherRequest, cipherLen);
    if(ret != cipherLen)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "send cipher request header failed! " \
            "ret=%d, cipherLen=%d\n", ret, cipherLen);
        free(pCipherRequest);
        pCipherRequest = NULL;
        return -2;
    }
    free(pCipherRequest);
    pCipherRequest = NULL;
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "send cipher request header succed.\n");
    
    ret = writen(gCtrlSockId, body, strlen(body));
    if(ret != cipherLen)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "send cipher request body failed! " \
            "ret=%d, bodyLen=%d\n", ret, strlen(body));
        return -3;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "send cipher request body succed.\n");

    //wait for response
    MOCLOUD_CTRL_RESPONSE resp;
    memset(&resp, 0x00, sizeof(MOCLOUD_CTRL_RESPONSE));
    ret = getRespHeader(&resp, MOCLOUD_CMDID_DWLD_START);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "getRespHeader failed! ret=%d\n", ret);
        return -4;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "getRespHeader succeed, signUp ret=%d.\n", resp.ret);

    if(resp.ret == MOCLOUD_DWLDSTART_RET_OK)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "resp.ret=%d, start dwld failed!\n", resp.ret);
        return -5;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "start dwld succeed.\n");
    return 0;
}

/*
    Generate a request;
    encrypt, then send to server;
    wait for response, and decrypt, then parse result;
*/
static int doStopDwld(MOCLOUD_FILEINFO_KEY key, const int fileId)
{
    char body[MOCLOUD_DWLDTASKINFO_MAXLEN] = {0x00};
    snprintf(body, MOCLOUD_DWLDTASKINFO_MAXLEN, MOCLOUD_DWLDTASKINFO_FORMAT,
        key.filetype, key.filename, 0, fileId);
    body[MOCLOUD_DWLDTASKINFO_MAXLEN - 1] = 0x00;
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "startDwld body is [%s]\n", body);

    MOCLOUD_CTRL_REQUEST request;
    genRequest(&request, MOCLOUD_REQUEST_TYPE_NEED_RESPONSE, MOCLOUD_CMDID_DWLD_STOP, strlen(body));
    int cipherLen = 0;
    char * pCipherRequest = NULL;
    int ret = getCipherRequestInfo(&request, &pCipherRequest, &cipherLen);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "getCipherRequestInfo failed! ret=%d\n", ret);
        return -1;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "getCipherRequestInfo succeed.\n");

    //send request header in cipher, and request body in plain
    ret = writen(gCtrlSockId, pCipherRequest, cipherLen);
    if(ret != cipherLen)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "send cipher request header failed! " \
            "ret=%d, cipherLen=%d\n", ret, cipherLen);
        free(pCipherRequest);
        pCipherRequest = NULL;
        return -2;
    }
    free(pCipherRequest);
    pCipherRequest = NULL;
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "send cipher request header succed.\n");
    
    ret = writen(gCtrlSockId, body, strlen(body));
    if(ret != cipherLen)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "send cipher request body failed! " \
            "ret=%d, bodyLen=%d\n", ret, strlen(body));
        return -3;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "send cipher request body succed.\n");

    //wait for response
    MOCLOUD_CTRL_RESPONSE resp;
    memset(&resp, 0x00, sizeof(MOCLOUD_CTRL_RESPONSE));
    ret = getRespHeader(&resp, MOCLOUD_CMDID_DWLD_STOP);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "getRespHeader failed! ret=%d\n", ret);
        return -4;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "getRespHeader succeed, signUp ret=%d.\n", resp.ret);

    if(resp.ret == MOCLOUD_DWLDSTOP_RET_OK)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "resp.ret=%d, start dwld failed!\n", resp.ret);
        return -5;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "start dwld succeed.\n");
    return 0;
}

int moCloudClient_startDownloadFile(const MOCLOUD_FILEINFO_KEY key, const size_t filesize, const char * pLocalFilepath)
{
    //if it's a new dwld task, get a free node to do this task
    //else, get it's dwld node from gDwldTasksInfoListHead
    DWLD_TASK_INFO_NODE * pDwldNode = NULL;
    int startOffset = 0;
    int ret = getOneDwldNode(key, filesize, pLocalFilepath, &pDwldNode, &startOffset);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "getFreeDwldNode failed! ret=%d\n", ret);
        return -1;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "getFreeDwldNode succeed.\n");
    
    ret = cliDataStartDwld(key, filesize, pLocalFilepath, &pDwldNode->dwldTaskInfo.fileId, setDwldProgress);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "cliDataStartDwld failed! ret=%d\n", ret);
        //delete node from list, refresh dwldInfoFile
        delDwldNode(key);
        return -2;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "cliDataStartDwld succeed.\n");

    ret = doStartDwld(key, startOffset);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "doStartDwld failed! ret = %d\n", ret);
        cliDataStopDwld(pDwldNode->dwldTaskInfo.fileId);
        delDwldNode(key);
        return -3;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "doStartDwld succeed.\n");
    
    return 0;
}

int moCloudClient_stopDownloadFile(const MOCLOUD_FILEINFO_KEY key)
{
    int fileId = 0;
    char localFilepath[MOCLOUD_FILEPATH_MAXLEN] = {0x00};
    int ret = getDwldFileInfo(key, &fileId, localFilepath);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "getFileId failed! filetype=%d, filename=[%s], ret=%d\n",
            key.filetype, key.filename, ret);
        return -1;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "filetype=%d, filename=[%s], fileId=%d, localFilepath=[%s]\n",
        key.filetype, key.filename, fileId, localFilepath);

    ret = doStopDwld(key, fileId);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "doStopDwld failed! filetype=%d, filename=[%s], fileId=%d, ret=%d\n",
            key.filetype, key.filename, fileId, ret);
//        return -2;    //cannot exit, because other stop operation must be done.
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "doStopDwld succeed.\n");

    ret = cliDataStopDwld(fileId);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "cliDataStopDwld failed! filetype=%d, filename=[%s], fileId=%d, ret=%d\n",
            key.filetype, key.filename, fileId, ret);
//        return -3;    //cannot exit, because other stop operation must be done.
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "cliDataStopDwld succeed.\n");

    //should delete local file
    unlink(localFilepath);

    //should refresh dwldInfo.file, then delete the global memory
    delDwldNode(key);
    
    return 0;
}

int moCloudClient_pauseDownloadFile()    
{
    //TODO
    return 0;
}

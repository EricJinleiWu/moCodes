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

    //Other params being added here;
}CFG_INFO;

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
static int gDataSockId = MOCLOUD_INVALID_SOCKID;
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

#define RSA_PRIV_KEYLEN 128
const static char RSA_PRIV_KEY[RSA_PRIV_KEYLEN] = {
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

#define CONNECT_TIMEOUT 3   //in seconds, when connect to server, this is the timeout value
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

    pthread_mutex_lock(&gFilelistMutex);
    
    int curType = 0;
    for(curType = (type == MOCLOUD_FILETYPE_ALL ? MOCLOUD_FILETYPE_VIDEO : type);
        curType < (type == MOCLOUD_FILETYPE_ALL ? MOCLOUD_FILETYPE_ALL : type + 1);
        curType++)
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

    pthread_mutex_unlock(&gFilelistMutex);

    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "Free resource ok.\n");
    return 0;
}

/*
    Read ctrl ip and port from @gCfgInfo;
    set socket id to @gCtrlSockId;
    set this socket to non block;
    bind it to the ctrl port;
*/
static int createCtrlSocket()
{
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
        "Client ctrl socket being created now, ip=[%s], port=%d\n",
        gCfgInfo.clientIp, gCfgInfo.clientCtrlPort);

    //1.create socket
    gCtrlSockId = MOCLOUD_INVALID_SOCKID;
    gCtrlSockId = socket(AF_INET, SOCK_STREAM, 0);
    if(gCtrlSockId < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "create socket failed! errno=%d, desc=[%s]\n", errno, strerror(errno));
        return -1;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "create socket succeed.\n");

    //2.set to reusable and nonblock
    int ret = setSockReusable(gCtrlSockId);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "setSockReusable failed! ret=%d, sockId=%d\n", ret, gCtrlSockId);
        close(gCtrlSockId);
        gCtrlSockId = MOCLOUD_INVALID_SOCKID;
        return -2;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "setSockReusable succeed.\n");

    ret = setSock2NonBlock(gCtrlSockId);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "setSock2NonBlock failed! ret=%d, sockId=%d\n", ret, gCtrlSockId);
        close(gCtrlSockId);
        gCtrlSockId = MOCLOUD_INVALID_SOCKID;
        return -3;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "setSock2NonBlock succeed.\n");

    //3.bind to the port being defined
    struct sockaddr_in cliAddr;
    memset(&cliAddr, 0x00, sizeof(struct sockaddr_in));
    cliAddr.sin_family = AF_INET;
    cliAddr.sin_port = htons(gCfgInfo.clientCtrlPort);
    cliAddr.sin_addr.s_addr = inet_addr(gCfgInfo.clientIp);
    ret = bind(gCtrlSockId, (struct sockaddr *)&cliAddr, sizeof(struct sockaddr_in));
    if(ret != 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "Bind failed! ret=%d, clientCtrlPort=%d, errno=%d, desc=[%s]\n",
            ret, gCfgInfo.clientCtrlPort, errno, strerror(errno));
        close(gCtrlSockId);
        gCtrlSockId = MOCLOUD_INVALID_SOCKID;
        return -4;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "Bind succeed.\n");
    
    return 0;
}

/*
    If ctrl socket has valid socket id, should free it;
*/
static void destroyCtrlSocket()
{
    if(gCtrlSockId != MOCLOUD_INVALID_SOCKID)
    {
        close(gCtrlSockId);
        gCtrlSockId = MOCLOUD_INVALID_SOCKID;
    }
}

/*
    Read data ip and port from @gCfgInfo;
    set socket id to @gDataSockId;
*/
static int createDataSocket()
{
    //TODO, stub
    return 0;
}

/*
    If data socket has valid socket id, should free it;
*/
static void destroyDataSocket()
{
    if(gDataSockId != MOCLOUD_INVALID_SOCKID)
    {
        close(gDataSockId);
        gDataSockId = MOCLOUD_INVALID_SOCKID;
    }
}

/*
    Read server ip and port from @gCfgInfo;
    connect to server with TCP mode;
    @sockId is the id of a socket, because we have data socket id and ctrl socket id, 
        so we should has this param to define them;
*/
static int connectToServer(const int sockId)
{
    struct sockaddr_in serAddr;
    memset(&serAddr, 0x00, sizeof(struct sockaddr_in));
    serAddr.sin_family = AF_INET;
    serAddr.sin_port = htons(gCfgInfo.serverPort);
    serAddr.sin_addr.s_addr = inet_addr(gCfgInfo.serverIp);
    int ret = connect(sockId, (struct sockaddr *)&serAddr, sizeof(struct sockaddr_in));
    if(0 == ret)
    {
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
            "Connect to Server succeed, donot waste any time^_^!\n");
    }
    else
    {
        moLoggerInfo(MOCLOUD_MODULE_LOGGER_NAME, 
            "errno=%d, desc=[%s]\n", errno, strerror(errno));
        if(errno == EINPROGRESS)
        {
            moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
                "errno == EINPROGRESS, should check result now.\n");
            //Do select here.
            fd_set wFdSet;
            FD_ZERO(&wFdSet);
            FD_SET(sockId, &wFdSet);
            fd_set rFdSet;
            FD_ZERO(&rFdSet);
            FD_SET(sockId, &rFdSet);
            struct timeval timeout;
            timeout.tv_sec = CONNECT_TIMEOUT;
            timeout.tv_usec = 0;
            ret = select(sockId + 1, &rFdSet, &wFdSet, NULL, &timeout);
            if(ret < 0)
            {
                moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
                    "select failed! ret = %d, errno = %d, desc = [%s]\n", 
                    ret, errno, strerror(errno));
                return -1;
            }
            else if(ret == 0)
            {
                moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
                    "select timeout, we think this means connect to server failed!\n");
                return -2;
            }
            else
            {
                moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "select ok, check its result now.\n");
                if(FD_ISSET(sockId, &wFdSet))
                {
                    if(FD_ISSET(sockId, &rFdSet))
                    {
                        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
                            "select ok, but wFdSet and rFdSet all being set, we should check!\n");
                        //check this socket is OK or not.
                        int err = 0;
                        socklen_t errLen = sizeof(err);
                        ret = getsockopt(sockId, SOL_SOCKET, SO_ERROR, &err, &errLen);
                        if(0 != ret)
                        {
                            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
                                "getsockopt failed! ret = %d, errno = %d, desc = [%s]\n", 
                                ret, errno, strerror(errno));
                            return -3;
                        }
                        else
                        {
                            if(err != 0)
                            {
                                moLoggerError(MOCLOUD_MODULE_LOGGER_NAME,
                                    "getsockopt ok, but err=%d, desc=[%s], donot mean OK, just mean some error!\n",
                                    err, strerror(err));
                                return -4;
                            }
                            else
                            {
                                moLoggerInfo(MOCLOUD_MODULE_LOGGER_NAME,
                                    "getsockopt ok, and err = %d, means connect succeed!\n", err);
                            }
                        }
                    }
                    else
                    {
                        moLoggerInfo(MOCLOUD_MODULE_LOGGER_NAME,
                            "select ok, and just wFdSet being set, means connect being OK.\n");
                    }
                }
                else
                {
                    //Just this socket being set to wFdSet, if not this socket, error ocurred!
                    moLoggerError(MOCLOUD_MODULE_LOGGER_NAME,
                        "select ok, but the fd is not client socket!\n");
                    return -5;
                }
            }
        }
        else
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME,
                "Connect failed! ret = %d, errno = %d, desc = [%s]\n",
                ret, errno, strerror(errno));
            return -6;
        }
    }
    
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

    pthread_mutex_lock(&gFilelistMutex);

    int ret = 0;
    MOCLOUD_BASIC_FILEINFO * pCurInfo = NULL;
    int readLen = 0;
    while(readLen + sizeof(MOCLOUD_BASIC_FILEINFO) <= bodyLen)
    {
        pCurInfo = pRespBody + readLen;
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
    1.read config file, get client ip(and port, not neccessary), get server ip and port;
    2.create socket;
    3.connect to server;
    4.do key agree;
    5.start heartbeat thread;
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

    //2.create socket, get ip and port from @gCfgInfo, set socketId to @gCtrlSockId;
    ret = createCtrlSocket();
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "create socket failed! ret=%d\n", ret);
        memset(&gCfgInfo, 0x00, sizeof(CFG_INFO));
        pthread_mutex_unlock(&gMutex);
        return MOCLOUDCLIENT_ERR_CREATE_SOCKET;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
        "create socket succeed, socketId=%d.\n", gCtrlSockId);

    //3.connect to server, server info being read from @gCfgInfo
    ret = connectToServer(gCtrlSockId);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "connect to server failed! ret=%d\n", ret);
        destroyCtrlSocket();
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
        
        destroyCtrlSocket();
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
        destroyCtrlSocket();
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
        destroyCtrlSocket();
        memset(&gCfgInfo, 0x00, sizeof(CFG_INFO));
        memset(&gCryptInfo, 0x00, sizeof(MOCLOUD_CRYPT_INFO));
        pthread_mutex_unlock(&gMutex);
        return MOCLOUDCLIENT_ERR_START_HEARTBEAT_THR;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "key agree succeed.\n");

    //6.other operations 
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
    pthread_mutex_lock(&gMutex);
    
    disConnectToServer();
    stopHeartbeatThr();
    stopRefreshFilelistThr();
    destroyCtrlSocket();
    destroyDataSocket();
    memset(&gCfgInfo, 0x00, sizeof(CFG_INFO));
    memset(&gCryptInfo, 0x00, sizeof(MOCLOUD_CRYPT_INFO));
    gIsInited = INIT_STATE_NO;

    freeLocalFilelist(MOCLOUD_FILETYPE_ALL);
    
    pthread_mutex_lock(&gFilelistMutex);
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
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "getRespHeader succeed, signUp ret=%d.\n", resp.ret);

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
int moCloudClient_getFilelist(MOCLOUD_BASIC_FILEINFO_NODE * pFilelist, const MOCLOUD_FILETYPE type)
{
    if(NULL != pFilelist || type >= MOCLOUD_FILETYPE_MAX || type < MOCLOUD_FILETYPE_VIDEO)
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
        curType++)
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
        
            if(pFilelist == NULL)
            {
                //The first node of this list
                pFilelist = pNewNode;
            }
            else
            {
                pNewNode->next = pFilelist;
                pFilelist = pNewNode;
            }
            
            pCurNode = pCurNode->next;
        }        
    }
    
    pthread_mutex_unlock(&gFilelistMutex);
    
    if(ret < 0)
    {
        //We should free all memory being malloced when error occured
        MOCLOUD_BASIC_FILEINFO_NODE * pCurNode = pFilelist;
        while(pCurNode != NULL)
        {
            MOCLOUD_BASIC_FILEINFO_NODE * pNextNode = pCurNode->next;
            free(pCurNode);
            pCurNode = pNextNode;
        }
        pFilelist = NULL;
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



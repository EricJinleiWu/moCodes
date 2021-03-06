#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "moCpsClient.h"
#include "moLogger.h"

typedef enum
{
    INIT_STATE_NO,  //donot init yet
    INIT_STATE_YES, //has been inited
}INIT_STATE;

typedef enum
{
    THREAD_STATE_STOPPED,
    THREAD_STATE_RUNNING
}THREAD_STATE;

typedef enum
{
    RECVDATA_STATE_RECVHEAD,    //in this state, should start to recv header
    RECVDATA_STATE_RECVBODY,  //head has been recved, should recv body now.
}RECVDATA_STATE;

#define THREAD_TIME_INTEVAL 1
#define CONNECT_TIMEOUT 3   //in seconds
#define KEYAGREE_TIMEOUT    4   //in seconds
#define WAIT_RESP_TIMEOUT   3   //in seconds
//The name of each attribute in config file
#define CONF_SERV_SEC_NAME          "servInfo"
#define CONF_SERVIP_NAME            "servIp"
#define CONF_SERVPORT_NAME          "servPort"
#define CONF_CLI_SEC_NAME          "cliInfo"
#define CONF_CLI_IP_NAME            "cliIp"
#define CONF_CLI_DATAPORT_NAME      "cliDataPort"
#define CONF_CLI_CTRLPORT_NAME      "cliCtrlPort"

static INIT_STATE gIsInited = INIT_STATE_NO;
static int gCtrlSockId = MOCPS_INVALID_SOCKID;
static int gDataSockId = MOCPS_INVALID_SOCKID;
static pthread_mutex_t gMutex = PTHREAD_MUTEX_INITIALIZER;
static pDataCallbackFunc gpDataCallbackFunc = NULL;
//When we need stop heartbeat thread, set this to 0
//And in heartbeat thread, it will stop running
static char gHeartbeatThrRunningFlag = 0;
//When we need stop heartbeat thread, we should check this state
//after it convert from RUNNING to STOPPED, the thread being stopped.
static THREAD_STATE gHeartbeatThrRunningState = THREAD_STATE_STOPPED;
//The flag and state of thread recvData
static char gRecvdataThrRunningFlag = 0;
static THREAD_STATE gRecvdataThrRunningState = THREAD_STATE_STOPPED;
//The crypt info, include algo, keylen, key
static MOCPS_CRYPT_INFO gCryptInfo;

typedef struct
{
    char servIp[MOCPS_IP_ADDR_MAXLEN];  //The ip of moCpsServer
    int servPort;   //The port of moCpsServer
    char cliIp[MOCPS_IP_ADDR_MAXLEN];   //The ip of moCpsClient
    int cliCtrlPort;    //the ctrl port of moCpsClient
    int cliDataPort;    //the data port of moCpsClient
}MOCPS_CLI_CONFINFO;

static int dmmInit();
static void dmmUnInit();
static int dmmSaveCurBodyBlk(const MOCPS_DATA_RESPONSE_HEADER header, const char *pCurBlk);
static void dmmClear();
static char * dmmDataPtr();
static char dmmIsCompleteData();

static int crmmInit();
static void crmmUnInit();
static char * crmmGet();
static int crmmMaxSize();

static int getRespFromServer(const MOCPS_CMDID cmdId, MOCPS_CTRL_RESPONSE * pCtrlResp);


/*
    Parse the @pConfFilepath, get its values
*/
static int parseConf(const char * pConfFilepath, MOCPS_CLI_CONFINFO * pConfInfo)
{
    if(NULL == pConfFilepath || NULL == pConfInfo)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Input param is NULL!\n");
        return -1;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "Config file path is [%s]\n", pConfFilepath);

    //init parser
    MOUTILS_INI_SECTION_INFO_NODE * pIniInfoNode = NULL;
    pIniInfoNode = moUtils_Ini_Init(pConfFilepath);
    if(NULL == pIniInfoNode)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, 
            "moUtils_Ini_Init failed, config file path is [%s]\n",
            pConfFilepath);
        return -2;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "moUtils_Ini_Init succeed.\n");

    //get server ip values
    char tmp[MOCPS_IP_ADDR_MAXLEN] = {0x00};
    memset(tmp, 0x00, MOCPS_IP_ADDR_MAXLEN);
    int ret = moUtils_Ini_GetAttrValue(CONF_SERV_SEC_NAME, CONF_SERVIP_NAME, tmp, pIniInfoNode);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "moUtils_Ini_GetAttrValue failed! " \
            "ret = %d, secName = [%s], key = [%s]\n", ret, CONF_SERV_SEC_NAME, CONF_SERVIP_NAME);
        moUtils_Ini_UnInit(pIniInfoNode);
        return -3;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "Get server ip ok, value is [%s]\n", tmp);
    strncpy(pConfInfo->servIp, tmp, MOCPS_IP_ADDR_MAXLEN);
    pConfInfo->servIp[MOCPS_IP_ADDR_MAXLEN - 1] = 0x00;
    //check ip address in right format or not
    if(!isValidIpAddr(pConfInfo->servIp))
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "servIp=[%s], donot a valid one!\n",
            pConfInfo->servIp);
        moUtils_Ini_UnInit(pIniInfoNode);
        return -4;
    }
    //server port
    memset(tmp, 0x00, MOCPS_IP_ADDR_MAXLEN);
    ret = moUtils_Ini_GetAttrValue(CONF_SERV_SEC_NAME, CONF_SERVPORT_NAME, tmp, pIniInfoNode);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "moUtils_Ini_GetAttrValue failed! " \
            "ret = %d, secName = [%s], key = [%s]\n", ret, CONF_SERV_SEC_NAME, CONF_SERVIP_NAME);
        moUtils_Ini_UnInit(pIniInfoNode);
        return -5;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "Get server port ok, value is [%s]\n", tmp);
    pConfInfo->servPort = atoi(tmp);
    //client ip
    memset(tmp, 0x00, MOCPS_IP_ADDR_MAXLEN);
    ret = moUtils_Ini_GetAttrValue(CONF_CLI_SEC_NAME, CONF_CLI_IP_NAME, tmp, pIniInfoNode);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "moUtils_Ini_GetAttrValue failed! " \
            "ret = %d, secName = [%s], key = [%s]\n", ret, CONF_SERV_SEC_NAME, CONF_SERVIP_NAME);
        moUtils_Ini_UnInit(pIniInfoNode);
        return -6;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "Get client ip ok, value is [%s]\n", tmp);
    strncpy(pConfInfo->cliIp, tmp, MOCPS_IP_ADDR_MAXLEN);
    pConfInfo->cliIp[MOCPS_IP_ADDR_MAXLEN - 1] = 0x00;
    //check ip address in right format or not
    if(!isValidIpAddr(pConfInfo->cliIp))
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "cliIp=[%s], donot a valid one!\n",
            pConfInfo->cliIp);
        moUtils_Ini_UnInit(pIniInfoNode);
        return -7;
    }
    //client data port
    memset(tmp, 0x00, MOCPS_IP_ADDR_MAXLEN);
    ret = moUtils_Ini_GetAttrValue(CONF_CLI_SEC_NAME, CONF_CLI_DATAPORT_NAME, tmp, pIniInfoNode);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "moUtils_Ini_GetAttrValue failed! " \
            "ret = %d, secName = [%s], key = [%s]\n", ret, CONF_SERV_SEC_NAME, CONF_SERVIP_NAME);
        moUtils_Ini_UnInit(pIniInfoNode);
        return -8;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "Get client data port ok, value is [%s]\n", tmp);
    pConfInfo->cliDataPort = atoi(tmp);
    //client ctrl port
    memset(tmp, 0x00, MOCPS_IP_ADDR_MAXLEN);
    ret = moUtils_Ini_GetAttrValue(CONF_CLI_SEC_NAME, CONF_CLI_CTRLPORT_NAME, tmp, pIniInfoNode);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "moUtils_Ini_GetAttrValue failed! " \
            "ret = %d, secName = [%s], key = [%s]\n", ret, CONF_SERV_SEC_NAME, CONF_SERVIP_NAME);
        moUtils_Ini_UnInit(pIniInfoNode);
        return -9;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "Get client ctrl port ok, value is [%s]\n", tmp);
    pConfInfo->cliCtrlPort = atoi(tmp);

    moUtils_Ini_UnInit(pIniInfoNode);
    return 0;
}

static void dumpConfInfo(const MOCPS_CLI_CONFINFO info)
{
    moLoggerInfo(MOCPS_MODULE_LOGGER_NAME, "Dump config info start:\n");
    moLoggerInfo(MOCPS_MODULE_LOGGER_NAME, "ServerIp=[%s], ServerPort=%d, " \
        "ClientIp=[%s], ClientDataPort=%d, ClientCtrlPort=%d\n", 
        info.servIp, info.servPort, info.cliIp, info.cliDataPort, info.cliCtrlPort);
    moLoggerInfo(MOCPS_MODULE_LOGGER_NAME, "Dump config info stop.\n");
}

static int encryptWithAes(const MOCPS_CTRL_REQUEST req, char *pCipher)
{
    //TODO, this is stub exec
    memcpy(pCipher, (char *)&req, sizeof(MOCPS_CTRL_REQUEST));
    return 0;
}

static int encryptWithDes(const MOCPS_CTRL_REQUEST req, char *pCipher)
{
#if 0
    unsigned int cipherLen = 0;
    int ret = moCrypt_DES_ECB(MOCRYPT_METHOD_ENCRYPT,
        (unsigned char *)&req, sizeof(MOCPS_CTRL_REQUEST),
        (unsigned char *)&gCryptInfo.cryptKey, gCryptInfo.keyLen,
        (unsigned char *)pCipher, &cipherLen);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "moCrypt_DES_ECB failed, ret = %d\n", ret);
        return -1;
    }
#else
    memcpy(pCipher, (char *)&req, sizeof(MOCPS_CTRL_REQUEST));
#endif
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "moCrypt_DES_ECB succeed.\n");    
    return 0;
}

static int encryptWithDes3(const MOCPS_CTRL_REQUEST req, char *pCipher)
{
#if 0
    unsigned int cipherLen = 0;
    int ret = moCrypt_DES3_ECB(MOCRYPT_METHOD_ENCRYPT,
        (unsigned char *)&req, sizeof(MOCPS_CTRL_REQUEST),
        (unsigned char *)&gCryptInfo.cryptKey, gCryptInfo.keyLen,
        (unsigned char *)pCipher, &cipherLen);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "moCrypt_DES3_ECB failed, ret = %d\n", ret);
        return -1;
    }
#else
    memcpy(pCipher, (char *)&req, sizeof(MOCPS_CTRL_REQUEST));
#endif
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "moCrypt_DES3_ECB succeed.\n");    
    return 0;
}

static int encryptWithRc4(const MOCPS_CTRL_REQUEST req, char *pCipher)
{
#if 0
    memcpy(pCipher, (char *)&req, sizeof(MOCPS_CTRL_REQUEST));
    int ret = moCrypt_RC4_cryptString((unsigned char *)&gCryptInfo.cryptKey, 
        gCryptInfo.keyLen, (unsigned char *)pCipher, sizeof(MOCPS_CTRL_REQUEST));
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "moCrypt_RC4_cryptString failed, ret = %d\n", ret);
        return -1;
    }
#else
    memcpy(pCipher, (char *)&req, sizeof(MOCPS_CTRL_REQUEST));
#endif
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "moCrypt_RC4_cryptString succeed.\n");    
    return 0;
}

/*
    Do encrypt to @req;
*/
static int doEncrypt2Req(const MOCPS_CTRL_REQUEST req, char *pCipher)
{
    if(NULL == pCipher)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Input param is NULL.\n");
        return -1;
    }

    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "mark=[%s], cmdId=%d, isNeedResp=%d\n",
        req.basicInfo.mark, req.basicInfo.cmdId, req.basicInfo.isNeedResp);

    int ret = 0;
    switch(gCryptInfo.cryptAlgoNo)
    {
        case MOCPS_CRYPT_ALGO_AES:
            ret = encryptWithAes(req, pCipher);
            break;
        case MOCPS_CRYPT_ALGO_DES:
            ret = encryptWithDes(req, pCipher);
            break;
        case MOCPS_CRYPT_ALGO_DES3:
            ret = encryptWithDes3(req, pCipher);
            break;
        case MOCPS_CRYPT_ALGO_RC4:
            ret = encryptWithRc4(req, pCipher);
            break;
        default:
            moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Input algoNo=%d, invalid one.\n", gCryptInfo.cryptAlgoNo);
            ret = -2;
            break;
    }
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "ret = %d\n", ret);
    }
    else
    {
        moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "encrypt succeed.\n");
    }
    return ret;
}

static int decryptWithAes(const char *pCipher, char * pPlain, const int len)
{
    //TODO, this is stub exec
    memcpy(pPlain, pCipher, len);
    return 0;
}

static int decryptWithDes(const char *pCipher, char * pPlain, const int len)
{
#if 0
    unsigned int plainLen = 0;
    int ret = moCrypt_DES_ECB(MOCRYPT_METHOD_DECRYPT,
        (unsigned char *)pCipher, len,
        (unsigned char *)&gCryptInfo.cryptKey, gCryptInfo.keyLen,
        (unsigned char *)pPlain, &plainLen);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "moCrypt_DES_ECB failed, ret = %d\n", ret);
        return -1;
    }
#else
    memcpy(pPlain, pCipher, len);
#endif
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "moCrypt_DES_ECB succeed.\n");    
    return 0;
}

static int decryptWithDes3(const char *pCipher, char * pPlain, const int len)
{
#if 0
    unsigned int plainLen = 0;
    int ret = moCrypt_DES3_ECB(MOCRYPT_METHOD_DECRYPT,
        (unsigned char *)pCipher, len,
        (unsigned char *)&gCryptInfo.cryptKey, gCryptInfo.keyLen,
        (unsigned char *)pPlain, &plainLen);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "moCrypt_DES3_ECB failed, ret = %d\n", ret);
        return -1;
    }
#else
    memcpy(pPlain, pCipher, len);
#endif
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "moCrypt_DES3_ECB succeed.\n");    
    return 0;
}

static int decryptWithRc4(const char *pCipher, char * pPlain, const int len)
{
#if 0
    memcpy((char *)pPlain, (char *)pCipher, len);
    int ret = moCrypt_RC4_cryptString((unsigned char *)&gCryptInfo.cryptKey, 
        gCryptInfo.keyLen, (unsigned char *)pPlain, len);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "moCrypt_RC4_cryptString failed, ret = %d\n", ret);
        return -1;
    }
#else
    memcpy(pPlain, pCipher, len);
#endif
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "moCrypt_RC4_cryptString succeed.\n");    
    return 0;
}

/*
    decrypt response from cipher to plain.
*/
static int doDecrypt2Resp(const char *pCipher, char * pPlain, const int len)
{
    if(NULL == pCipher || NULL == pPlain)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Input param is NULL.\n");
        return -1;
    }

    int ret = 0;
    switch(gCryptInfo.cryptAlgoNo)
    {
        case MOCPS_CRYPT_ALGO_AES:
            ret = decryptWithAes(pCipher, pPlain, len);
            break;
        case MOCPS_CRYPT_ALGO_DES:
            ret = decryptWithDes(pCipher, pPlain, len);
            break;
        case MOCPS_CRYPT_ALGO_DES3:
            ret = decryptWithDes3(pCipher, pPlain, len);
            break;
        case MOCPS_CRYPT_ALGO_RC4:
            ret = decryptWithRc4(pCipher, pPlain, len);
            break;
        default:
            moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Input algoNo=%d, invalid one.\n", gCryptInfo.cryptAlgoNo);
            ret = -2;
            break;
    }
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "ret = %d\n", ret);
    }
    else
    {
        moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "encrypt succeed.\n");
    }
    return ret;
}

/*
    Create a socket, and bind to @port;
*/
static int createSockAndBind(const char *pCliIp, const int port, int * pSockId)
{
    if(NULL == pSockId || NULL == pCliIp)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Input param is NULL!\n");
        return -1;
    }

    int sockId = MOCPS_INVALID_SOCKID;
    sockId = socket(AF_INET, SOCK_STREAM, 0);
    if(sockId < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Create socket failed! errno = %d, desc = [%s]\n",
            errno, strerror(errno));
        return -2;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "Create socket ok, sockId = %d.\n", sockId);

    //bind to the port
    struct sockaddr_in addrInfo;
    memset(&addrInfo, 0x00, sizeof(struct sockaddr_in));
    addrInfo.sin_family = AF_INET;
    addrInfo.sin_addr.s_addr = inet_addr(pCliIp);
    addrInfo.sin_port = htons(port);
    int ret = bind(sockId, (struct sockaddr *)&addrInfo, sizeof(struct sockaddr));
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Bind failed! ret = %d, errno = %d, desc = [%s]\n",
            ret, errno, strerror(errno));
        close(sockId);
        sockId = MOCPS_INVALID_SOCKID;
        return -3;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "Bind succeed.\n");

    *pSockId = sockId;
    return 0;   
}

/*
    Set socket attribute, currently, support:
        1.reused;
        2.non-block;
*/
static int setSockAttr(const int sockId)
{
    //1.set this socket to reusable
    int on = 1;
    int ret = setsockopt(sockId, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "setsockopt failed! ret = %d, errno = %d, desc = [%s]\n",
            ret, errno, strerror(errno));
        return -1;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "setsockopt ok.\n");

    //2.set socket to non-block
    ret = setSock2NonBlock(sockId);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "setSock2NonBlock failed! ret = %d\n", ret);
        return -2;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "setSock2NonBlock ok.\n");
    return 0;
}

/*
    Connect to server, being async, if connect failed in timeout, return errno.
*/
static int connect2Server(const int sockId, const char *servIp, const int servPort)
{
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "sockId=%d, server ip=[%s], server port=%d, timeout=%d\n",
        sockId, servIp, servPort, CONNECT_TIMEOUT);
    
    struct sockaddr_in servAddr;
    memset(&servAddr, 0x00, sizeof(struct sockaddr_in));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = inet_addr(servIp);
    servAddr.sin_port = htons(servPort);
    int ret = connect(sockId, (struct sockaddr *)&servAddr, sizeof(struct sockaddr));
    if(0 == ret)
    {
        moLoggerInfo(MOCPS_MODULE_LOGGER_NAME, "Connect succeed once!\n");
    }
    else
    {
        if(errno == EINPROGRESS)
        {
            moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "Socket being blocked yet, we have no ret.\n");
            //Do select here.
            fd_set wFdSet;
            FD_ZERO(&wFdSet);
            FD_SET(sockId, &wFdSet);
            fd_set rFdSet;
            FD_ZERO(&rFdSet);
            FD_SET(sockId, &rFdSet);
            struct timeval tm;
            tm.tv_sec = CONNECT_TIMEOUT;
            tm.tv_usec = 0;
            ret = select(sockId + 1, &rFdSet, &wFdSet, NULL, &tm);
            if(ret < 0)
            {
                moLoggerError(MOCPS_MODULE_LOGGER_NAME, "when connect, select failed! ret = %d, errno = %d, desc = [%s]\n", 
                    ret, errno, strerror(errno));
                return -1;
            }
            else if(ret == 0)
            {
                moLoggerError(MOCPS_MODULE_LOGGER_NAME, "when connect, select timeout!\n");
                return -2;
            }
            else
            {
                //select succeed, but its neccessary to assure its really succeed.
                if(FD_ISSET(sockId, &wFdSet))
                {
                    if(FD_ISSET(sockId, &rFdSet))
                    {
                        moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, 
                            "select ok, but wFdSet and rFdSet all being set, we should check!\n");
                        //check this socket is OK or not.
                        int err = 0;
                        socklen_t errLen;
                        ret = getsockopt(sockId, SOL_SOCKET, SO_ERROR, (void *)&err, &errLen);
                        if(0 != ret)
                        {
                            moLoggerError(MOCPS_MODULE_LOGGER_NAME, 
                                "getsockopt failed! ret = %d, errno = %d, desc = [%s]\n", 
                                ret, errno, strerror(errno));
                            return -3;
                        }
                        else
                        {
                            if(err != 0)
                            {
                                moLoggerError(MOCPS_MODULE_LOGGER_NAME, 
                                    "getsockopt ok, but err = %d, donot mean OK, just mean some error!\n",
                                    err);
                                return -4;
                            }
                            else
                            {
                                moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, 
                                    "getsockopt ok, and err = %d, means connect succeed!\n", err);
                            }
                        }
                    }
                    else
                    {
                        moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, 
                            "select ok, and just wFdSet being set, means connect being OK.\n");
                    }
                }
                else
                {
                    //Just this socket being set to wFdSet, if not this socket, error ocurred!
                    moLoggerError(MOCPS_MODULE_LOGGER_NAME, "select ok, but the fd is not client socket!\n");
                    return -5;
                }
            }
        }
        else
        {
            moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Connect failed! ret = %d, errno = %d, desc = [%s]\n",
                ret, errno, strerror(errno));
            return -6;
        }
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "Connect succeed now!\n");
    
    return 0;
}

/*
    Use data port to create a socket;
    connect to server;
*/
static int createDataSocket(const MOCPS_CLI_CONFINFO confInfo)
{
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, 
        "Server ip = [%s], port = %d, client data port = %d\n",
        confInfo.servIp, confInfo.servPort, confInfo.cliDataPort);

    if(gDataSockId != MOCPS_INVALID_SOCKID)
    {
        moLoggerInfo(MOCPS_MODULE_LOGGER_NAME,
            "Data socket has been created, cannot create it again.\n");
        return MOCPS_CLI_ERR_OK;
    }

    //create socket
    int ret = createSockAndBind(confInfo.cliIp, confInfo.cliDataPort, &gDataSockId);
    if(gDataSockId < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, 
            "createSockAndBind failed! ret = %d\n", ret);
        return MOCPS_CLI_ERR_CREATE_SOCKET;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "createSockAndBind succeed. sockId = %d\n", gDataSockId);

    //set socket attribute
    ret = setSockAttr(gDataSockId);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "setSockAttr to data socket failed! ret = %d\n", ret);
        close(gDataSockId);
        gDataSockId = MOCPS_INVALID_SOCKID;
        return MOCPS_CLI_ERR_SETSOCKATTR;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "setSockAttr succeed.\n");

    //Connect to server in timeout
    ret = connect2Server(gDataSockId, confInfo.servIp, confInfo.servPort);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "connect2Server failed! ret = %d\n", ret);
        close(gDataSockId);
        gDataSockId = MOCPS_INVALID_SOCKID;
        return MOCPS_CLI_ERR_CONN_FAILED;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "connect2Server succeed.\n");

    return MOCPS_CLI_ERR_OK;
}

static void destroyDataSocket()
{
    if(gDataSockId > 0)
    {
        moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "gDataSocket = %d\n", gDataSockId);
        close(gDataSockId);
    }
    gDataSockId = MOCPS_INVALID_SOCKID;
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "DataPort socket has been destroy.\n");
}

/*
    Use ctrl port to create a socket;
    connect to server;
*/
static int createCtrlSocket(const MOCPS_CLI_CONFINFO confInfo)
{
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, 
        "Server ip = [%s], port = %d, client ctrl port = %d\n",
        confInfo.servIp, confInfo.servPort, confInfo.cliCtrlPort);

    if(gCtrlSockId != MOCPS_INVALID_SOCKID)
    {
        moLoggerInfo(MOCPS_MODULE_LOGGER_NAME,
            "Data socket has been created, cannot create it again.\n");
        return MOCPS_CLI_ERR_OK;
    }

    //create socket
    int ret = createSockAndBind(confInfo.cliIp, confInfo.cliCtrlPort, &gCtrlSockId);
    if(gCtrlSockId < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "createSockAndBind failed! ret = %d\n", ret);
        return MOCPS_CLI_ERR_CREATE_SOCKET;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "createSockAndBind succeed. sockId = %d\n", gCtrlSockId);

    //set socket attribute
    ret = setSockAttr(gCtrlSockId);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "setSockAttr to data socket failed! ret = %d\n", ret);
        close(gCtrlSockId);
        gCtrlSockId = MOCPS_INVALID_SOCKID;
        return MOCPS_CLI_ERR_SETSOCKATTR;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "setSockAttr succeed.\n");

    //Connect to server in timeout
    ret = connect2Server(gCtrlSockId, confInfo.servIp, confInfo.servPort);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "connect2Server failed! ret = %d\n", ret);
        close(gCtrlSockId);
        gCtrlSockId = MOCPS_INVALID_SOCKID;
        return MOCPS_CLI_ERR_CONN_FAILED;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "connect2Server succeed.\n");

    return MOCPS_CLI_ERR_OK;
}

static void destroyCtrlSocket()
{
    if(gCtrlSockId> 0)
    {
        moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "gCtrlSockId = %d\n", gCtrlSockId);
        close(gCtrlSockId);
    }
    gCtrlSockId = MOCPS_INVALID_SOCKID;
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "DataPort socket has been destroy.\n");
}

static int sayByebye2Server()
{
    //generate a byebye request
    MOCPS_CTRL_REQUEST req;
    memset(&req, 0x00, sizeof(MOCPS_CTRL_REQUEST));
    strncpy(req.basicInfo.mark, MOCPS_MARK_CLIENT, MOCPS_MARK_MAXLEN);
    req.basicInfo.mark[MOCPS_MARK_MAXLEN - 1] = 0x00;
    req.basicInfo.isNeedResp = MOCPS_REQUEST_TYPE_NOTNEED_RESPONSE;
    req.basicInfo.cmdId = MOCPS_CMDID_BYEBYE;
    moUtils_Check_getCrc((char *)&req.basicInfo, sizeof(MOCPS_CTRL_REQUEST_BASIC),
        MOUTILS_CHECK_CRCMETHOD_32, &req.crc32);

    //do crypt to this request
    char *pCipher = NULL;
    pCipher = (char *)malloc(sizeof(MOCPS_CTRL_REQUEST) * sizeof(char));
    if(NULL == pCipher)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Malloc for cipher byebye failed." \
            "errno = %d, desc = [%s]\n", errno, strerror(errno));
        return -1;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "Malloc for cipher byebye succeed.\n");
    int ret = doEncrypt2Req(req, pCipher);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "doEncrypt2Req failed! ret = %d\n", ret);
        free(pCipher);
        pCipher = NULL;
        return -2;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "doEncrypt2Req succeed.\n");
    
    //send this request to moCpsServer
    int writeLen = writen(gCtrlSockId, pCipher, sizeof(MOCPS_CTRL_REQUEST));
    if(writeLen != sizeof(MOCPS_CTRL_REQUEST))
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "send byebye failed! writeLen = %d\n", writeLen);
        free(pCipher);
        pCipher = NULL;
        return -2;
    }

    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "send byebye to moCpsServer ok.\n");
    free(pCipher);
    pCipher = NULL;
    return 0;
}

/*
    Each 1 second, do a loop, this can help us to stop thread;
*/
static void * heartBeatThr(void * args)
{
    args = args;
    
    //generate a heartbeat request
    MOCPS_CTRL_REQUEST req;
    memset(&req, 0x00, sizeof(MOCPS_CTRL_REQUEST));
    strncpy(req.basicInfo.mark, MOCPS_MARK_CLIENT, MOCPS_MARK_MAXLEN);
    req.basicInfo.mark[MOCPS_MARK_MAXLEN - 1] = 0x00;
    req.basicInfo.isNeedResp = MOCPS_REQUEST_TYPE_NOTNEED_RESPONSE;
    req.basicInfo.cmdId = MOCPS_CMDID_HEARTBEAT;
    moUtils_Check_getCrc((char *)&req.basicInfo, sizeof(MOCPS_CTRL_REQUEST_BASIC),
        MOUTILS_CHECK_CRCMETHOD_32, &req.crc32);

    //do crypt to this request
    char *pCipher = NULL;
    pCipher = (char *)malloc(sizeof(MOCPS_CTRL_REQUEST) * sizeof(char));
    if(NULL == pCipher)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Malloc for cipher heartbeat failed." \
            "errno = %d, desc = [%s]\n", errno, strerror(errno));
        return NULL;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "Malloc for cipher heartbeat succeed.\n");
    int ret = doEncrypt2Req(req, pCipher);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "doEncrypt2Req failed! ret = %d\n", ret);
        free(pCipher);
        pCipher = NULL;
        return NULL;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "doEncrypt2Req succeed.\n");

    int waitTime = MOCPS_HEARTBEAT_INTEVAL;
    
    while(1)
    {
        if(gHeartbeatThrRunningFlag)
        {
            if(waitTime >= MOCPS_HEARTBEAT_INTEVAL)
            {
                //send this request to moCpsServer
                int writeLen = writen(gCtrlSockId, pCipher, sizeof(MOCPS_CTRL_REQUEST));
                if(writeLen != sizeof(MOCPS_CTRL_REQUEST))
                {
                    moLoggerError(MOCPS_MODULE_LOGGER_NAME, "send heartbeat failed! writeLen = %d\n", writeLen);
                    free(pCipher);
                    pCipher = NULL;
                    gHeartbeatThrRunningFlag = 0;
                    gHeartbeatThrRunningState = THREAD_STATE_STOPPED;
                    break;
                }
                moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "send heartbeat ok.\n");
                waitTime = 0;
                continue;
            }
            else
            {
                sleep(THREAD_TIME_INTEVAL);
                waitTime += THREAD_TIME_INTEVAL;
                continue;
            }
        }
        else
        {
            moLoggerInfo(MOCPS_MODULE_LOGGER_NAME, 
                "gHeartbeatThrRunningFlag=%d, will exit heartbeat thread now.\n",
                gHeartbeatThrRunningFlag);
            free(pCipher);
            pCipher = NULL;
            gHeartbeatThrRunningState = THREAD_STATE_STOPPED;
            break;
        }
    }
    
    return NULL;
}

static int createHeartBeatThr()
{
    gHeartbeatThrRunningFlag = 1;
    gHeartbeatThrRunningState = THREAD_STATE_RUNNING;
    
    pthread_t heartbeatThrId;
    int ret = pthread_create(&heartbeatThrId, NULL, heartBeatThr, NULL);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "create heartbeat thread failed!" \
            "ret = %d, errno = %d, desc = [%s]\n", ret, errno, strerror(errno));
        gHeartbeatThrRunningFlag = 0;
        gHeartbeatThrRunningState = THREAD_STATE_STOPPED;
        return -1;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "heartbeat thread create succeed.\n");
    return 0;
}

static void stopHeartBeatThr()
{
    if(gHeartbeatThrRunningFlag == 1)
    {        
        gHeartbeatThrRunningFlag = 0;
        while(1)
        {
            if(gHeartbeatThrRunningState != THREAD_STATE_STOPPED)
            {
                usleep(300 * 1000); //300ms
                continue;
            }
            else
            {
                moLoggerInfo(MOCPS_MODULE_LOGGER_NAME, "HeartBeat thread being stopped yet!\n");
                break;
            }
        }
    }
    else
    {
        moLoggerInfo(MOCPS_MODULE_LOGGER_NAME, "HeartBeat thread donot running, cannot stop!\n");
    }
}

/*
    data port should send to moCpsServer firstly;
*/
static int sendDataport2Server(const int dataport)
{
    //generate a byebye request
    MOCPS_CTRL_REQUEST req;
    memset(&req, 0x00, sizeof(MOCPS_CTRL_REQUEST));
    strncpy(req.basicInfo.mark, MOCPS_MARK_CLIENT, MOCPS_MARK_MAXLEN);
    req.basicInfo.mark[MOCPS_MARK_MAXLEN - 1] = 0x00;
    req.basicInfo.isNeedResp = MOCPS_REQUEST_TYPE_NEED_RESPONSE;
    req.basicInfo.cmdId = MOCPS_CMDID_SEND_DATAPORT;
    splitInt2Char(dataport, req.basicInfo.res);
    moUtils_Check_getCrc((char *)&req.basicInfo, sizeof(MOCPS_CTRL_REQUEST_BASIC),
        MOUTILS_CHECK_CRCMETHOD_32, &req.crc32);

    //do crypt to this request
    char *pCipher = NULL;
    pCipher = (char *)malloc(sizeof(MOCPS_CTRL_REQUEST) * sizeof(char));
    if(NULL == pCipher)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Malloc for cipher sendDataport failed." \
            "errno = %d, desc = [%s]\n", errno, strerror(errno));
        return -1;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "Malloc for cipher sendDataport succeed.\n");
    int ret = doEncrypt2Req(req, pCipher);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "doEncrypt2Req failed! ret = %d\n", ret);
        free(pCipher);
        pCipher = NULL;
        return -2;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "doEncrypt2Req succeed.\n");
    
    //send this request to moCpsServer
    int writeLen = writen(gCtrlSockId, pCipher, sizeof(MOCPS_CTRL_REQUEST));
    if(writeLen != sizeof(MOCPS_CTRL_REQUEST))
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "sendDataport failed! writeLen = %d\n", writeLen);
        free(pCipher);
        pCipher = NULL;
        return -3;
    }
    
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "sendDataport ok.\n");
    free(pCipher);
    pCipher = NULL;

    //should recv response from moCpsServer
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "Need response from server, get it now.\n");
    MOCPS_CTRL_RESPONSE resp;
    memset(&resp, 0x00, sizeof(MOCPS_CTRL_RESPONSE));
    ret = getRespFromServer(MOCPS_CMDID_SEND_DATAPORT, &resp);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "getRespFromServer failed! ret = %d\n", ret);
        return -4;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "getRespFromServer ok.\n");
    
    return 0;
}

/*
    Check @header in right format or not;
*/
static int isRightFormatHeader(const MOCPS_DATA_RESPONSE_HEADER header)
{
    if(strcmp(header.mark, MOCPS_MARK_SERVER) != 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Mark=[%s], donot [%s], not a right format header!\n",
            header.mark, MOCPS_MARK_SERVER);
        return 0;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "Mark ok.\n");

    if(header.cmdId >= MOCPS_CMDID_MAX)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "CmdId=%d, larger than %d, not a right format header!\n",
            header.cmdId, MOCPS_CMDID_MAX);
        return 0;        
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "CmdId ok.\n");

    int ret = moUtils_Check_checkSum((unsigned char *)&header, 
        sizeof(MOCPS_DATA_RESPONSE_HEADER) - sizeof(MOUTILS_CHECK_CRCVALUE), 
        header.checksum);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, 
            "moUtils_Check_checkSum failed! ret = %d\n", ret);
        return 0;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "header check ok.\n");
    return 1;
}

/*
    Recv from server looply util find a header in right format;
    then decrypt it, and set to pHeader;
*/
static int recvHeadAndCheck(MOCPS_DATA_RESPONSE_HEADER *pHeader)
{
    int cipherOffset = 0;
    int cipherLen = sizeof(MOCPS_DATA_RESPONSE_HEADER);
    int ret = 0;    
    MOCPS_DATA_RESPONSE_HEADER cipher, plain;
    memset(&cipher, 0x00, sizeof(MOCPS_DATA_RESPONSE_HEADER));
    memset(&plain, 0x00, sizeof(MOCPS_DATA_RESPONSE_HEADER));
    while(1)
    {
        //1.read cipher text firstly
        ret = readn(gDataSockId, (char *)&cipher + cipherOffset, cipherLen);
        if(ret != cipherLen)
        {
            moLoggerError(MOCPS_MODULE_LOGGER_NAME, "readn failed! readLen=%d, cipherLen=%d\n",
                ret, cipherLen);
            ret = -1;
            break;
        }
        moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "readn succeed.\n");
        //2.decrypt to it
        memcpy(&plain, &cipher, sizeof(MOCPS_DATA_RESPONSE_HEADER));
        ret = doDecrypt2Resp((char *)&cipher + cipherOffset, (char *)&plain + cipherOffset, cipherLen);
        if(ret < 0)
        {
            moLoggerError(MOCPS_MODULE_LOGGER_NAME, "decrypt failed! ret = %d\n", ret);
            ret = -2;
            break;
        }
        moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "decrypt succeed.\n");
        //3.find mark
        ret = moUtils_Search_BF((unsigned char *)&plain, sizeof(MOCPS_DATA_RESPONSE_HEADER),
            (unsigned char *)MOCPS_MARK_SERVER, strlen(MOCPS_MARK_SERVER));
        if(ret < 0)
        {
            moLoggerWarn(MOCPS_MODULE_LOGGER_NAME, "moUtils_Search_BF failed! ret = %d\n", ret);
            //reset cipher offset and length, find it looply
            memset(&cipher, 0x00, sizeof(MOCPS_DATA_RESPONSE_HEADER));
            memcpy(&cipher, &plain + (sizeof(MOCPS_DATA_RESPONSE_HEADER) - strlen(MOCPS_MARK_SERVER) + 1), 
                strlen(MOCPS_MARK_SERVER) - 1);
            memset(&plain, 0x00, sizeof(MOCPS_DATA_RESPONSE_HEADER));
            cipherOffset = strlen(MOCPS_MARK_SERVER) - 1;
            cipherLen = sizeof(MOCPS_DATA_RESPONSE_HEADER) - strlen(MOCPS_MARK_SERVER) + 1;
            moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "New loop, cipherOffset=%d, cipherLen=%d\n",
                cipherOffset, cipherLen);
            continue;
        }
        else if(ret == 0)
        {
            moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, 
                "Find mark at the beginning! Now start check it in right format or not!\n");
            //check its format
            if(isRightFormatHeader(plain))
            {
                moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "This header is a right format header!\n");
                memcpy(pHeader, &plain, sizeof(MOCPS_DATA_RESPONSE_HEADER));
                ret = 0;
                break;
            }
            else
            {
                moLoggerWarn(MOCPS_MODULE_LOGGER_NAME, "This donot a valid header format!\n");
                //reset cipher offset and length, find it looply
                memset(&cipher, 0x00, sizeof(MOCPS_DATA_RESPONSE_HEADER));
                memcpy(&cipher, &plain + (sizeof(MOCPS_DATA_RESPONSE_HEADER) - strlen(MOCPS_MARK_SERVER) + 1), 
                    strlen(MOCPS_MARK_SERVER) - 1);
                memset(&plain, 0x00, sizeof(MOCPS_DATA_RESPONSE_HEADER));
                cipherOffset = strlen(MOCPS_MARK_SERVER) - 1;
                cipherLen = sizeof(MOCPS_DATA_RESPONSE_HEADER) - strlen(MOCPS_MARK_SERVER) + 1;
                moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "New loop, cipherOffset=%d, cipherLen=%d\n",
                    cipherOffset, cipherLen);
                continue;
            }
        }
        else    //Find a mark, but donot at the beginning of this header
        {
            moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, 
                "Find mark in the middle of this header, should recv the next data now.\n");
            memset(&cipher, 0x00, sizeof(MOCPS_DATA_RESPONSE_HEADER));
            memcpy(&cipher, &plain + ret, sizeof(MOCPS_DATA_RESPONSE_HEADER) - ret);
            memset(&plain, 0x00, sizeof(MOCPS_DATA_RESPONSE_HEADER));
            cipherOffset = sizeof(MOCPS_DATA_RESPONSE_HEADER) - ret;
            cipherLen = ret;
            continue;
        }
    }
    
    return ret;    
}

/*
    Recv a block from server with length @blkLen, and set to @pBlk
*/
static int recvBody(const int blkLen, char *pBlk)
{
    if(NULL == pBlk)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Input param is NULL!\n");
        return -1;
    }
    int readLen = readn(gDataSockId, pBlk, blkLen);
    if(readLen != blkLen)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "readn failed! readLen=%d, blkLen=%d\n",
            readLen, blkLen);
        return -2;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "read block succeed.\n");
    
    return 0;
}

/*
    Each 1 second, do a loop, this can help us to stop thread;
    1.get data;
    2.parse data(decrypt), get header;
    3.get length of body, then recv body;
    4.goto step1;
*/
static void * recvDataThr(void * args)
{
    args = args;
    
    RECVDATA_STATE recvState = RECVDATA_STATE_RECVHEAD;
    MOCPS_DATA_RESPONSE_HEADER header;

    int ret = 0;

    while(1)
    {
        if(gRecvdataThrRunningFlag)
        {
            fd_set rFdSet;
            FD_ZERO(&rFdSet);
            FD_SET(gDataSockId, &rFdSet);

            struct timeval tm;
            tm.tv_sec = THREAD_TIME_INTEVAL;
            tm.tv_usec = 0;
            ret = select(gDataSockId + 1, &rFdSet, NULL, NULL, &tm);
            if(ret < 0)
            {
                moLoggerError(MOCPS_MODULE_LOGGER_NAME, "select failed! ret=%d, errno=%d, desc=[%s]\n",
                    ret, errno, strerror(errno));
                break;
            }
            else if(ret == 0)
            {
                //timeout
                continue;
            }
            else
            {
                if(recvState == RECVDATA_STATE_RECVHEAD)
                {
                    memset(&header, 0x00, sizeof(MOCPS_DATA_RESPONSE_HEADER));
                    ret = recvHeadAndCheck(&header);
                    if(ret < 0)
                    {
                        //Server closed!!
                        moLoggerError(MOCPS_MODULE_LOGGER_NAME,
                            "recvHeadAndCheck failed! ret = %d\n", ret);
                        exit(-1);
                    }
                    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "Recv head ok.\n");
                    //change state, to recv body
                    recvState = RECVDATA_STATE_RECVBODY;
                }
                else
                {
                    char curBlk[MOCPS_DATA_BODY_BLK_MAXSIZE] = {0x00};
                    memset(curBlk, 0x00, MOCPS_DATA_BODY_BLK_MAXSIZE);
                    ret = recvBody(header.curBlkLen, curBlk);
                    if(ret < 0)
                    {
                        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "recv body failed!\n");
                        recvState = RECVDATA_STATE_RECVHEAD;
                        continue;
                    }
                    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "recv body succeed.\n");
                    //data memory manager, save a complete data in local memory, if sequence id changed,
                    //and current data donot complete yet, we will drop it!!
                    dmmSaveCurBodyBlk(header, curBlk);
                    //If current data is completed, call function, throw it to outside for using
                    if(dmmIsCompleteData())
                    {
                        ret = gpDataCallbackFunc(dmmDataPtr(), header.totalLen);
                        if(ret < 0)
                        {
                            moLoggerError(MOCPS_MODULE_LOGGER_NAME, 
                                "gpDataCallbackFunc failed, ret = %d\n", ret);
                        }
                        else
                            moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "gpDataCallbeckFunc succeed.\n");

                        dmmClear();
                        recvState = RECVDATA_STATE_RECVHEAD;
                    }
                }
            }
        }
        else
        {
            //Need stop this thread
            moLoggerInfo(MOCPS_MODULE_LOGGER_NAME, "Should stop recvData thread now.\n");
            gRecvdataThrRunningState = THREAD_STATE_STOPPED;
            break;
        }
    }
    
    return NULL;
}

static int createRecvDataThr()
{
    gRecvdataThrRunningFlag = 1;
    gRecvdataThrRunningState = THREAD_STATE_RUNNING;
    
    pthread_t recvdataThrId;
    int ret = pthread_create(&recvdataThrId, NULL, recvDataThr, NULL);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "create recvdata thread failed!" \
            "ret = %d, errno = %d, desc = [%s]\n", ret, errno, strerror(errno));
        gRecvdataThrRunningFlag = 0;
        gRecvdataThrRunningState = THREAD_STATE_STOPPED;
        return -1;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "recvdata thread create succeed.\n");
    return 0;
}

static void stopRecvDataThr()
{
    if(gRecvdataThrRunningFlag == 1)
    {        
        gRecvdataThrRunningFlag = 0;
        while(1)
        {
            if(gRecvdataThrRunningState != THREAD_STATE_STOPPED)
            {
                usleep(300 * 1000); //300ms
                continue;
            }
            else
            {
                moLoggerInfo(MOCPS_MODULE_LOGGER_NAME, "RecvData thread being stopped yet!\n");
                break;
            }
        }
    }
    else
    {
        moLoggerInfo(MOCPS_MODULE_LOGGER_NAME, "RecvData thread donot running, cannot stop!\n");
    }
}

/*
    Use private key, to decrypt @pRespCipher to pRespPlain;
*/
static int decryptKeyAgreeResponse(const char * pRespCipher, MOCPS_KEYAGREE_RESPONSE * pRespPlain)
{
    //TODO,this just a stub, we will use RSA to do it later.
    memcpy(pRespPlain, pRespCipher, sizeof(MOCPS_KEYAGREE_RESPONSE));
    
    return 0;
}

static int checkKeyagreeResp(const MOCPS_KEYAGREE_RESPONSE resp)
{
    if(resp.cmdId != MOCPS_CMDID_KEYAGREE)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "input cmdid=%d, donot MOCPS_CMDID_KEYAGREE(%d)\n",
            resp.cmdId, MOCPS_CMDID_KEYAGREE);
        return -1;
    }
    int ret = moUtils_Check_checkCrc(
        (unsigned char *)&resp, sizeof(MOCPS_KEYAGREE_RESPONSE) - sizeof(MOUTILS_CHECK_CRCVALUE),
        MOUTILS_CHECK_CRCMETHOD_32, resp.crc32);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "moUtils_Check_checkCrc failed! ret = %d\n", ret);
        return -2;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "check key agree response ok.\n");
    return 0;
}

/*
    Use ctrl socket, to send a keyagree request to moCpsServer, then parse its response,
    get crypt infomation, set to @pCryptInfo;
*/
static int doKeyAgree()
{
    //1.generate request
    MOCPS_CTRL_REQUEST req;
    memset(&req, 0x00, sizeof(MOCPS_CTRL_REQUEST));
    strncpy(req.basicInfo.mark, MOCPS_MARK_CLIENT, MOCPS_MARK_MAXLEN);
    req.basicInfo.mark[MOCPS_MARK_MAXLEN - 1] = 0x00;
    req.basicInfo.isNeedResp = MOCPS_REQUEST_TYPE_NEED_RESPONSE;
    req.basicInfo.cmdId = MOCPS_CMDID_KEYAGREE;
    moUtils_Check_getCrc((char *)&req.basicInfo, sizeof(MOCPS_CTRL_REQUEST_BASIC),
        MOUTILS_CHECK_CRCMETHOD_32, &req.crc32);
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "generate request for KEYAGREE succeed.\n");

    //2.send to moCpsServer
    int writeLen = writen(gCtrlSockId, (char *)&req, sizeof(MOCPS_CTRL_REQUEST));
    if(writeLen != sizeof(MOCPS_CTRL_REQUEST))
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Send keyagree request to server failed! ret = %d\n", writeLen);
        return -1;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "Send keyagree request to server succeed.\n");

    //3.wait for response in timeout
    int waitTime = 0;
    int ret = 0;
    MOCPS_KEYAGREE_RESPONSE resp;
    memset(&resp, 0x00, sizeof(MOCPS_KEYAGREE_RESPONSE));
    while(waitTime <= KEYAGREE_TIMEOUT)
    {
        fd_set rFdSet;
        FD_ZERO(&rFdSet);
        FD_SET(gCtrlSockId, &rFdSet);
        struct timeval tm;
        tm.tv_sec = 1;
        tm.tv_usec = 0;
        ret = select(gCtrlSockId + 1, &rFdSet, NULL, NULL, &tm);
        if(ret < 0)
        {
            moLoggerError(MOCPS_MODULE_LOGGER_NAME, "select failed! ret = %d, errno = %d, desc = [%s]\n",
                ret, errno, strerror(errno));
            return -2;
        }
        else if(ret == 0)
        {
            moLoggerWarn(MOCPS_MODULE_LOGGER_NAME, "select timeout, current waitTime = %d\n", waitTime);
            waitTime++;
            continue;
        }
        else
        {
            if(FD_ISSET(gCtrlSockId, &rFdSet))
            {
                moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "Get keyagree response from server.\n");

                int readLen = readn(gCtrlSockId, (char *)&resp, sizeof(MOCPS_KEYAGREE_RESPONSE));
                if(readLen != sizeof(MOCPS_KEYAGREE_RESPONSE))
                {
                    moLoggerError(MOCPS_MODULE_LOGGER_NAME, "readn failed! readLen = %d, size = %d\n",
                        readLen, sizeof(MOCPS_KEYAGREE_RESPONSE));
                    return -3;
                }
                moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "read keyagree original response succeed.\n");
                break;
            }
            else
            {            
                moLoggerWarn(MOCPS_MODULE_LOGGER_NAME, "Wait for keyagree response, but server donot send it, error.\n");
                return -4;
            }
        }
    }
    if(waitTime > KEYAGREE_TIMEOUT)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "KeyAgree failed! donot get response in %d seconds.\n",
            KEYAGREE_TIMEOUT);
        return -5;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "get keyagree response waste time %d seconds.\n", waitTime);

    //4.decrypt the response to plain text
    MOCPS_KEYAGREE_RESPONSE respPlain;
    memset(&respPlain, 0x00, sizeof(MOCPS_KEYAGREE_RESPONSE));
    ret = decryptKeyAgreeResponse((char * )&resp, &respPlain);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "decryptKeyAgreeResponse failed! ret = %d\n", ret);
        return -6;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "decryptKeyAgreeResponse OVER.\n");

    //5.check the response in right format or not
    ret = checkKeyagreeResp(respPlain);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "checkKeyagreeResp failed! ret = %d\n", ret);
        return -7;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "checkKeyagreeResp ok.\n");

    //6.get cryptInfo we needed
    memcpy(&gCryptInfo, &respPlain.cryptInfo, sizeof(MOCPS_CRYPT_INFO));
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "Set crypt info to local memory ok.\n");
    
    return 0;
}

/*
    1.get config info from @pConfFilepath;
    2.Create a ctrl socket using ctrl port which we get from @pConfFilepath, then connect to server;
    3.Do KeyAgree to moCpsServer, and get crypt info this time;
    4.Create our heartbeat thread to send heartbeat to moCpsServer;
    5.Use ctrl socket, send a request to moCpsServer, tell it the data port value;
    6.Create a data socket using data port which we get from @pConfFilepath, then connect to server;
    7.Create thread RecvData to recv data when we needed;
*/
int moCpsCli_init(const char* pConfFilepath, const pDataCallbackFunc pFunc)
{
    if(NULL == pConfFilepath || NULL == pFunc)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Input param is NULL!\n");
        return MOCPS_CLI_ERR_INPUT_NULL;
    }
    
    pthread_mutex_lock(&gMutex);

    if(gIsInited)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "MoCpsClient has been inited yet, cannot init again!\n");
        pthread_mutex_unlock(&gMutex);
        return MOCPS_CLI_ERR_DUP_INIT;
    }

    //do data memory manager init firstly, if memory cannot malloced, other will not be done
    int ret = dmmInit();
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "dmmInit failed, ret = %d\n", ret);
        return MOCPS_CLI_ERR_INTERNAL;
    }
    moLoggerError(MOCPS_MODULE_LOGGER_NAME, "dmmInit succeed.\n");

    //do ctrl response memory manager init firstly, if memory cannot malloced, other will not be done
    ret = crmmInit();
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "crmmInit failed, ret = %d\n", ret);
        return MOCPS_CLI_ERR_INTERNAL;
    }
    moLoggerError(MOCPS_MODULE_LOGGER_NAME, "crmmInit succeed.\n");

    //1.get config info
    MOCPS_CLI_CONFINFO confInfo;
    memset(&confInfo, 0x00, sizeof(MOCPS_CLI_CONFINFO));
    ret = parseConf(pConfFilepath, &confInfo);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "parseConf failed! ret = %d, pConfFilepath = [%s]\n",
            ret, pConfFilepath);
        crmmUnInit();
        dmmUnInit();
        pthread_mutex_unlock(&gMutex);
        return MOCPS_CLI_ERR_PARSECONF_FAILED;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "parseConf succeed.\n");
    dumpConfInfo(confInfo);

    //2.create ctrl socket, connect to server
    ret = createCtrlSocket(confInfo);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "createCtrlSocket failed! ret = %d\n",
            ret);
        crmmUnInit();
        dmmUnInit();
        pthread_mutex_unlock(&gMutex);
        return MOCPS_CLI_ERR_CREATESOCK_FAILED;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "createCtrlSocket succeed.\n");

    //3.do keyagree to moCpsServer
    ret = doKeyAgree();
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "doKeyAgree failed! ret = %d\n",
            ret);
        sayByebye2Server();
        destroyCtrlSocket();
        crmmUnInit();
        dmmUnInit();
        pthread_mutex_unlock(&gMutex);
        return MOCPS_CLI_ERR_CREATETHR_FAILED;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "doKeyAgree succeed.\n");

    //4.create heartbeat thread
    ret = createHeartBeatThr();
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "createHeartBeatThr failed! ret = %d\n",
            ret);
        sayByebye2Server();
        destroyCtrlSocket();
        crmmUnInit();
        dmmUnInit();
        pthread_mutex_unlock(&gMutex);
        return MOCPS_CLI_ERR_CREATETHR_FAILED;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "createHeartBeatThr succeed.\n");

    //5.send a msg to moCpsServer, tell it the dataPort value
    ret = sendDataport2Server(confInfo.cliDataPort);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "sendDataport2Server failed! ret = %d\n",
            ret);
        stopHeartBeatThr();
        sayByebye2Server();
        destroyCtrlSocket();
        crmmUnInit();
        dmmUnInit();
        pthread_mutex_unlock(&gMutex);
        return MOCPS_CLI_ERR_CREATETHR_FAILED;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "sendDataport2Server succeed.\n");

    //6.create data socket
    ret = createDataSocket(confInfo);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "createDataSocket failed! ret = %d\n",
            ret);
        stopHeartBeatThr();
        sayByebye2Server();
        destroyCtrlSocket();
        crmmUnInit();
        dmmUnInit();
        pthread_mutex_unlock(&gMutex);
        return MOCPS_CLI_ERR_CREATESOCK_FAILED;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "createDataSocket succeed.\n");

    //7.start recvDataThread
    ret = createRecvDataThr();
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "createRecvDataThr failed! ret = %d\n",
            ret);
        stopHeartBeatThr();
        sayByebye2Server();
        destroyCtrlSocket();
        destroyDataSocket();
        crmmUnInit();
        dmmUnInit();
        pthread_mutex_unlock(&gMutex);
        return MOCPS_CLI_ERR_CREATETHR_FAILED;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "createRecvDataThr succeed.\n");
    
    //8.set pFunc to global memory
    gpDataCallbackFunc = pFunc;
    gIsInited = INIT_STATE_YES;

    pthread_mutex_unlock(&gMutex);
    
    return MOCPS_CLI_ERR_OK;
}

void moCpsCli_unInit()
{
    pthread_mutex_lock(&gMutex);
    
    if(!gIsInited)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "MoCpsClient donot init yet, cannot unInit to it!\n");
        pthread_mutex_unlock(&gMutex);
        return ;
    }
    
    stopHeartBeatThr();
    sayByebye2Server();
    stopRecvDataThr();
    destroyCtrlSocket();
    destroyDataSocket();
    gpDataCallbackFunc = NULL;
    crmmUnInit();
    dmmUnInit();
    gIsInited = INIT_STATE_NO;
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "moCpsCli_unInit ok.\n");
    
    pthread_mutex_unlock(&gMutex);
}

static int checkRequestParams(const unsigned char cmdId, const MOCPS_REQUEST_TYPE isNeedResp, 
    MOCPS_CTRL_RESPONSE * pCtrlResp)
{
    if(cmdId >= MOCPS_CMDID_MAX)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, 
            "Input cmdId invalid! its value %d, max value we allowed is %d\n",
            cmdId, MOCPS_CMDID_MAX);
        return -1;
    }

    if(isNeedResp == MOCPS_REQUEST_TYPE_NEED_RESPONSE && NULL == pCtrlResp)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME,
            "Need response, but param pCtrlResp is NULL!\n");
        return -2;
    }

    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "Param all ok. cmdId=%d, isNeedResp=%d\n",
        cmdId, isNeedResp);
    
    return 0;

}

/*
    Generate a request in our format;
*/
static int genCtrlReq(const MOCPS_CMDID cmdId, const MOCPS_REQUEST_TYPE isNeedResp, 
    MOCPS_CTRL_REQUEST *pReq)
{
    if(NULL == pReq)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Input param is NULL.\n");
        return -1;
    }
    memset(pReq, 0x00, sizeof(MOCPS_CTRL_REQUEST));
    strncpy(pReq->basicInfo.mark, MOCPS_MARK_CLIENT, MOCPS_MARK_MAXLEN);
    pReq->basicInfo.mark[MOCPS_MARK_MAXLEN - 1] = 0x00;
    pReq->basicInfo.cmdId = cmdId;
    pReq->basicInfo.isNeedResp = isNeedResp;
    int ret = moUtils_Check_getCrc((char *)&pReq->basicInfo, sizeof(MOCPS_CTRL_REQUEST_BASIC),
        MOUTILS_CHECK_CRCMETHOD_32, &pReq->crc32);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "moUtils_Check_getCrc failed! ret = %d\n", ret);
        return -2;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "moUtils_Check_getCrc ok.\n");
    return 0;
}

/*
    Use ctrl port, send request to server;
*/
static int sendReq2Server(const char * buf, const int bufLen)
{
    if(NULL == buf)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Input param is NULL!\n");
        return -1;
    }

    int writeLen = writen(gCtrlSockId, buf, bufLen);
    if(writeLen != bufLen)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "writen failed! writeLen = %d, bufLen = %d\n", writeLen, bufLen);
        return -2;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "writen succeed.\n");
    return 0;
}

/*
    After send request to moCpsServer, if we need a response from server,
    use this function to get it.

    1.wait response in timeout;
    2.decrypt to response;
*/
static int getRespFromServer(const MOCPS_CMDID cmdId, MOCPS_CTRL_RESPONSE * pCtrlResp)
{
    if(NULL == pCtrlResp)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Input param is NULL!\n");
        return -1;        
    }

    while(1)
    {
        //1.get response from server;
        MOCPS_CTRL_RESPONSE_BASIC basicRespCipher;
        memset(&basicRespCipher, 0x00, sizeof(MOCPS_CTRL_RESPONSE_BASIC));

        fd_set rFdSet;
        FD_ZERO(&rFdSet);
        FD_SET(gCtrlSockId, &rFdSet);
        struct timeval tm;
        tm.tv_sec = WAIT_RESP_TIMEOUT;
        tm.tv_usec = 0;
        int ret = select(gCtrlSockId + 1, &rFdSet, NULL, NULL, &tm);
        if(ret < 0)
        {
            moLoggerError(MOCPS_MODULE_LOGGER_NAME, "select failed! ret = %d, errno = %d, desc = [%s]\n",
                ret, errno, strerror(errno));
            return -3;
        }
        else if(ret == 0)
        {
            moLoggerError(MOCPS_MODULE_LOGGER_NAME, "select timeout!\n");
            return -4;
        }
        //just read this socket, because we donot monitor other sockets....haha, so FD_ISSET is not necessary
        char * pCipher = (char * )&basicRespCipher;
        int readLen = readn(gCtrlSockId, pCipher, sizeof(MOCPS_CTRL_RESPONSE_BASIC));
        if(readLen != sizeof(MOCPS_CTRL_RESPONSE_BASIC))
        {
            moLoggerError(MOCPS_MODULE_LOGGER_NAME, "readn failed! readLen=%d, size=%d\n",
                readLen, sizeof(MOCPS_CTRL_RESPONSE_BASIC));
            return -5;
        }
        moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "Read cipher response from server succeed.\n");

        //find the MARK position
        int pos = moUtils_Search_BF((unsigned char *)pCipher, sizeof(MOCPS_CTRL_RESPONSE_BASIC),
            (unsigned char *)MOCPS_MARK_SERVER, strlen(MOCPS_MARK_SERVER));
        if(pos < 0)
        {
            moLoggerError(MOCPS_MODULE_LOGGER_NAME, "moUtils_Search_BF failed! ret = %d\n", ret);
            return -6;
        }
        moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "Find the MARK, its position is %d\n", pos);
        if(pos != 0)
        {
            //move the MARK to the beginning firstly
            memmove(pCipher, pCipher + pos, sizeof(MOCPS_CTRL_RESPONSE_BASIC) - pos);

            //read the bytes then
            readLen = readn(gCtrlSockId, pCipher + sizeof(MOCPS_CTRL_RESPONSE_BASIC) - pos, pos);
            if(readLen != pos)
            {
                moLoggerError(MOCPS_MODULE_LOGGER_NAME, 
                    "readn for left context failed! readLen=%d, pos=%d\n",
                    readLen, pos);
                return -7;
            }
            moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "readn for left context succeed.\n");
        }
        
        //2.decrypt the cipher to plain
        MOCPS_CTRL_RESPONSE_BASIC basicRespPlain;
        memset(&basicRespPlain, 0x00, sizeof(MOCPS_CTRL_RESPONSE_BASIC));
        ret = doDecrypt2Resp(pCipher, (char *)&basicRespPlain, sizeof(MOCPS_CTRL_RESPONSE_BASIC));
        if(ret < 0)
        {
            moLoggerError(MOCPS_MODULE_LOGGER_NAME, "doDecrypt2Resp failed! ret = %d\n", ret);
            return -7;
        }
        moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "doDecrypt2Resp succeed.\n");
        
        //3.Check this response is we need or not
        if(cmdId != basicRespPlain.cmdId)
        {
            moLoggerError(MOCPS_MODULE_LOGGER_NAME, "This donot the response we need! " \
                "cmdId we needed is %d, pCtrlResp->basicInfo.cmdId = %d", 
                cmdId, basicRespPlain.cmdId);
            return -8;
        }
        moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "Get the response we need!\n");

        //4.Check crc
        if((ret = moUtils_Check_checkCrc((unsigned char *)&basicRespPlain,
            sizeof(MOCPS_CTRL_RESPONSE_BASIC) - sizeof(MOUTILS_CHECK_CRCVALUE),
            MOUTILS_CHECK_CRCMETHOD_32, basicRespPlain.crc32)) != 0)
        {
            moLoggerError(MOCPS_MODULE_LOGGER_NAME, "CRC check failed! ret = %d\n", ret);
            return -9;
        }
        moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "CRC check ok!\n");
        memcpy(&pCtrlResp->basicInfo, &basicRespPlain, sizeof(MOCPS_CTRL_RESPONSE_BASIC));
        
        break;
    }

    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "resp mark=[%s], cmdId=%d, body len = %d\n", 
        pCtrlResp->basicInfo.mark, pCtrlResp->basicInfo.cmdId, pCtrlResp->basicInfo.bodyLen);
    //Then, we must get the body
    if(pCtrlResp->basicInfo.bodyLen == 0)
    {
        moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "This response body length is 0, will not have body.\n");
        pCtrlResp->pBody = NULL;
    }
    else if(pCtrlResp->basicInfo.bodyLen >= crmmMaxSize())
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "bodyLen=%d, too larger! The max we allowed is %d\n",
            pCtrlResp->basicInfo.bodyLen, crmmMaxSize());
        return -10;
    }
    else
    {
        char * pBodyCipher = (char *)malloc(pCtrlResp->basicInfo.bodyLen);
        if(NULL == pBodyCipher)
        {
            moLoggerError(MOCPS_MODULE_LOGGER_NAME, 
                "malloc for pBodyCipher failed! errno=%d, desc=[%s], size=%d\n",
                errno, strerror(errno), pCtrlResp->basicInfo.bodyLen);
            return -11;
        }
        moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "malloc succeed.\n");
        
        int ret = readn(gCtrlSockId, pBodyCipher, pCtrlResp->basicInfo.bodyLen);
        if(ret < 0)
        {
            moLoggerError(MOCPS_MODULE_LOGGER_NAME, "recvRespBody failed! ret = %d\n", ret);
            free(pBodyCipher);
            pBodyCipher = NULL;
            return -12;
        }
        moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "recvRespBody ok.\n");
        
        pCtrlResp->pBody = crmmGet();
        ret = doDecrypt2Resp(pBodyCipher, pCtrlResp->pBody, pCtrlResp->basicInfo.bodyLen);
        if(ret < 0)
        {
            moLoggerError(MOCPS_MODULE_LOGGER_NAME, "doDecrypt2Resp failed! ret=%d\n", ret);
            free(pBodyCipher);
            pBodyCipher = NULL;
            return -13;
        }
        moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "decrypt to cipher pBody succeed.\n");
        free(pBodyCipher);
        pBodyCipher = NULL;
    }
    return 0;
}

/*
    1.check params;
    2.generate request in our format;
    3.encrypt this request;
    4.send this request to moCpsServer;
    5.if need response, just wait for response from moCpsServer, and parse it to @pCtrlResp;
*/
int moCpsCli_sendRequest(const MOCPS_CMDID cmdId, const MOCPS_REQUEST_TYPE isNeedResp, 
    MOCPS_CTRL_RESPONSE * pCtrlResp)
{
    //1.Check params
    int ret = checkRequestParams(cmdId, isNeedResp, pCtrlResp);
    if(ret != 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "checkRequestParams failed! ret = %d\n", ret);
        return MOCPS_CLI_ERR_INVALID_INPUTPARAM;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "Input param is OK.\n");
    //check is inited or not
    if(!gIsInited)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Donot init moCpsClient yet! cannot send request!\n");
        return MOCPS_CLI_ERR_DONOT_INIT;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "MoCpsClient has been inited.\n");
    //2.generate request in our format
    MOCPS_CTRL_REQUEST req;
    memset(&req, 0x00, sizeof(MOCPS_CTRL_REQUEST));
    ret = genCtrlReq(cmdId, isNeedResp, &req);
    if(ret != 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME,
            "genCtrlReq failed! ret = %d\n", ret);
        return MOCPS_CLI_ERR_INTERNAL;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "genCtrlReq succeed.\n");
    //3.encrypt to this request
    char *pCipher = NULL;
    pCipher = (char *)malloc(sizeof(MOCPS_CTRL_REQUEST) * sizeof(char));
    if(pCipher == NULL)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Malloc failed!" \
            "size = %d, errno = %d, desc = [%s]\n", 
            sizeof(MOCPS_CTRL_RESPONSE), errno, strerror(errno));
        return MOCPS_CLI_ERR_MALLOC;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "Malloc ok, size = %d\n", sizeof(MOCPS_CTRL_REQUEST));
    ret = doEncrypt2Req(req, pCipher);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "doEncrypt2Req failed! ret = %d\n", ret);
        free(pCipher);
        pCipher = NULL;
        return MOCPS_CLI_ERR_ENCRYPT;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "doEncrypt2Req succeed.\n");
    //4.send this request to moCpsServer
    ret = sendReq2Server(pCipher, sizeof(MOCPS_CTRL_REQUEST));
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "sendReq2Server failed! ret = %d\n", ret);
        free(pCipher);
        pCipher = NULL;
        return MOCPS_CLI_ERR_SEND_REQUEST;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "sendReq2server succeed.\n");
    
    free(pCipher);
    pCipher = NULL;
    
    //5.If need response, we get it in timeout
    if(isNeedResp)
    {
        moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "Need response from server, get it now.\n");
        ret = getRespFromServer(req.basicInfo.cmdId, pCtrlResp);
        if(ret < 0)
        {
            moLoggerError(MOCPS_MODULE_LOGGER_NAME, "getRespFromServer failed! ret = %d\n", ret);
            return MOCPS_CLI_ERR_GET_RESPONSE;
        }
        moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "getRespFromServer ok.\n");
    }
    else
    {
        moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "Donot need response from server, return directly now.\n");
    }
    
    return MOCPS_CLI_ERR_OK;
}

/****************************************************************************************************/
/******************************* A module named CtrlRespMemoryManager ****************************/
/****************************************************************************************************/

#define CRMM_SIZE   (2 * 1024 * 1024)   //2M

static char * gCrmmData = NULL;

static int crmmInit()
{
    if(gCrmmData != NULL)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, 
            "gCrmmData donot NULL, CRMM has been inited! cannot init again.\n");
        return -1;
    }
    gCrmmData = (char *)malloc(sizeof(char) * CRMM_SIZE);
    if(NULL == gCrmmData)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Malloc for gCrmmData failed! errno=%d, desc=[%s]\n",
            errno, strerror(errno));
        return -2;
    }
    memset(gCrmmData, 0x00, CRMM_SIZE);
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "crmmInit ok.\n");
    return 0;
}

static void crmmUnInit()
{
    if(gCrmmData != NULL)
    {
        free(gCrmmData);
        gCrmmData = NULL;
    }
}

static char * crmmGet()
{
    return gCrmmData;
}

static int crmmMaxSize()
{
    return CRMM_SIZE;
}


/****************************************************************************************************/
/******************************* A module named DataMemoryManager ****************************/
/****************************************************************************************************/

#define DMM_SIZE         (4 * 1024 * 1024)   //4M

typedef struct
{
    unsigned int seqId;
    unsigned long totalLen;
    unsigned long totalBlkNum;
    unsigned long curSaveBlkNum;    //currently, how many blockes being saved in local memory, this can help us to assure this data being complete or not
}DMM_DATAINFO;

static char * gpDmmData = NULL;
static pthread_mutex_t gDmmMutex = PTHREAD_MUTEX_INITIALIZER;
static DMM_DATAINFO gDmmDatainfo;

static int dmmInit()
{
    pthread_mutex_lock(&gDmmMutex);
    if(gpDmmData != NULL)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "DMM module has been inited yet, cannot init again!\n");
        pthread_mutex_unlock(&gDmmMutex);
        return -1;
    }

    //malloc for memory
    gpDmmData = (char *)malloc(sizeof(char ) * DMM_SIZE);
    if(NULL == gpDmmData)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Malloc for gpDmmData failed! errno=%d, desc=[%s]\n",
            errno, strerror(errno));
        pthread_mutex_unlock(&gDmmMutex);
        return -2;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "malloc for gpDmmData succeed.\n");

    memset(&gDmmDatainfo, 0x00, sizeof(DMM_DATAINFO));
    pthread_mutex_unlock(&gDmmMutex);
        
    return 0;
}

static void dmmUnInit()
{
    if(gpDmmData != NULL)
    {
        free(gpDmmData);
        gpDmmData = NULL;
    }
}

static void dmmClear()
{
    memset(gpDmmData, 0x00, DMM_SIZE);
    memset(&gDmmDatainfo, 0x00, sizeof(DMM_DATAINFO));
}

/*
    If input sequence id equal with current one, save it to memory;
    else, clear memory, and set this as a new data;
*/
static int dmmSaveCurBodyBlk(const MOCPS_DATA_RESPONSE_HEADER header, const char *pCurBlk)
{
    if(header.totalLen > DMM_SIZE || header.totalLen == 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "header.totalLen=%lld, not a valid one, our range is (0, %d]\n",
            header.totalLen, DMM_SIZE);
        return -1;
    }

    pthread_mutex_lock(&gDmmMutex);

    if(gDmmDatainfo.seqId != header.seqId)
    {
        moLoggerInfo(MOCPS_MODULE_LOGGER_NAME, "gDmmDatainfo.seqId=%d, header.seqId=%d, " \
            "not equal, should clear current data to save this new data.\n",
            gDmmDatainfo.seqId, header.seqId);
        dmmClear();
    }
    else
    {
        moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "The same sequence id(value = %d), save it now.\n",
            header.seqId);
    }

    //calc the @pCurBlk should save in where
    unsigned int offset = header.curBlkIdx * MOCPS_DATA_BODY_BLK_MAXSIZE;
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "offset = %d\n", offset);
    //check this block has been write or not, if being write yet, donot write again.
    //duplicat block, logically speaking, donot appeared.
    int i = 0;
    for(; i < 16; i++)
    {
        if(*(gpDmmData + offset + i) == 0x00)
        {
            continue;
        }
        else
        {
            break;
        }
    }
    if(i < 16)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "The block duplicated! " \
            "header.cmdId=%d, header.seqId=%u, header.curBlkIdx=%lu, header.curBlkLen=%u", 
            header.cmdId, header.seqId, header.curBlkIdx, header.curBlkLen);
        pthread_mutex_unlock(&gDmmMutex);
        return 0;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "The block donot write to local yet, write it now.\n");
    //save it to local memory
    memcpy(gpDmmData + offset, pCurBlk, header.curBlkLen);
    //refresh local record
    gDmmDatainfo.seqId = header.seqId;
    gDmmDatainfo.totalLen = header.totalLen;
    gDmmDatainfo.totalBlkNum = gDmmDatainfo.totalLen / MOCPS_DATA_BODY_BLK_MAXSIZE + 
        (gDmmDatainfo.totalLen % MOCPS_DATA_BODY_BLK_MAXSIZE) ? 1 : 0;
    gDmmDatainfo.curSaveBlkNum++;

    pthread_mutex_unlock(&gDmmMutex);
    
    return 0;
}

static char * dmmDataPtr()
{
    return gpDmmData;
}

static char dmmIsCompleteData()
{
    return ((gDmmDatainfo.totalLen!=0)&&(gDmmDatainfo.totalBlkNum == gDmmDatainfo.curSaveBlkNum)) ? 1 : 0;
}






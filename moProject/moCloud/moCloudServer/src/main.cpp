#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <sys/types.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <semaphore.h>


using namespace std;
#include <string>
#include <list>
#include <map>

#include "moCloudUtils.h"
#include "moCloudUtilsTypes.h"
#include "moCloudUtilsCrypt.h"
#include "moCloudUtilsCheck.h"
#include "fileMgr.h"
#include "cliCtrl.h"

#include "moLogger.h"
#include "moUtils.h"
#include "cliMgr.h"


typedef struct
{
    string ip;
    int port;
    //Others
}CONF_INFO;

#define CONF_SYMB_SEC_ADDRINFO  "addrInfo"
#define CONF_SYMB_ATTR_SERVIP   "servIp"
#define CONF_SYMB_ATTR_SERVPORT "servPort"

/*
    config file format : 
        [addrInfo]
        servIp = 127.0.0.1
        servPort = 4111
*/
static int getConfInfo(CONF_INFO & confInfo, const char * pConfFilepath)
{
    if(NULL == pConfFilepath)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "Input param is NULL!\n");
        return -1;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "conf filepath=[%s]\n", pConfFilepath);

    MOUTILS_INI_SECTION_INFO_NODE * pIniHdr = moUtils_Ini_Init(pConfFilepath);
    if(NULL == pIniHdr)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "moUtils_Ini_Init failed! conf filepath=[%s]\n",
            pConfFilepath);
        return -2;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "moUtils_Ini_Init ok.\n");

    char tmp[MOUTILS_INI_ATTR_VALUE_MAX_LEN] = {0x00};
    memset(tmp, 0x00, MOUTILS_INI_ATTR_VALUE_MAX_LEN);
    int ret = moUtils_Ini_GetAttrValue(
        CONF_SYMB_SEC_ADDRINFO, CONF_SYMB_ATTR_SERVIP, tmp, pIniHdr);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "moUtils_Ini_GetAttrValue failed! secName=[%s], attrKey=[%s]\n",
            CONF_SYMB_SEC_ADDRINFO, CONF_SYMB_ATTR_SERVIP);
        moUtils_Ini_UnInit(pIniHdr);
        return -3;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "moUtils_Ini_GetAttrValue ok.\n");
    //check ip valid or not
    if(!isValidIpAddr(tmp))
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "The value [%s] in conf file is not a valid IP address.\n",
            tmp);
        moUtils_Ini_UnInit(pIniHdr);
        return -4;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "ip=[%s], valid value!\n", tmp);
    //set value to @confInfo
    confInfo.ip = tmp;
    
    memset(tmp, 0x00, MOUTILS_INI_ATTR_VALUE_MAX_LEN);
    ret = moUtils_Ini_GetAttrValue(
        CONF_SYMB_SEC_ADDRINFO, CONF_SYMB_ATTR_SERVPORT, tmp, pIniHdr);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "moUtils_Ini_GetAttrValue failed! secName=[%s], attrKey=[%s]\n",
            CONF_SYMB_SEC_ADDRINFO, CONF_SYMB_ATTR_SERVPORT);
        moUtils_Ini_UnInit(pIniHdr);
        return -5;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "moUtils_Ini_GetAttrValue ok.\n");
    int port = atoi(tmp);
    //check ip valid or not
    if(!isValidUserPort(port))
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "The value [%d] in conf file is not a valid port.\n",
            port);
        moUtils_Ini_UnInit(pIniHdr);
        return -6;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "port=%d, valid value!\n", port);
    //set value to @confInfo
    confInfo.port = port;

    moUtils_Ini_UnInit(pIniHdr);
    return 0;
}

static void dumpConfInfo(CONF_INFO & confInfo)
{
    moLoggerInfo(MOCLOUD_MODULE_LOGGER_NAME, 
        "ConfInfo dump here: ip=[%s], port=[%d]\n",
        confInfo.ip.c_str(), confInfo.port);
}

/*
    create socket;
    set attributes;
    listen;
*/
static int initSocket(int & sockId, CONF_INFO & confInfo)
{
    sockId = socket(AF_INET, SOCK_STREAM, 0);
    if(sockId < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "create socket failed! errno=%d, desc=[%s]\n",
            errno, strerror(errno));
        return -1;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "create socket ok, sockId=%d\n", sockId);

    setSockReusable(sockId);

    //bind to port
    struct sockaddr_in servAddr;
    memset(&servAddr, 0x00, sizeof(struct sockaddr_in));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = inet_addr(confInfo.ip.c_str());
    servAddr.sin_port = ntohs(confInfo.port);
    int ret = bind(sockId, (struct sockaddr *)&servAddr, sizeof(struct sockaddr));
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "bind failed! ret=%d, errno=%d, desc=[%s]\n",
            ret, errno, strerror(errno));
        close(sockId);
        sockId = MOCLOUD_INVALID_SOCKID;
        return -2;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "listen succeed.\n");

    ret = listen(sockId, 128);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "listen failed! ret=%d, errno=%d, desc=[%s]\n",
            ret, errno, strerror(errno));
        close(sockId);
        sockId = MOCLOUD_INVALID_SOCKID;
        return -3;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "listen succeed.\n");

    return 0;
}

static int doNewCliConnect(struct sockaddr_in & cliAddr, const int cliSockId)
{
    string cliName("thread_ip=");
    cliName = cliName + inet_ntoa(cliAddr.sin_addr);
    string cliIp(inet_ntoa(cliAddr.sin_addr));

    CliCtrl * pCurCliCtrl = new CliCtrl(cliIp, cliSockId, ntohs(cliAddr.sin_port), cliName);
    pCurCliCtrl->setState(CLI_STATE_IDLE);
    pCurCliCtrl->run();

    CliMgrSingleton::getInstance()->insertCliCtrl(pCurCliCtrl);

    return 0;
}

static int startServer(const char * pConfFilepath)
{
    if(pConfFilepath == NULL)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "Input param is NULL.\n");
        return -1;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
        "confFilepath = [%s]\n", pConfFilepath);

    CONF_INFO confInfo;
    memset(&confInfo, 0x00, sizeof(CONF_INFO));
    int ret = getConfInfo(confInfo, pConfFilepath);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "getConfInfo failed! ret=%d, confFilepath=[%s]\n",
            ret, pConfFilepath);
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "getConfInfo succeed.\n");
    dumpConfInfo(confInfo);

    int sockId = MOCLOUD_INVALID_SOCKID;
    //Create socket, set attributes, listen()
    ret = initSocket(sockId, confInfo);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "initSocket failed! ret=%d\n", ret);
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "initSocket succeed.\n");

    while(1)
    {
        struct sockaddr_in cliAddr;
        memset(&cliAddr, 0x00, sizeof(struct sockaddr_in));
        socklen_t length;
        int cliSockId = accept(sockId, (struct sockaddr *)&cliAddr, &length);
        if(cliSockId < 0)
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
                "accept failed! ret=%d, errno=%d, desc=[%s]\n",
                ret, errno, strerror(errno));
            break;
        }
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
            "get a client. cliSockId=%d, ip=[%s], port=%d\n",
            cliSockId, inet_ntoa(cliAddr.sin_addr), ntohs(cliAddr.sin_port));

        //new a cliCtrl, add to cliMgr
        ret = doNewCliConnect(cliAddr, cliSockId);
        if(ret < 0)
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
                "doNewCliConnect failed! ret=%d, clientIp=[%s], clientPort=%d\n",
                ret, inet_ntoa(cliAddr.sin_addr), ntohs(cliAddr.sin_port));
        }
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
            "doNewCliConnect succeed! clientIp=[%s], clientPort=%d\n",
            inet_ntoa(cliAddr.sin_addr), ntohs(cliAddr.sin_port));
    }

    return 0;
}

int main(int argc, char ** argv)
{
    if(argc != 3)
    {
        fprintf(stderr, "Usage : %s dirPath confFilepath\n\n", argv[0]);
        return -1;
    }

    int ret = moLoggerInit("./");
    if(ret < 0)
    {
        fprintf(stderr, "moLoggerInit failed! ret=%d.\n", ret);
        return -2;
    }

    FileMgrSingleton::getInstance()->modifyDirpath(argv[1]);
    ret = FileMgrSingleton::getInstance()->refreshFileinfoTable();
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "refreshFileinfoTable failed! ret=%d\n", ret);
        moLoggerUnInit();
        return -3;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "refreshFileinfoTable succeed.\n");

    CliMgrSingleton::getInstance()->start();

    //start main thread, to accept all request, then do it
    ret = startServer(argv[2]);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "startServer failed! ret=%d\n", ret);
        
        CliMgrSingleton::getInstance()->stop();
        CliMgrSingleton::getInstance()->join();
        moLoggerUnInit();
        return -4;
    }
    
    moLoggerInfo(MOCLOUD_MODULE_LOGGER_NAME, "Main process will exit now...\n");
    moLoggerUnInit();
    
    return 0;
}

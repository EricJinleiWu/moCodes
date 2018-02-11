#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/types.h>

#include "moCpsClient.h"
#include "moCpsUtils.h"
#include "moLogger.h"

#define ORDER_MAX_LEN   32
#define ORDER_ABBR_MAX_LEN  8

typedef enum
{
    ORDER_TYPE_HELP,
    ORDER_TYPE_GETFILELIST,
    ORDER_TYPE_GETFILE,
    ORDER_TYPE_QUIT,
    ORDER_TYPE_MAX
}ORDER_TYPE;

static char gOrderNames[ORDER_TYPE_MAX][ORDER_MAX_LEN] = 
    {
        "help", 
        "getFileList", 
        "getFile",
        "quit"
    };

static char gOrderAbbrNames[ORDER_TYPE_MAX][ORDER_ABBR_MAX_LEN] =    /*Abbreviation name*/
    {
        "h", 
        "gfl", 
        "gf",
        "q"
    };

static ORDER_TYPE getOrderType(const char *order)
{
    ORDER_TYPE ret = ORDER_TYPE_MAX;
    if(NULL == order)
    {
        fprintf(stderr, "order is NULL!\n");
        return ret;
    }
    int i = 0;
    for(; i < ORDER_TYPE_MAX; i++)
    {
        if(0 == strcmp(order, gOrderNames[i]) || 0 == strcmp(order, gOrderAbbrNames[i]))
        {
            ret = i;
            break;
        }
    }
    return ret;
}

static void showHelp()
{
    printf("help(h) : show help infomation;\n");

    printf("getFileList(gfl) : get all files in moCpsServer;\n");
    printf("getFile(gf) : get the file from moCpsServer to local;\n");
    //TODO, others
    
    printf("quit(q) : quit from this tool;\n");
}

static char gIsRecvAllData = 0;
static char gIsRecvStopSignal = 0;  //The moCpsClient will send a signal when file being sent yet

typedef struct _DATA_NODE
{
    int length;
    char data[MOCPS_DATA_BODY_CHUNK_MAXSIZE];
    struct _DATA_NODE * next;
}DATA_NODE;

//donot head node, the first node
static DATA_NODE * pDataList = NULL;
static pthread_mutex_t gDataListMutex = PTHREAD_MUTEX_INITIALIZER;

/*
    if length ==0, data == NULL, means the file being recved yet!
*/
static int recvData(const char * data, const int length)
{
    pthread_mutex_lock(&gDataListMutex);

    if(NULL == data && 0 == length)
        gIsRecvStopSignal = 1;
    
    if(NULL == pDataList)
    {
        //The first chunk
        pDataList = (DATA_NODE *)malloc(sizeof(DATA_NODE) * 1);
        if(NULL == pDataList)
        {
            printf("malloc for data node failed! errno=%d, desc=[%s]\n",
                errno, strerror(errno));
            pthread_mutex_unlock(&gDataListMutex);
            return -1;
        }
        pDataList->length = length;
        memcpy(pDataList->data, data, length);
        pDataList->next = NULL;
        pthread_mutex_unlock(&gDataListMutex);
    }
    else
    {
        DATA_NODE * pPreNode = pDataList;
        DATA_NODE * pCurNode = pDataList->next;
        while(NULL != pCurNode)
        {
            pPreNode = pCurNode;
            pCurNode = pCurNode->next;
        }
        pCurNode = (DATA_NODE *)malloc(sizeof(DATA_NODE) * 1);
        if(NULL == pDataList)
        {
            printf("malloc for data node failed! errno=%d, desc=[%s]\n",
                errno, strerror(errno));
            pthread_mutex_unlock(&gDataListMutex);
            return -2;
        }
        pCurNode->length = length;
        memcpy(pCurNode->data, data, length);
        pCurNode->next = NULL;
        pPreNode->next = pCurNode;
        pthread_mutex_unlock(&gDataListMutex);
    }
    printf("recv data!\n");
    return 0;
}

static void * writeFileThr(void * args)
{
    args = args;

}

static int isRightFormatResp(MOCPS_CTRL_RESPONSE * pResp)
{
    if(pResp->basicInfo.cmdId >= MOCPS_CMDID_MAX)
    {
        return 0;
    }
    if(0 != strcmp(pResp->basicInfo.mark, MOCPS_MARK_SERVER))
    {
        return 0;
    }
    return 1;
}

static int getFileList()
{
    MOCPS_CTRL_RESPONSE resp;
    memset(&resp, 0x00, sizeof(MOCPS_CTRL_RESPONSE));
    int ret = moCpsCli_getFileList(&resp);
    if(ret < 0)
    {
        fprintf(stderr, "moCpsCli_sendRequest failed! ret=%d\n", ret);
        return -1;
    }
    printf("send getFileList ok, and recv response ok.\n");
    if(!isRightFormatResp(&resp))
    {
        fprintf(stderr, "The response donot int right format!\n");
        return -2;
    }
    printf("check response format ok. bodyLen = %d\n", resp.basicInfo.bodyLen);
    //dump the files info to the format I like ^_^

//    printf("\n");
//    int i = 0;
//    for(i = 0; i < resp.basicInfo.bodyLen; i++)
//    {
//        printf("%d, ", *(resp.pBody + i));
//    }
//    printf("\n\n");
    
    int len = 0;
    while(len < resp.basicInfo.bodyLen)
    {
        MOCPS_BASIC_FILEINFO curFileInfo;
        memset(&curFileInfo, 0x00, sizeof(MOCPS_BASIC_FILEINFO));
        memcpy(&curFileInfo, resp.pBody + len, sizeof(MOCPS_BASIC_FILEINFO));
        printf("File: name=[%s], size=%lld, type=%d\n",
            curFileInfo.filename, curFileInfo.size, curFileInfo.type);

        len += sizeof(MOCPS_BASIC_FILEINFO);
    }
    printf("Dump ok.\n");

    return 0;
}

static int getFile()
{
    MOCPS_CTRL_RESPONSE resp;
    memset(&resp, 0x00, sizeof(MOCPS_CTRL_RESPONSE));
    int ret = moCpsCli_sendRequest(MOCPS_CMDID_GETDATA, MOCPS_REQUEST_TYPE_NEED_RESPONSE, &resp);
    if(ret < 0)
    {
        fprintf(stderr, "moCpsCli_sendRequest failed! ret=%d\n", ret);
        return -1;
    }
    printf("send getFileList ok, and recv response ok.\n");
    if(!isRightFormatResp(&resp))
    {
        fprintf(stderr, "The response donot int right format!\n");
        return -2;
    }
    printf("check response format ok. bodyLen = %d\n", resp.basicInfo.bodyLen);

    while(gIsRecvAllData != 1)
        sleep(1);

    gIsRecvAllData  = 0;
    gIsRecvStopSignal = 0;

    printf("All data being recved!\n");
    return 0;
}

static int doOrder(ORDER_TYPE order)
{
    int ret = 0;
    switch(order)
    {
        case ORDER_TYPE_HELP:
            showHelp();
            break;
        case ORDER_TYPE_GETFILELIST:
            ret = getFileList();
            break;
        case ORDER_TYPE_GETFILE:
            ret = getFile();
            break;
        default:
            printf("order=%d, currently, donot do it.\n", order);
            ret = -1;
            break;
    }
    return ret;
}

int main(int argc, char **argv)
{
	if(argc != 2)
	{
		fprintf(stderr, "Usage : ./%s configFilePath\n", argv[0]);
		return -1;
	}

    signal(SIGPIPE, SIG_IGN);

    int ret = moLoggerInit("./");
    if(ret < 0)
    {
        fprintf(stderr, "moLoggerInit failed! ret = %d\n", ret);
        return -2;
    }

    //do init to this client
    ret = moCpsCli_init(argv[1], recvData);
    if(ret < 0)
    {
        fprintf(stderr, "moCpsCli_init failed! ret=%d, argv[1]=[%s]\n",
            ret, argv[1]);
        moLoggerUnInit();
        return -3;
    }
    printf("moCpsCli_init succeed.\n");

    while(1)
    {
        showHelp();
        char order[ORDER_MAX_LEN] = {0x00};
        memset(order, 0x00, ORDER_MAX_LEN);
        scanf("%s", order);
        ORDER_TYPE type = getOrderType(order);
        if(type >= ORDER_TYPE_MAX)
        {
            fprintf(stderr, "Input order[%s], donot being supported!\n", order);
            continue;
        }
        else if(type == ORDER_TYPE_QUIT)
        {
            printf("Quiting...\n");
            break;
        }
        else
        {
            ret = doOrder(type);
            if(ret < 0)
            {
                fprintf(stderr, "doOrder failed! ORDER_TYPE=%d, ret=%d\n", type, ret);
            }
            else
            {
                printf("doOrder succeed!\n");
            }
        }
    }

    moCpsCli_unInit();
    moLoggerUnInit();
    printf("Quited!\n");
	return 0;	
}





























#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include "utils.h"
#include "server.h"
#include "httpServer.h"
#include "mongoose.h"
#include "fileMgr.h"


#define REQUEST_SYMB_FILENAME "filename="


/*
	********* 注意*********
	遵循HTTP 协议中的状态码, 在服务器内部出现错误的时候使用状态码500.
	并在返回的HTTP payload 中表达具体的服务错误码(HTTP playload 返回对象中的error 对象)

	对于GET/PUT/DELETE/请求, 成功后返回200
	对于POST 请求, 成功创建资源实体后返回201
*/
typedef struct
{
	unsigned long handle;
	HttpHeadPrm headPrm;
	char method[8];
	char url[128];
	char query[128];
	char *reqData;
	int  reqLen;

	void *checkFunc;
}HttpReqThrParam;

static int doReqMainThread(unsigned long handle, void* headPrm, 
    char* method, char* url, char* query, char* reqData, int reqLen, void* apiFunc, void *checkFunc)
{
	if(headPrm == NULL || method == NULL || url == NULL || query == NULL)
	{
        dbgError("Input param invalid!\n");
		return -1;
	}

	HTTPSERVER_API_CALLBACK pApiFunc = (HTTPSERVER_API_CALLBACK)apiFunc;
	HttpReqThrParam *arg = (HttpReqThrParam*)malloc(sizeof(HttpReqThrParam));
	memset(arg, 0x00, sizeof(*arg));
	arg->handle = handle;
	memcpy(&arg->headPrm, (pHttpHeadPrm)headPrm, sizeof(arg->headPrm));
	strncpy(arg->method, method, sizeof(arg->method) - 1);
	strncpy(arg->url, url, sizeof(arg->url) - 1);
	strncpy(arg->query, query, sizeof(arg->query) - 1);
	arg->reqData = reqData;
	arg->reqLen = reqLen;
	arg->checkFunc = checkFunc;

//    dbgDebug("dump request info: url=[%s], method=[%s], query=[%s], reqData=[%s], reqLen=%d\n",
//        arg->url, arg->method, arg->query, arg->reqData, arg->reqLen);

	if(pApiFunc)
		pApiFunc(arg);
    else
        dbgError("pApiFunc is NULL, do nothing for this request.\n");

	return 0;
}

/*
    Write the capture file data from @pReqData to @jpgFilepath;
*/
static int saveJpgFile(char * pReqData, const int reqLen, const int binStartOffset, const int binLen,
    string & jpgFilepath)
{
    if(NULL == pReqData)
    {
        dbgError("Input param is NULL.\n");
        return -1;
    }

    if(binStartOffset >= reqLen)
    {
        dbgError("binaryStartOffset=%d, reqLen=%d, donot valid value!\n", binStartOffset, reqLen);
        return -2;
    }

    FILE * fp = NULL;
    fp = fopen(jpgFilepath.c_str(), "wb");
    if(fp == NULL)
    {
        dbgError("open file [%s] for write failed! errno=%d, desc=[%s]\n",
            jpgFilepath.c_str(), errno, strerror(errno));
        return -3;
    }

    fwrite(pReqData + binStartOffset, 1, binLen, fp);
    fflush(fp);
    
    fclose(fp);
    fp = NULL;
    
    return 0;
}

/*
    @pReqData is the data being sent from http;
    @pFilename is in the form from http post body;
    @pBinStartPos is the binary data position in @pReqData, when we need to get binary data, will use it;
*/
static int getFilename(char * pReqData, const int reqLen, char * pFilename, int * pBinStartOffset)
{
    if(NULL == pReqData || NULL == pFilename || NULL == pBinStartOffset)
    {
        dbgError("Input param is NULL.\n");
        return -1;
    }

    char * pFilenameSymbStart = strstr(pReqData, REQUEST_SYMB_FILENAME);
    if(NULL == pFilenameSymbStart)
    {
        dbgError("donot find REQUEST_SYMB_FILENAME(%s) in reqData(%s)!\n", REQUEST_SYMB_FILENAME, pReqData);
        return -2;
    }
    char * pFilenameStart = pFilenameSymbStart + strlen(REQUEST_SYMB_FILENAME) + 1; /* filename="timestamp_w%d_h%d.jpg" */
    char * pFilenameEnd = strchr(pFilenameStart, '"');
    if(NULL == pFilenameEnd)
    {
        dbgError("find the end symb of filename failed! reqData=[%s]\n", pReqData);
        return -3;
    }
    int filenameLen = pFilenameEnd - pFilenameStart;
    strncpy(pFilename, pFilenameStart, filenameLen);

    char cnt = 1;
    while(*(pFilenameEnd + cnt) == 0x0d || *(pFilenameEnd + cnt) == 0x0a)   //enter or LR
    {
        cnt++;
        continue;
    }
    
    *pBinStartOffset = pFilenameEnd + cnt - pReqData;    
    dbgDebug("filename=[%s], binaryDataStartOffset=%d\n", pFilename, *pBinStartOffset);
    return 0;
}

/*
    @filename in format like : timestamp_w%d_h%d_l%lld.jpg
*/
static int getParamFromFilename(const char * filename, int * pWidth, int * pHeight, long long int * pFileLen,
    string & cameraName)
{
    if(NULL == filename || NULL == pWidth || NULL == pHeight)
    {
        dbgError("Input param is NULL.\n");
        return -1;
    }

    time_t timestamp;
    char tmp[MAX_FILENAME_LEN] = {0x00};
    int cnt = sscanf(filename, REQ_FILENAME_FORMAT, &timestamp, pWidth, pHeight, pFileLen, tmp);
    if(cnt != REQ_FILENAME_PARAM_NUM)
    {
        dbgError("filename=[%s], donot get width and height in it!\n", filename);
        return -2;
    }
    //Now, cameraName include suffix ".jpg" yet, I clear it here.
    if(strlen(tmp) <= strlen(JPG_SUFFIX) || 
        0 != strcmp(tmp + (strlen(tmp) - strlen(JPG_SUFFIX)), JPG_SUFFIX))
    {
        dbgError("tmp=[%s], donot in right jpg file name format, it should ends with [%s] as suffix.\n",
            tmp, JPG_SUFFIX);
        return -3;
    }
    tmp[strlen(tmp) - strlen(JPG_SUFFIX)] = 0x00;
    cameraName = tmp;
    dbgDebug("filename=[%s], width=%d, height=%d, fileLen=%lld, cameraName=[%s]\n", 
        filename, *pWidth, *pHeight, *pFileLen, cameraName.c_str());
    return 0;
}

static void captureHdl(void *arg)
{
	HttpReqThrParam *prm = (HttpReqThrParam *)arg;
	HTTPSERVER_CHECKPARAM_CALLBACK pCheckFunc = (HTTPSERVER_CHECKPARAM_CALLBACK)prm->checkFunc;
    if(NULL == pCheckFunc)
    {
        dbgError("pCheckFunc is NULL! cannot do anything for it!\n");
        httpSerDoResponse(prm->handle, prm->reqData, HTTPSERVER_STATUS_INTERNALSERVERERROR, NULL);
        free(arg);
        return ;
    }
    
    int ret = pCheckFunc(prm->reqData, NULL);
    if(ret < 0)
    {
        dbgError("checkFunc failed! ret=%d\n", ret);
        httpSerDoResponse(prm->handle, prm->reqData, HTTPSERVER_STATUS_INTERNALSERVERERROR, NULL);
        free(arg);
        return ;
    }

    //get filename, save its position, we will used later when write binary data to file
    int binStartPos = 0;
    char filename[MAX_FILENAME_LEN] = {0x00};
    memset(filename, 0x00, MAX_FILENAME_LEN);
    ret = getFilename(prm->reqData, prm->reqLen, filename, &binStartPos);
    if(ret != 0)
    {
        dbgError("get filename from request data failed! ret=%d, reqData=[%s]\n", ret, prm->reqData);
        httpSerDoResponse(prm->handle, prm->reqData, HTTPSERVER_STATUS_INTERNALSERVERERROR, NULL);
        free(arg);
        return ;
    }
    dbgDebug("get filename from request data succeed, filename=[%s]\n", filename);

    //get width and height value
    int width = 0, height = 0;
    string cameraName;
    long long int jpgFileLen = 0;
    ret = getParamFromFilename(filename, &width, &height, &jpgFileLen, cameraName);
    if(ret != 0)
    {
        dbgError("get width and height from filename failed! ret=%d, filename=[%s]\n", ret, filename);
        httpSerDoResponse(prm->handle, prm->reqData, HTTPSERVER_STATUS_INTERNALSERVERERROR, NULL);
        free(arg);
        return ;
    }
    dbgDebug(
        "get width and height from filename succeed, filename=[%s], width=%d, height=%d, fileLen=%lld\n", 
        filename, width, height, jpgFileLen);

    //get timestamp and ip from request header
    time_t timestamp = time(NULL);
    in_addr_t cameraIp = inet_addr(prm->headPrm.cRemoteIp);
    if(ret != 0 || cameraIp < 0)
    {
        dbgError("get all capture params failed! ret=%d, width=%d, height=%d, timestamp=%ld, cameraIp=%ld\n",
            ret, width, height, timestamp, cameraIp);
        httpSerDoResponse(prm->handle, prm->reqData, HTTPSERVER_STATUS_INTERNALSERVERERROR, NULL);
        free(arg);
        return ;
    }
    dbgDebug("get all capture params succeed! width=%d, height=%d, timestamp=%ld, cameraIp=%ld\n",
        width, height, timestamp, cameraIp);

    //construct a fileInfo, then set to fileMgr
    CaptFileInfo fileInfo(width, height, timestamp, cameraIp, cameraName);
    ret = FileMgrSingleton::getInstance()->insertCaptFileTask(fileInfo);
    if(ret != 0)
    {
        dbgError("insertCaptFileTask failed! ret=%d\n", ret);
        httpSerDoResponse(prm->handle, prm->reqData, HTTPSERVER_STATUS_INTERNALSERVERERROR, NULL);
        free(arg);
        return ;
    }
    dbgDebug("insertCaptFileTask succeed.\n");

    //write the binary data from request data to file
    string captDirpath;
    FileMgrSingleton::getInstance()->getCaptDirPath(captDirpath);
    string jpgFilename;
    FileMgrSingleton::getInstance()->getCaptFilename(fileInfo, jpgFilename);
    string jpgFilepath = captDirpath + "/" + jpgFilename;
    ret = saveJpgFile(prm->reqData, prm->reqLen, binStartPos, jpgFileLen, jpgFilepath);
    if(ret != 0)
    {
        dbgError("saveJpgFile failed! ret=%d, jpgFilepath=[%s]\n", ret, jpgFilepath.c_str());
        FileMgrSingleton::getInstance()->delCaptFileTask(fileInfo);
        httpSerDoResponse(prm->handle, prm->reqData, HTTPSERVER_STATUS_INTERNALSERVERERROR, NULL);
        free(arg);
        return ;
    }
    dbgDebug("saveJpgFile succeed. jpgFilepath=[%s]\n", jpgFilepath.c_str());

    FileMgrSingleton::getInstance()->notifyCaptFileTask();

	int httpStatus = HTTPSERVER_STATUS_OK; // 注意此返回值，会因为HTTP 方法不同而不同
    char * httpRes = NULL;
	httpSerDoResponse(prm->handle, prm->reqData, httpStatus, httpRes); // httpRes 如果不为NULL，则会在函数内部进行free 操作

	free(arg);
}

/*
    the format of capture is : height=480&width=640&captureFile=%s&captureFile=%s&captureFile=%s....
*/
static int captureParamCheck(char *request, void *arg)
{
    return 0;

    if(NULL == request)
    {
        dbgError("Input param is NULL.\n");
        return -1;
    }
    arg = arg;

    if(strstr(request, REQUEST_SYMB_FILENAME) == NULL)
    {
        dbgError("request is [%s], donot in right format!\n", request);
        return -2;
    }
    
    return 0;
}


FaceServer::FaceServer() : mPort(8080)
{
    ;
}

FaceServer::FaceServer(const int port) : mPort(port)
{
    ;
}

FaceServer::~FaceServer()
{
    ;
}

int FaceServer::createServer()
{
    HttpServerPrm stHttpPrm;
	memset(&stHttpPrm, 0, sizeof(stHttpPrm));
	strcpy(stHttpPrm.cWebRoot, ".");
	stHttpPrm.iHttpPort = mPort;
	stHttpPrm.pHttpSerReqCbFunc = &doReqMainThread;
	stHttpPrm.pWebSktReqCbFunc = NULL;

	int ret = httpSerInit(&stHttpPrm);
    if(ret != 0)
    {
        dbgError("httpSerInit failed! ret=%d\n", ret);
        return -1;
    }
    dbgDebug("httpSerInit succeed.\n");

    ret = httpSerAddEndpoint("POST", FACESERVER_CAPTURE_URL, &captureHdl, &captureParamCheck);
    if(ret != 0)
    {
        dbgError("httpSerAddEndpoint failed! ret=%d\n", ret);
        httpSerDeinit();
        return -2;
    }
    dbgDebug("httpSerAddEndpoint succeed for URL[%s].\n", FACESERVER_CAPTURE_URL);

    httpSerDaemon();
    
    return 0;
}

void FaceServer::destroyServer()
{
    httpSerDeinit();
}

void FaceServer::run()
{
    struct timeval tm;

    while(getRunState())
    {
        tm.tv_sec = 1;
        tm.tv_usec = 0;
        select(0, NULL, NULL, NULL, &tm);
    }
}


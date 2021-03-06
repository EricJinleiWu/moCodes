#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>

#include <new>

#include "cliCtrl.h"
#include "moCloudUtils.h"
#include "moCloudUtilsCheck.h"
#include "moCloudUtilsCrypt.h"
#include "moCloudUtilsTypes.h"
#include "fileMgr.h"

#define RSA_KEY_LEN 128
//TODO
const static char RSA_PUB_KEY[RSA_KEY_LEN] = {0x00};

CliData::CliData() : 
    mSockId(MOCLOUD_INVALID_SOCKID), mPort(MOCLOUD_INVALID_PORT), mIp(""), 
    mOffset(-1), mFileId(-1), mReadHdl(-1)
{
    ;
}

CliData::CliData(const int sockId, const int port, const string & ip, const MOCLOUD_FILEINFO_KEY & key,
    const size_t offset, const int fileId, int & readHdl) : 
    mSockId(sockId), mPort(port), mIp(ip), mOffset(offset), mFileId(fileId), mReadHdl(readHdl)
{
    memcpy(&mKey, &key, sizeof(MOCLOUD_FILEINFO_KEY));
}

CliData::~CliData()
{
    ;
}

void CliData::run()
{
    MOCLOUD_DATA_HEADER header;
    int unitId = mOffset / MOCLOUD_DATA_UNIT_LEN;
    
    while(getRunState() && mReadHdl >= 0)
    {
        memset(&header, 0x00, sizeof(MOCLOUD_DATA_HEADER));
        
        //seek, read, genResponse, send, looply
        int ret = lseek(mReadHdl, unitId * MOCLOUD_DATA_UNIT_LEN, SEEK_SET);
        if(ret < 0)
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "lseek failed! ret=%d, offset=%d, errno=%d, desc=[%s]\n",
                ret, unitId * MOCLOUD_DATA_UNIT_LEN, errno, strerror(errno));
            //We think this is the end of file, just send the last response here
            genEofRespHeader(header);
            sendHeader(header);
            
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "To the end of file, should exit thread now.\n");
            break;
        }
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "lseek succeed.\n");

        char data[MOCLOUD_DATA_UNIT_LEN] = {0x00};
        int readLen = read(mReadHdl, data, MOCLOUD_DATA_UNIT_LEN);
        if(readLen <= 0)
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
                "readLen=%d, error ocurred, errno=%d, desc=[%s], should exit thread now.\n", 
                readLen, errno, strerror(errno));
            genEofRespHeader(header);
            sendHeader(header);
            break;
        }
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "readLen=%d, length=%d\n", readLen, MOCLOUD_DATA_UNIT_LEN);

        genRespHeader(header, readLen, unitId);
        sendHeader(header);
        sendBody(data, readLen);

        if(readLen != MOCLOUD_DATA_UNIT_LEN)
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
                "read uncompleted! readLen=%d, length=%d\n", readLen, MOCLOUD_DATA_UNIT_LEN);
            genEofRespHeader(header);
            sendHeader(header);
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "To the end of file, should exit thread now.\n");
            break;
        }
    
        unitId++;
    }
}

void CliData::dump()
{
    moLoggerInfo(MOCLOUD_MODULE_LOGGER_NAME, "Dump cliData now.\n");
    moLoggerInfo(MOCLOUD_MODULE_LOGGER_NAME, 
        "sockId=%d, port=%d, ip=[%s], filetype=%d, filename=[%s], offset=%d, fileId=%d, readHdl=%d\n",
        mSockId, mPort, mIp.c_str(), mKey.filetype, mKey.filename, mOffset, mFileId, mReadHdl);
}

int CliData::genEofRespHeader(MOCLOUD_DATA_HEADER & header)
{
    memset(&header, 0x00, sizeof(MOCLOUD_DATA_HEADER));
    strcpy(header.mark, MOCLOUD_MARK_DWLD);
    header.fileId = mFileId;
    header.isEof = 1;
    header.unitId = 0;
    header.bodyLen = 0;
    moCloudUtilsCheck_checksumGetValue((char *)&header,
        sizeof(MOCLOUD_DATA_HEADER) - sizeof(unsigned char),
        &header.checkSum);
    return 0;
}

int CliData::genRespHeader(MOCLOUD_DATA_HEADER & header,const int len,const int unitId)
{
    memset(&header, 0x00, sizeof(MOCLOUD_DATA_HEADER));
    strcpy(header.mark, MOCLOUD_MARK_DWLD);
    header.fileId = mFileId;
    header.isEof = 0;
    header.unitId = unitId;
    header.bodyLen = len;
    moCloudUtilsCheck_checksumGetValue((char *)&header,
        sizeof(MOCLOUD_DATA_HEADER) - sizeof(unsigned char),
        &header.checkSum);
    return 0;
}

int CliData::sendHeader(MOCLOUD_DATA_HEADER & header)
{
    int writeLen = writen(mSockId, (char *)&header, sizeof(MOCLOUD_DATA_HEADER));
    if(writeLen != sizeof(MOCLOUD_DATA_HEADER))
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "writen failed! writeLen=%d, length=%d\n", writeLen, sizeof(MOCLOUD_DATA_HEADER));
        return -1;
    }
    return 0;
}

int CliData::sendBody(char * pData,const int length)
{
    if(NULL == pData)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "Input param is NULL!\n");
        return -1;
    }

    int writeLen = writen(mSockId, pData, length);
    if(writeLen != length)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "writen failed! writeLen=%d, length=%d\n", writeLen, length);
        return -2;
    }
    return 0;
}

int & CliData::getFd()
{
    return mReadHdl;
}

MOCLOUD_FILEINFO_KEY & CliData::getFileKey()
{
    return mKey;
}


CliCtrl::CliCtrl() : 
    MoThread(), 
    mState(CLI_STATE_IDLE), 
    mCtrlSockId(MOCLOUD_INVALID_SOCKID),
    mIp("127.0.0.1"), 
    mCtrlPort(MOCLOUD_INVALID_PORT),
    mIsFilelistChanged(false),
    mDataSockId(MOCLOUD_INVALID_SOCKID),
    mDataPort(MOCLOUD_INVALID_PORT)
{
    mLastHeartbeatTime = time(NULL);
    pthread_mutex_init(&mDwldMutex, NULL);
}
CliCtrl::CliCtrl(const string & thrName) : 
    MoThread(thrName), 
    mState(CLI_STATE_IDLE), 
    mCtrlSockId(MOCLOUD_INVALID_SOCKID),
    mIp("127.0.0.1"), 
    mCtrlPort(MOCLOUD_INVALID_PORT),
    mIsFilelistChanged(false),
    mDataSockId(MOCLOUD_INVALID_SOCKID),
    mDataPort(MOCLOUD_INVALID_PORT)
{
    mLastHeartbeatTime = time(NULL);
    pthread_mutex_init(&mDwldMutex, NULL);
}
CliCtrl::CliCtrl(const string & ip, const int ctrlSockId, const int ctrlSockPort, 
    const string & thrName) :
    MoThread(thrName), 
    mState(CLI_STATE_IDLE), 
    mCtrlSockId(ctrlSockId),
    mIp(ip), 
    mCtrlPort(ctrlSockPort),
    mIsFilelistChanged(false),
    mDataSockId(MOCLOUD_INVALID_SOCKID),
    mDataPort(MOCLOUD_INVALID_PORT)
{
    mLastHeartbeatTime = time(NULL);
    pthread_mutex_init(&mDwldMutex, NULL);
}

bool CliCtrl::operator==(const CliCtrl & other)
{
    return mIp == other.mIp ? true : false;
}

CliCtrl::~CliCtrl()
{
    pthread_mutex_destroy(&mDwldMutex);
}

void CliCtrl::run()
{
    int ret = 0;
    while(getRunState() && getState() != CLI_STATE_INVALID)
    {
        time_t curTime = time(NULL);
        if(abs(curTime - mLastHeartbeatTime) > MOCLOUD_HEARTBEAT_INTEVAL * 2)
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
                "curTime=%ld, lastHeartbeatTime=%ld, heartbeat invalid!\n",
                curTime, mLastHeartbeatTime);
            setState(CLI_STATE_INVALID);
            continue;
        }
        
        //recv request from client, and do it.
        if(mState == CLI_STATE_IDLE)
        {
            ret = doKeyAgree();
            if(ret < 0)
            {
                moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
                    "doKeyAgree failed! ret=%d.\n", ret);
                setState(CLI_STATE_INVALID);
            }
            else
            {
                setState(CLI_STATE_KEYAGREED);
            }
        }
        else
        {
            bool isGetReq = true;
            ret = doCtrlRequest(isGetReq);
            if(ret < 0)
            {
                moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
                    "doCtrlRequest failed! ret=%d.\n", ret);
            }
            else if(isGetReq)
            {
                moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
                    "This time donot get a request in timeout.\n"); 
            }
            else
            {
                moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
                    "Get a request, and do it successfully.\n"); 
            }
        }
    }

    moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
        "CliCtrl thread stopped! getRunState()=%d, getState()=%d, CLI_STATE_INVALID=%d\n",
        getRunState(), getState(), CLI_STATE_INVALID);
}

/*
    1.read a cipher keyagree request;
    2.decrypt;
    3.random a crypt key info, set a response;
    4.encrypt;
    5.write response to client;
*/
int CliCtrl::doKeyAgree()
{
    //1.read cipher request from client in timeout, decrypt it, check it is keyagree or not,
    int ret = getKeyAgreeReqFromCli();
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "getKeyAgreeReqFromCli failed! ret=%d\n", ret);
        return -1;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "getKeyAgreeReqFromCli succeed.\n");
    
    //2.get a crypt info to this client
    MOCLOUD_CRYPT_INFO cryptInfo;
    getCryptInfo4Cli(cryptInfo);
    
    //3.generate response, do encrypt, send to client;
    ret = sendKeyAgreeResp(cryptInfo);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "sendKeyAgreeResp failed! ret=%d\n", ret);
        return -2;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "sendKeyAgreeResp succeed.\n");

    //4.If ok, we should save this cryptinfo in our param
    memcpy(&mCryptInfo, &cryptInfo, sizeof(MOCLOUD_CRYPT_INFO));

    return 0;
}

int CliCtrl::sendKeyAgreeResp(MOCLOUD_CRYPT_INFO & cryptInfo)
{
    //1.generate response
    MOCLOUD_KEYAGREE_RESPONSE resp;
    memset(&resp, 0x00, sizeof(MOCLOUD_KEYAGREE_RESPONSE));
    resp.cmdId = MOCLOUD_CMDID_KEYAGREE;
    memcpy(&resp.info, &cryptInfo, sizeof(MOCLOUD_CRYPT_INFO));
    strcpy(resp.mark, MOCLOUD_MARK_SERVER);
    moCloudUtilsCheck_crcGetValue(MOUTILS_CHECK_CRCMETHOD_32,
        (char *)&resp, 
        sizeof(MOCLOUD_KEYAGREE_RESPONSE) - sizeof(MOUTILS_CHECK_CRCVALUE),
        &resp.crc32);

    //2.get cipher response memory
    int cipherLen = 0;
    moCloudUtilsCrypt_getCipherTxtLen(MOCLOUD_CRYPT_ALGO_RSA,
        sizeof(MOCLOUD_KEYAGREE_RESPONSE), &cipherLen);
    char * pCipherResp = NULL;
    pCipherResp = (char *)malloc(sizeof(char ) * cipherLen);
    if(pCipherResp == NULL)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "Malloc failed! errno=%d, desc=[%s]\n",
            errno, strerror(errno));
        return -1;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "malloc succeed.\n");

    //3.encrypt
    MOCLOUD_CRYPT_INFO rsaCryptInfo;
    memset(&rsaCryptInfo, 0x00, sizeof(MOCLOUD_CRYPT_INFO));
    rsaCryptInfo.cryptAlgoNo = MOCLOUD_CRYPT_ALGO_RSA;
    memcpy(rsaCryptInfo.cryptKey.rsaKey, RSA_PUB_KEY, RSA_KEY_LEN);
    rsaCryptInfo.keyLen = RSA_KEY_LEN;
    int length = 0;
    int ret = moCloudUtilsCrypt_doCrypt(MOCRYPT_METHOD_ENCRYPT,
        rsaCryptInfo, (char *)&resp, sizeof(MOCLOUD_KEYAGREE_RESPONSE),
        pCipherResp, &length);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "moCloudUtilsCrypt_doCrypt failed! ret=%d\n", ret);
        free(pCipherResp);
        pCipherResp = NULL;
        return -2;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "moCloudUtilsCrypt_doCrypt succeed.\n");

    //4.send it to client
    int writeLen = writen(mCtrlSockId, pCipherResp, length);
    if(writeLen != length)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "Write response to client failed! writeLen=%d, length=%d\n",
            writeLen, length);
        free(pCipherResp);
        pCipherResp = NULL;
        return -3;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "send response to client succeed.\n");
    
    free(pCipherResp);
    pCipherResp = NULL;
    return 0;
}

int CliCtrl::getCryptInfo4Cli(MOCLOUD_CRYPT_INFO & cryptInfo)
{
    //TODO, just a stub
    cryptInfo.cryptAlgoNo = MOCLOUD_CRYPT_ALGO_DES;
    char tmpKey[MOCRYPT_DES_KEYLEN] = {1, 2, 3, 4, 5, 6, 7, 8};
    memcpy(cryptInfo.cryptKey.desKey, tmpKey, MOCRYPT_DES_KEYLEN);
    cryptInfo.keyLen = MOCRYPT_DES_KEYLEN;
    return 0;
}

int CliCtrl::getKeyAgreeReqFromCli()
{
    //1.get cipher request length
    int cipherReqLen = 0;
    int ret = moCloudUtilsCrypt_getCipherTxtLen(MOCLOUD_CRYPT_ALGO_RSA,
        sizeof(MOCLOUD_KEYAGREE_REQUEST), &cipherReqLen);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "moCloudUtilsCrypt_getCipherTxtLen failed! ret=%d\n", ret);
        return -1;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "cipherReqLen=%d\n", cipherReqLen);

    int i = 0;
    while(i < 3)
    {
        //2.check client send request in timeout or not
        fd_set rFds;
        FD_ZERO(&rFds);
        FD_SET(mCtrlSockId, &rFds);
        struct timeval tm;
        tm.tv_sec = 1;
        tm.tv_usec = 0;
        ret = select(mCtrlSockId + 1, &rFds, NULL, NULL, &tm);
        if(ret <= 0)
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "select failed! " \
                "ret=%d, errno=%d, desc=[%s]\n", ret, errno, strerror(errno));
            ret = -2;
            i++;
            continue;
        }
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
            "get the request from client, mCtrlSockId=%d.\n",
            mCtrlSockId);
        
        //3.get memory for cipher request
        char * pCipherReq = NULL;
        pCipherReq = (char *)malloc(sizeof(char ) * cipherReqLen);
        if(NULL == pCipherReq)
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
                "malloc failed! length=%d, errno=%d, desc=[%s]\n", 
                cipherReqLen, errno, strerror(errno));
            ret = -3;
            break;
        }
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "malloc succeed.\n");

        //4.read it now.
        int readLen = readn(mCtrlSockId, pCipherReq, cipherReqLen);
        if(readLen != cipherReqLen)
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
                "readn failed! readLen=%d, cipherReqLen=%d\n",
                readLen, cipherReqLen);
            free(pCipherReq);
            pCipherReq = NULL;
            ret = -4;
            i++;
            continue;
        }
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "read cipher request from client succeed.\n");
        
        //5.decrypt it
        MOCLOUD_CRYPT_INFO cryptInfo;
        memset(&cryptInfo, 0x00, sizeof(MOCLOUD_CRYPT_INFO));
        cryptInfo.cryptAlgoNo = MOCLOUD_CRYPT_ALGO_RSA;
        memcpy(cryptInfo.cryptKey.rsaKey, RSA_PUB_KEY, RSA_KEY_LEN);
        cryptInfo.keyLen = RSA_KEY_LEN;
        MOCLOUD_KEYAGREE_REQUEST req;
        int reqLen = 0;
        ret = moCloudUtilsCrypt_doCrypt(MOCRYPT_METHOD_DECRYPT, cryptInfo,
            pCipherReq, cipherReqLen, (char *)&req, &reqLen);
        if(ret < 0)
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
                "moCloudUtilsCrypt_doCrypt failed! ret=%d\n", ret);
            free(pCipherReq);
            pCipherReq = NULL;
            ret = -5;
            i++;
            continue;
        }
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "readCliCipherReq succeed.\n");
        free(pCipherReq);
        pCipherReq = NULL;
        
        //6.check it
        if(strcmp(req.mark, MOCLOUD_MARK_CLIENT) || req.cmdId != MOCLOUD_CMDID_KEYAGREE || 
            moCloudUtilsCheck_crcCheckValue(MOUTILS_CHECK_CRCMETHOD_32, (char *)&req,
            sizeof(MOCLOUD_KEYAGREE_REQUEST) - sizeof(MOUTILS_CHECK_CRCVALUE),
            req.crc32) != 0)
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "The request check failed! "\
                "mark=[%s], cmdId=[%d]\n", req.mark, req.cmdId);
            ret = -6;
            i++;
            continue;
        }
        
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "We get keyagree reqeust from client!\n");
        ret = 0;
        break;
    }
    
    return ret;
}

/*
    read cipher request header, do decrypt, check if have body, read plain body;
    do this request;
    send response;

    @isGetReq, if true, means get a request in timeout;
                  false, means donot have request;
*/
int CliCtrl::doCtrlRequest(bool & isGetReq)
{
    //1.get request header
    MOCLOUD_CTRL_REQUEST req;
    int ret = getCtrlReq(req, isGetReq);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "getCtrlReq failed! ret=%d\n", ret);
        return -1;
    }
    else if(!isGetReq)
    {
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "Donot get request this time.\n");
        return 0;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "getCtrlReq succeed. req.bodyLen=%d\n", req.bodyLen);
    
    //2.if have body, should get it.
    char * pBody = NULL;
    if(req.bodyLen != 0)
    {
        ret = getCtrlReqBody(req.bodyLen, &pBody);        
        if(ret < 0)
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "getCtrlReqBody failed! ret=%d\n", ret);
            return -3;
        }
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "getCtrlReqBody succeed. pBody=[%s]\n", pBody);
    }

    //3.do this request, set ret to response
    MOCLOUD_CTRL_RESPONSE resp;
    memset(&resp, 0x00, sizeof(MOCLOUD_CTRL_RESPONSE));
    char * pRespBody = NULL;
    ret = doRequest(req, pBody, resp, &pRespBody);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "doRequest failed! " \
            "ret=%d, req.cmdId=%d\n", ret, req.cmdId);
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "doRequest succeed.\n");
    free(pBody);
    pBody = NULL;

    if(req.isNeedResp == MOCLOUD_REQUEST_TYPE_NEED_RESPONSE)
    {
        //4.encrypt response, then send it to client
        ret = sendCtrlResp2Cli(resp, pRespBody);
        if(ret < 0)
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "sendCtrlResp2Cli failed! ret=%d\n", ret);
            if(pRespBody != NULL)
            {
                free(pRespBody);
                pRespBody = NULL;
            }
            return -5;
        }
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "sendCtrlResp2Cli succeed.\n");
    }
    
    if(pRespBody != NULL)
    {
        free(pRespBody);
        pRespBody = NULL;
    }
    return 0;
}

/*
    If in timeout, get a request, set value to @req;
    if donot get, set isGetReq to false, we will do it outside;
*/
int CliCtrl::getCtrlReq(MOCLOUD_CTRL_REQUEST & req, bool & isGetReq)
{
    isGetReq = false;

    fd_set rFds;
    FD_ZERO(&rFds);
    FD_SET(mCtrlSockId, &rFds);
    struct timeval tm;
    tm.tv_sec = 1;
    tm.tv_usec = 0;
    int ret = select(mCtrlSockId + 1, &rFds, NULL, NULL, &tm);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "select failed! ret=%d, errno=%d, desc=[%s]\n",
            ret, errno, strerror(errno));
        return -1;
    }
    else if(ret == 0)
    {
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "select timeout.\n");
        isGetReq = false;
        return 0;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "select succeed.\n");

    //get cipher request
    int cipherLen = 0;
    ret = moCloudUtilsCrypt_getCipherTxtLen(mCryptInfo.cryptAlgoNo, sizeof(MOCLOUD_CTRL_REQUEST), &cipherLen);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "moCloudUtilsCrypt_getCipherTxtLen failed, ret=%d.\n", ret);
        return -2;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "cipherLen=%d.\n", cipherLen);

    char * pCipher = NULL;
    pCipher = (char *)malloc(sizeof(char) * cipherLen);
    if(NULL == pCipher)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "malloc failed, ret=%d, errno=%d, desc=[%s].\n", 
            ret, errno, strerror(errno));
        return -3;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "malloc succeed.\n");

    int readLen = readn(mCtrlSockId, pCipher, cipherLen);
    if(readLen != cipherLen)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "readn failed! readLen=%d, cipherLen=%d\n", readLen, cipherLen);

        //If read failed, client being closed, should set this client to invalid
        setState(CLI_STATE_INVALID);
        
        free(pCipher);
        pCipher = NULL;
        return -4;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "read cipher request succeed.\n");

    //do decrypt to cipher request
    MOCLOUD_CTRL_REQUEST tmpReq;
    memset(&tmpReq, 0x00, sizeof(MOCLOUD_CTRL_REQUEST));
    int length = 0;
    ret = moCloudUtilsCrypt_doCrypt(MOCRYPT_METHOD_DECRYPT, mCryptInfo,
        pCipher, cipherLen, (char *)&tmpReq, &length);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "moCloudUtilsCrypt_doCrypt failed, ret=%d.\n", ret);
        free(pCipher);
        pCipher = NULL;
        return -5;
    }
    free(pCipher);
    pCipher = NULL;
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "moCloudUtilsCrypt_doCrypt succeed.\n");

    //check this request.
    if(tmpReq.cmdId >= MOCLOUD_CMDID_MAX || 0 != strcmp(tmpReq.mark, MOCLOUD_MARK_CLIENT) || 
        0 != moCloudUtilsCheck_crcCheckValue(MOUTILS_CHECK_CRCMETHOD_32,
            (char *)&tmpReq,
            sizeof(MOCLOUD_CTRL_REQUEST) - sizeof(MOUTILS_CHECK_CRCVALUE),
            tmpReq.crc32))
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "Request check failed! " \
            "cmdId=%d, mark=[%s]\n", tmpReq.cmdId, tmpReq.mark);
        return -6;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "Request being checked OK.\n");

    memcpy(&req, &tmpReq, sizeof(MOCLOUD_CTRL_REQUEST));
    isGetReq = true;
    
    return 0;
}

int CliCtrl::getCtrlReqBody(const int bodyLen, char ** ppBody)
{
    if(*ppBody != NULL)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "Input param is invalid!\n");
        return -1;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "bodyLen=%d\n", bodyLen);
    
    fd_set rFds;
    FD_ZERO(&rFds);
    FD_SET(mCtrlSockId, &rFds);
    struct timeval tm;
    tm.tv_sec = 1;
    tm.tv_usec = 0;
    int ret = select(mCtrlSockId + 1, &rFds, NULL, NULL, &tm);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "select failed! ret=%d, errno=%d, desc=[%s]\n",
            ret, errno, strerror(errno));
        return -2;
    }
    else if(ret == 0)
    {
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "select timeout.\n");
        return -3;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "select succeed.\n");

    *ppBody = (char * )malloc(sizeof(char) * (bodyLen + 1));
    if(*ppBody == NULL)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "malloc failed! errno=%d, desc=[%s]\n", errno, strerror(errno));
        return -4;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "malloc ok.\n");
    memset(*ppBody, 0x00, bodyLen + 1);

    int readLen = readn(mCtrlSockId, *ppBody, bodyLen);
    if(readLen != bodyLen)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "readn failed! readLen=%d, bodyLen=%d\n", readLen, bodyLen);
        free(*ppBody);
        *ppBody = NULL;
        return -5;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "readn succeed.\n");
    return 0;
}

int CliCtrl::doRequest(MOCLOUD_CTRL_REQUEST & req, const char * pBody, 
    MOCLOUD_CTRL_RESPONSE & resp, char ** ppRespBody)
{
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
        "do request with cmdId=%d\n", req.cmdId);

    int ret = 0;
    switch(req.cmdId)
    {
    case MOCLOUD_CMDID_HEARTBEAT:
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "start doHeartBeat.\n");
        ret = doHeartBeat(req, resp);
        break;
    case MOCLOUD_CMDID_SIGNUP:
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "start doSignUp.\n");
        ret = doSignUp(req, pBody, resp);
        break;
    case MOCLOUD_CMDID_LOGIN:
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "start doLogIn.\n");
        ret = doLogIn(req, pBody, resp);
        break;
    case MOCLOUD_CMDID_LOGOUT:
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "start doLogOut.\n");
        ret = doLogOut(req, resp);
        break;
    case MOCLOUD_CMDID_BYEBYE:
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "start doByebye.\n");
        ret = doByebye(req, resp);
        break;
    case MOCLOUD_CMDID_GETFILELIST:
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "start doGetFilelist.\n");
        ret = doGetFilelist(req, resp, ppRespBody);
        break;
    case MOCLOUD_CMDID_DWLD_START:
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "start doStartDwld.\n");
        ret = doStartDwld(req, pBody, resp);
        break;
    case MOCLOUD_CMDID_DWLD_STOP:
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "start doStopDwld.\n");
        ret = doStopDwld(req, pBody, resp);
        break;
    default:
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "CmdId=%d, currently, donot support it.\n", req.cmdId);
        ret = -2;
        break;
    }

    return ret;
}

int CliCtrl::getUserPasswd(const char * pBody, string & username, string & passwd)
{
    if(NULL == pBody)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "Input param is NULL.\n");
        return -1;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "pBody=[%s]\n", pBody);

    //#define MOCLOUD_USER_PASSWD_FORMAT  "username=%s, password=%s"
    string body(pBody);
    string symbUsername("username=");
    string symbPasswd(", password=");
    //1."username=" must at the beginning of this string
    unsigned int pos1 = body.find(symbUsername);
    if(pos1 == string::npos)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "Donot find symbol [%s] in body [%s]!\n", 
            symbUsername.c_str(), pBody);
        return -2;
    }
    if(pos1 != 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "symbol [%s] donont in beginning of body [%s]!\n", 
            symbUsername.c_str(), pBody);
        return -3;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "Find username symbol at the beginning.\n");

    //2.get ", password=" from body, must donot at the end of this string
    unsigned int pos2 = body.find(symbPasswd);
    if(pos2 == string::npos)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "Donot find symbol [%s] in body [%s]!\n", 
            symbPasswd.c_str(), pBody);
        return -4;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "Find passwd symbol at pos=%u.\n", pos2);

    //get username firstly
    username = string(body, symbUsername.length(), pos2 - symbUsername.length());
    if(username.length() > MOCLOUD_USERNAME_MAXLEN)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "Username=[%s], length=%d, larger than max value %d we allowed.\n",
            username.c_str(), username.length(), MOCLOUD_USERNAME_MAXLEN);
        return -5;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "Username=[%s]\n", username.c_str());
    
    passwd = string(body, pos2 + symbPasswd.length(), body.length());
    if(passwd.length() < MOCLOUD_PASSWD_MINLEN || passwd.length() > MOCLOUD_PASSWD_MAXLEN)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "passwd=[%s], length=[%d], donot in our valid range [%d, %d]\n",
            passwd.c_str(), passwd.length(), MOCLOUD_PASSWD_MINLEN, MOCLOUD_PASSWD_MAXLEN);
        return -6;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "passwd=[%s]\n", passwd.c_str());
    return 0;
}

int CliCtrl::doHeartBeat(MOCLOUD_CTRL_REQUEST & req, MOCLOUD_CTRL_RESPONSE & resp)
{
    //refresh mLastHeartbeat time
    mLastHeartbeatTime = time(NULL);

    if(req.isNeedResp == MOCLOUD_REQUEST_TYPE_NEED_RESPONSE)
    {
        //generate resp
        memset(&resp, 0x00, sizeof(MOCLOUD_CTRL_RESPONSE));
        strcpy(resp.mark, MOCLOUD_MARK_SERVER);
        resp.cmdId = MOCLOUD_CMDID_HEARTBEAT;
        if(mIsFilelistChanged)
        {
            resp.ret = MOCLOUD_HEARTBEAT_RET_FILELIST_CHANGED;
            resp.addInfo.cInfo[0] = mFilelistChangedValue;
        }
        else
        {
            resp.ret = MOCLOUD_HEARTBEAT_RET_OK;
        }
        resp.bodyLen = 0;
        moCloudUtilsCheck_crcGetValue(MOUTILS_CHECK_CRCMETHOD_32,
            (char *)&resp,
            sizeof(MOCLOUD_CTRL_RESPONSE) - sizeof(MOUTILS_CHECK_CRCVALUE),
            &resp.crc32);
    }

    return 0;
}

int CliCtrl::genResp(const int ret, const MOCLOUD_CMDID cmdId, MOCLOUD_CTRL_RESPONSE & resp)
{
    memset(&resp, 0x00, sizeof(MOCLOUD_CTRL_RESPONSE));
    strcpy(resp.mark, MOCLOUD_MARK_SERVER);
    resp.cmdId = cmdId;
    resp.ret = ret;
    moCloudUtilsCheck_crcGetValue(MOUTILS_CHECK_CRCMETHOD_32,
        (char *)&resp,
        sizeof(MOCLOUD_CTRL_RESPONSE) - sizeof(MOUTILS_CHECK_CRCVALUE),
        &resp.crc32);
    return 0;
}

int CliCtrl::doSignUp(MOCLOUD_CTRL_REQUEST & req, const char * pBody, 
    MOCLOUD_CTRL_RESPONSE & resp)    
{
    //check the format 
    string username, passwd;
    int ret = getUserPasswd(pBody, username, passwd);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "getUserPasswd failed! ret=%d, body=[%s]\n", ret, pBody);
        if(req.isNeedResp)
            genResp(MOCLOUD_SIGNUP_RET_ERR_OTHERS, MOCLOUD_CMDID_SIGNUP, resp);
        return -1;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
        "body=[%s], username=[%s], passwd=[%s]\n", 
        pBody, username.c_str(), passwd.c_str());

    DB_USERINFO userInfo;
    memset(&userInfo, 0x00, sizeof(DB_USERINFO));
    userInfo.username = username;
    userInfo.password = passwd;
    userInfo.role = (userInfo.username == "EricWu") ? DB_USER_ROLE_ADMIN : DB_USER_ROLE_ORDINARY;
    ret = FileMgrSingleton::getInstance()->getDbCtrlHdl()->insertUserinfo(userInfo);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "insertUserInfo failed! username=[%s], passwd=[%s], ret=%d\n",
            username.c_str(), passwd.c_str(), ret);
        if(req.isNeedResp)
            genResp(MOCLOUD_SIGNUP_RET_ERR_DUP_USERNAME, MOCLOUD_CMDID_SIGNUP, resp);
        return -2;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
        "insert userinfo succeed, username=[%s], passwd=[%s].\n",
        username.c_str(), passwd.c_str());

    //should refresh sighUpTime to it
    userInfo.signUpTime = time(NULL);
    ret = FileMgrSingleton::getInstance()->getDbCtrlHdl()->modifyUserinfo(username, userInfo);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "modifyUserinfo failed! username=[%s], signUpTime=[%ld], ret=%d\n",
            username.c_str(), userInfo.signUpTime, ret);
        FileMgrSingleton::getInstance()->getDbCtrlHdl()->deleteUserinfo(username);
        if(req.isNeedResp)
            genResp(MOCLOUD_SIGNUP_RET_ERR_OTHERS, MOCLOUD_CMDID_SIGNUP, resp);
        return -3;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "modify signUpTime succeed.\n");

    genResp(ret, MOCLOUD_CMDID_SIGNUP, resp);
    return 0;
}
    
int CliCtrl::doLogIn(MOCLOUD_CTRL_REQUEST & req, const char * pBody, 
    MOCLOUD_CTRL_RESPONSE & resp)    
{
    int ret = MOCLOUD_LOGIN_RET_OK;
    
    if(getState() == CLI_STATE_LOGIN)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "This client has been login, cannot login again.\n");
        if(req.isNeedResp)
            genResp(MOCLOUD_LOGIN_RET_HAS_LOGIN, MOCLOUD_CMDID_LOGIN, resp);
        return -1;
    }
    if(getState() != CLI_STATE_KEYAGREED)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "Current state=%d, cannot do login.\n", getState());
        if(req.isNeedResp)
            genResp(MOCLOUD_LOGIN_RET_OTHERS, MOCLOUD_CMDID_LOGIN, resp);
        return -2;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "Will login it.\n");

    //check the format 
    string username, passwd;
    ret = getUserPasswd(pBody, username, passwd);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "getUserPasswd failed! ret=%d, body=[%s]\n", ret, pBody);
        if(req.isNeedResp)
            genResp(MOCLOUD_LOGIN_RET_OTHERS, MOCLOUD_CMDID_LOGIN, resp);
        return -3;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
        "body=[%s], username=[%s], passwd=[%s]\n", 
        pBody, username.c_str(), passwd.c_str());

    ret = FileMgrSingleton::getInstance()->getDbCtrlHdl()->userLogin(username, passwd);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "userLogin failed! username=[%s], passwd=[%s], ret=%d\n",
            username.c_str(), passwd.c_str(), ret);
        if(req.isNeedResp)
            genResp(ret, MOCLOUD_CMDID_LOGIN, resp);
        return -4;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
        "userLogin succeed, username=[%s], passwd=[%s].\n",
        username.c_str(), passwd.c_str());

    setState(CLI_STATE_LOGIN);
    if(req.isNeedResp)
        genResp(ret, MOCLOUD_CMDID_LOGIN, resp);
    return 0;
}

int CliCtrl::doLogOut(MOCLOUD_CTRL_REQUEST & req, MOCLOUD_CTRL_RESPONSE & resp)    
{
    if(getState() != CLI_STATE_LOGIN)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "This client donot login, cannot logout.\n");
        if(req.isNeedResp)
            genResp(MOCLOUD_LOGOUT_RET_DONOT_LOGIN, MOCLOUD_CMDID_LOGOUT, resp);
        return -1;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "Will logout now.\n");

    setState(CLI_STATE_KEYAGREED);
    if(req.isNeedResp)
        genResp(0, MOCLOUD_CMDID_LOGOUT, resp);
    return 0;
}

int CliCtrl::doByebye(MOCLOUD_CTRL_REQUEST & req, MOCLOUD_CTRL_RESPONSE & resp)    
{
    if(getState() == CLI_STATE_LOGIN)
    {
        setState(CLI_STATE_INVALID);
    }
    if(req.isNeedResp)
        genResp(0, MOCLOUD_CMDID_BYEBYE, resp);
    return 0;
}

int CliCtrl::doGetFilelist(MOCLOUD_CTRL_REQUEST & req, MOCLOUD_CTRL_RESPONSE & resp,
    char ** ppRespBody)    
{
    if(getState() != CLI_STATE_LOGIN)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "Current state=%d, donot logIn yet, cannot get filelist.\n", getState());
        if(req.isNeedResp)
            genResp(MOCLOUD_GETFILELIST_ERR_DONOT_LOGIN, MOCLOUD_CMDID_GETFILELIST, resp);
        return -1;
    }

    int type = req.addInfo.cInfo[0];
    if(type >= MOCLOUD_FILETYPE_MAX)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "Input type=%d, invalid!\n", type);
        if(req.isNeedResp)
            genResp(MOCLOUD_GETFILELIST_ERR_TYPE_INVALID, MOCLOUD_CMDID_GETFILELIST, resp);
        return -2;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "type=%d\n", type);

    if(req.isNeedResp)
    {
        list<DB_FILEINFO> dbFilelist;
        int ret = FileMgrSingleton::getInstance()->getFilelist(req.addInfo.cInfo[0], dbFilelist);
        if(ret < 0)
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "getFilelist from db failed! ret=%d\n", ret);
            if(req.isNeedResp)
                genResp(MOCLOUD_GETFILELIST_ERR_TYPE_INVALID, MOCLOUD_CMDID_GETFILELIST, resp);
            return -3;
        }
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "getFilelist from db succeed.\n");
        
        int length = dbFilelist.size() * sizeof(MOCLOUD_BASIC_FILEINFO);
        *ppRespBody = (char * )malloc(sizeof(char) * length);
        if(NULL == *ppRespBody)
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
                "malloc failed! length=%d, errno=%d, desc=[%s]\n", 
                length, errno, strerror(errno));
            if(req.isNeedResp)
                genResp(MOCLOUD_GETFILELIST_ERR_TYPE_INVALID, MOCLOUD_CMDID_GETFILELIST, resp);
            return -4;
        }
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "malloc succeed.\n");
        
        char * pPos = *ppRespBody;
        for(list<DB_FILEINFO>::iterator it = dbFilelist.begin(); it != dbFilelist.end(); it++)
        {
            memcpy(pPos, &(it->basicInfo), sizeof(MOCLOUD_BASIC_FILEINFO));
            pPos += sizeof(MOCLOUD_BASIC_FILEINFO);
        }
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "pBody being generated.\n");
    
        genResp(MOCLOUD_GETFILELIST_ERR_OK, MOCLOUD_CMDID_GETFILELIST, resp);
        resp.addInfo.cInfo[0] = req.addInfo.cInfo[0];
        resp.bodyLen = length;
    }
    return 0;
}

/*    
    #define MOCLOUD_DWLDTASKINFO_MAXLEN (MOCLOUD_FILENAME_MAXLEN + 128)
    #define MOCLOUD_DWLDTASKINFO_FORMAT "filetype=%d, filename=%s, startOffset=%d, fileId=%d"
*/
int CliCtrl::getDwldReqInfo(const char * pBody, MOCLOUD_FILEINFO_KEY & key, 
    int & startOffset, int & fileId)
{
    if(NULL == pBody)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "Input param is NULL.\n");
        return -1;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "pBody=[%s]\n", pBody);

    string body(pBody);
    string symbFiletype("filetype=");
    string symbFilename(", filename=");
    string symbStartOffset(", startOffset=");
    string symbFileId(", fileId=");
    //1."filetype=" must at the beginning of this string
    unsigned int pos1 = body.find(symbFiletype);
    if(pos1 == string::npos)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "Donot find symbol [%s] in body [%s]!\n", 
            symbFiletype.c_str(), pBody);
        return -2;
    }
    if(pos1 != 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "symbol [%s] donont in beginning of body [%s]!\n", 
            symbFiletype.c_str(), pBody);
        return -3;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "Find filetype symbol at the beginning.\n");

    //2.get ", filename=" from body, must donot at the end of this string
    unsigned int pos2 = body.find(symbFilename);
    if(pos2 == string::npos)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "Donot find symbol [%s] in body [%s]!\n", 
            symbFilename.c_str(), pBody);
        return -4;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "Find filename symbol at pos=%u.\n", pos2);

    //3.get ", startOffset=" from body, must donot at the end of this string
    unsigned int pos3 = body.find(symbStartOffset);
    if(pos3 == string::npos)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "Donot find symbol [%s] in body [%s]!\n", 
            symbStartOffset.c_str(), pBody);
        return -5;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "Find startOffset symbol at pos=%u.\n", pos3);

    //4.get ", fileId=" from body, must donot at the end of this string
    unsigned int pos4 = body.find(symbFileId);
    if(pos4 == string::npos)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "Donot find symbol [%s] in body [%s]!\n", 
            symbFileId.c_str(), pBody);
        return -6;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "Find FileId symbol at pos=%u.\n", pos4);

    //get filetype firstly
    string filetype = string(body, symbFiletype.length(), pos2 - symbFiletype.length());
    key.filetype = MOCLOUD_FILETYPE(atoi(filetype.c_str()));
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "filetype=[%s], its value=%d\n", filetype.c_str(), key.filetype);
    //filename secondly
    string filename = string(body, pos2 + symbFilename.length(), pos3 - (pos2 + symbFilename.length()));
    strncpy(key.filename, filename.c_str(), MOCLOUD_FILENAME_MAXLEN);
    key.filename[MOCLOUD_FILENAME_MAXLEN] = 0x00;
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "filename=[%s], its value=[%s]\n", filename.c_str(), key.filename);
    //startOffset thirdly
    string startOffsetStr = string(body, pos3 + symbStartOffset.length(), pos4 - (pos3 + symbStartOffset.length()));
    startOffset = atoi(startOffsetStr.c_str());
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "startOffsetStr=[%s], its value=%d\n", startOffsetStr.c_str(), startOffset);
    //get fileId endly
    string fileIdStr = string(body, pos4 + symbFileId.length(), body.length());
    fileId = atoi(fileIdStr.c_str());
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "fileIdString=[%s], its value=%d\n", fileIdStr.c_str(), fileId);
    
    return 0;
}

int CliCtrl::doStartDwld(MOCLOUD_CTRL_REQUEST & req, const char * pBody, 
        MOCLOUD_CTRL_RESPONSE & resp)
{
    if(NULL == pBody)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "Input param is NULL!\n");
        genResp(-1, req.cmdId, resp);
        return -1;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "pBody=[%s]\n", pBody);

    pthread_mutex_lock(&mDwldMutex);
    //check the number of dwld tasks now
    if(mCliDataList.size() >= MOCLOUD_DWLD_TASK_MAX_NUM)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "mCliDataList size is %d, larger than max value %d! cannot start new dwld task!\n",
            mCliDataList.size(), MOCLOUD_DWLD_TASK_MAX_NUM);
        pthread_mutex_unlock(&mDwldMutex);
        genResp(-2, req.cmdId, resp);
        return -2;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "mCliDataList.size()=%d\n", mCliDataList.size());

    //get dwld request info
    MOCLOUD_FILEINFO_KEY key;
    memset(&key, 0x00, sizeof(MOCLOUD_FILEINFO_KEY));
    int startOffset = 0;
    int fileId = 0;
    int ret = getDwldReqInfo(pBody, key, startOffset, fileId);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "getDwldReqInfo failed! ret=%d, body=[%s]\n", ret, pBody);
        pthread_mutex_unlock(&mDwldMutex);
        genResp(-3, req.cmdId, resp);
        return -3;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
        "pBody=[%s], filetype=%d, filename=[%s], startOffset=%d, fileId=%d\n",
        pBody, key.filetype, key.filename, startOffset, fileId);

    //check this dwld task being dwlding or not
    for(list<CliData *>::iterator it = mCliDataList.begin(); it != mCliDataList.end(); it++)
    {
        CliData * pCurCliData = *it;
        if( pCurCliData->getFileKey().filetype == key.filetype && 
            0 == strcmp(pCurCliData->getFileKey().filename, key.filename))
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
                "filetype=%d, filename=[%s], being dwlding now, cannot start it again.\n",
                key.filetype, key.filename);
            pthread_mutex_unlock(&mDwldMutex);
            genResp(-4, req.cmdId, resp);
            return -4;
        }
    }

    //open this file now.
    int fd = -1;
    ret = FileMgrSingleton::getInstance()->openFile(key, fd);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "Open file failed! ret=%d, filetype=%d, filename=[%s]\n",
            ret, key.filetype, key.filename);
        pthread_mutex_unlock(&mDwldMutex);
        genResp(-5, req.cmdId, resp);
        return -5;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "openFile succeed. fd=%d\n", fd);

    //new a cliData object
    CliData * pNewCliData = new (nothrow) CliData(mDataSockId, mDataPort, mIp, key,
        startOffset, fileId, fd);
    if(NULL == pNewCliData)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "New cliData object failed! mDataSockId=%d, mDataPort=%d.\n",
            mDataSockId, mDataPort);
        FileMgrSingleton::getInstance()->closeFile(fd);
        pthread_mutex_unlock(&mDwldMutex);
        genResp(-6, req.cmdId, resp);
        return -6;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
        "New cliData object succeed, mDataSockId=%d, mDataPort=%d.\n",
        mDataSockId, mDataPort);
    pNewCliData->start();

    //add this cliData object to list
    mCliDataList.push_back(pNewCliData);

    pthread_mutex_unlock(&mDwldMutex);

    //generate response to it
    genResp(0, req.cmdId, resp);
    
    return 0;
}

int CliCtrl::doStopDwld(MOCLOUD_CTRL_REQUEST & req, const char * pBody, 
        MOCLOUD_CTRL_RESPONSE & resp)
{
    if(NULL == pBody)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "Input param is NULL!\n");
        genResp(-1, req.cmdId, resp);
        return -1;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "pBody=[%s]\n", pBody);

    pthread_mutex_lock(&mDwldMutex);

    //get dwld request info
    MOCLOUD_FILEINFO_KEY key;
    memset(&key, 0x00, sizeof(MOCLOUD_FILEINFO_KEY));
    int startOffset = 0;
    int fileId = 0;
    int ret = getDwldReqInfo(pBody, key, startOffset, fileId);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "getDwldReqInfo failed! ret=%d, body=[%s]\n", ret, pBody);
        pthread_mutex_unlock(&mDwldMutex);
        genResp(-2, req.cmdId, resp);
        return -2;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
        "pBody=[%s], filetype=%d, filename=[%s], startOffset=%d, fileId=%d\n",
        pBody, key.filetype, key.filename, startOffset, fileId);

    //check this dwld task being dwlding or not
    CliData * pCurCliData = NULL;
    bool isFind = false;
    list<CliData *>::iterator it;
    for(it = mCliDataList.begin(); it != mCliDataList.end(); it++)
    {
        pCurCliData = *it;
        if( pCurCliData->getFileKey().filetype == key.filetype && 
            0 == strcmp(pCurCliData->getFileKey().filename, key.filename))
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
                "filetype=%d, filename=[%s], being dwlding now, will stop it.\n",
                key.filetype, key.filename);
            isFind = true;
            break;
        }
    }
    if(!isFind)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "filetype=%d, filename=[%s], donot dwlding, cannot stop it!\n",
            key.filetype, key.filename);
        pthread_mutex_unlock(&mDwldMutex);
        genResp(-3, req.cmdId, resp);
        return -3;
    }

    //close this file now.
    ret = FileMgrSingleton::getInstance()->closeFile(pCurCliData->getFd());
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "Close file failed! ret=%d, filetype=%d, filename=[%s], fileId=%d, fd=%d\n",
            ret, key.filetype, key.filename, fileId, pCurCliData->getFd());
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "closeFile succeed.\n");

    //delete this node
    pCurCliData->stop();
    pCurCliData->join();
    mCliDataList.erase(it);

    pthread_mutex_unlock(&mDwldMutex);

    genResp(0, req.cmdId, resp);
    
    return 0;
}
/*
    encrypt;
    send cipher head;
    if needed, send plain body;
*/
int CliCtrl::sendCtrlResp2Cli(MOCLOUD_CTRL_RESPONSE & resp, char *pRespBody)
{
    int cipherLen = 0;
    int ret = moCloudUtilsCrypt_getCipherTxtLen(mCryptInfo.cryptAlgoNo,
        sizeof(MOCLOUD_CTRL_RESPONSE), &cipherLen);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "moCloudUtilsCrypt_getCipherTxtLen failed! ret=%d\n", ret);
        return -1;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "cipherLen=%d\n", cipherLen);

    char * pCipher = NULL;
    pCipher = (char * )malloc(sizeof(char) * cipherLen);
    if(NULL == pCipher)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "Malloc failed! len=%d, errno=%d, desc=[%s]\n",
            cipherLen, errno, strerror(errno));
        return -2;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "malloc succeed.\n");

    int len = 0;
    ret = moCloudUtilsCrypt_doCrypt(MOCRYPT_METHOD_ENCRYPT,
        mCryptInfo, (char *)&resp, sizeof(MOCLOUD_CTRL_RESPONSE),
        pCipher, &len);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "moCloudUtilsCrypt_doCrypt failed! ret=%d\n", ret);
        free(pCipher);
        pCipher = NULL;
        return -3;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "moCloudUtilsCrypt_doCrypt succeed.\n");

    int writeLen = writen(mCtrlSockId, pCipher, cipherLen);
    if(writeLen != cipherLen)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "Send cipher response header failed! writeLen=%d, cipherLen=%d\n",
            writeLen, cipherLen);        
        free(pCipher);
        pCipher = NULL;
        return -4;
    }
    free(pCipher);
    pCipher = NULL;
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
        "Send cipher response header succeed.\n");

    //to some cmdid, we should send body
    ret = sendRespBody(resp, pRespBody);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "sendRespBody failed! ret=%d\n", ret);
        return -5;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "sendRespBody succeed.\n");

    return 0;
}

int CliCtrl::sendRespBody(const MOCLOUD_CTRL_RESPONSE & resp, char * pRespBody)
{
    int ret = 0;
    if(resp.cmdId == MOCLOUD_CMDID_GETFILELIST)
    {
        ret = sendFilelistBody(pRespBody, resp.bodyLen);
    }

    return ret;
}

int CliCtrl::sendFilelistBody(char * pRespBody, int bodyLen)
{
    int writeLen = writen(mCtrlSockId, pRespBody, bodyLen);
    if(writeLen != bodyLen)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "send filelist body to client failed! writeLen=%d, length=%d\n",
            writeLen, bodyLen);
        return -1;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "send filelist body to client succeed.\n");
    return 0;
}

void CliCtrl::setFilelistChangedFlag()
{
    mIsFilelistChanged = true;
}

void CliCtrl::clearFilelistChangedFlag()
{
    mIsFilelistChanged = false;
}

int CliCtrl::setState(const CLI_STATE state)
{
    if(state >= CLI_STATE_MAX)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "state = %d, invalid value!\n", state);
        return -1;
    }
    
    mState = state;
    return 0;
}

int CliCtrl::setFilelistChangeValue(const int value)
{
    if(value >= MOCLOUD_FILETYPE_MAX)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "value=%d, invalid!\n", value);
        return -1;
    }
    
    mFilelistChangedValue = value;
    return 0;
}

int CliCtrl::setDataPort(const int port)
{
    mDataPort = port;
    return 0;
}

int CliCtrl::setDataSockId(const int sockId)
{
    mDataSockId = sockId;
    return 0;
}

CLI_STATE CliCtrl::getState()
{
    return mState;
}

int CliCtrl::getCtrlSockId()
{
    return mCtrlSockId;
}

string CliCtrl::getIp()
{
    return mIp;
}

int CliCtrl::getCtrlPort()
{
    return mCtrlPort;
}

int CliCtrl::getFilelistChangedFlag()
{
    return mIsFilelistChanged;
}

int CliCtrl::getFilelistChangedValue()
{
    return mFilelistChangedValue;
}

int CliCtrl::getDataSockId()
{
    return mDataSockId;
}

int CliCtrl::getDataPort()
{
    return mDataPort;
}




#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>

#include "cliCtrl.h"
#include "moCloudUtils.h"
#include "moCloudUtilsCheck.h"
#include "moCloudUtilsCrypt.h"
#include "moCloudUtilsTypes.h"

#define RSA_KEY_LEN 128
//TODO
const static char RSA_PUB_KEY[RSA_KEY_LEN] = {0x00};

CliData::CliData(const int sockId, const int port, const string & ip) : 
    MoThread(), mSockId(sockId), mPort(port), mIp(ip)
{
    ;
}

CliData::~CliData()
{
    stop();
    join();
}

void CliData::run()
{
    //TODO
}

int CliData::startRead(const MOCLOUD_FILEINFO_KEY & fileKey, const size_t offset)
{
    //TODO
    return 0;
}

int CliData::stopRead(const MOCLOUD_FILEINFO_KEY & fileKey)
{
    //TODO
    return 0;
}

void CliData::dump()
{
    moLoggerInfo(MOCLOUD_MODULE_LOGGER_NAME, 
        "dump cliData here: ip=[%s], port=%d, sockId=%d\n",
        mIp.c_str(), mPort, mSockId);
}


CliCtrl::CliCtrl() : 
    MoThread(), 
    mState(CLI_STATE_IDLE), 
    mCtrlSockId(MOCLOUD_INVALID_SOCKID), //mDataSockId(MOCLOUD_INVALID_SOCKID), 
    mIp("127.0.0.1"), 
    mCtrlPort(MOCLOUD_INVALID_PORT), //mDataPort(MOCLOUD_INVALID_PORT), 
    mIsFilelistChanged(false)
{
    ;
}
CliCtrl::CliCtrl(const string & thrName) : 
    MoThread(thrName), 
    mState(CLI_STATE_IDLE), 
    mCtrlSockId(MOCLOUD_INVALID_SOCKID), //mDataSockId(MOCLOUD_INVALID_SOCKID), 
    mIp("127.0.0.1"), 
    mCtrlPort(MOCLOUD_INVALID_PORT), //mDataPort(MOCLOUD_INVALID_PORT), 
    mIsFilelistChanged(false)
{
    ;
}
CliCtrl::CliCtrl(const string & ip, const int ctrlSockId, const int ctrlSockPort, 
    const string & thrName) :
    MoThread(thrName), 
    mState(CLI_STATE_IDLE), 
    mCtrlSockId(ctrlSockId), //mDataSockId(MOCLOUD_INVALID_SOCKID), 
    mIp(ip), 
    mCtrlPort(ctrlSockPort), //mDataPort(MOCLOUD_INVALID_PORT), 
    mIsFilelistChanged(false)
{
    ;
}

bool CliCtrl::operator==(const CliCtrl & other)
{
    return mCtrlSockId == other.mCtrlSockId ? true : false;
}

CliCtrl::~CliCtrl()
{
    ;
}

void CliCtrl::run()
{
    int ret = 0;
    while(getRunState() && mState != CLI_STATE_INVALID)
    {
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
            ret = doCtrlRequest();
            if(ret < 0)
            {
                moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
                    "doCtrlRequest failed! ret=%d.\n", ret);
            }
        }
    }
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

    //2.check client send request in timeout or not
    fd_set rFds;
    FD_ZERO(&rFds);
    FD_SET(mCtrlSockId, &rFds);
    struct timeval tm;
    tm.tv_sec = 3;
    tm.tv_usec = 0;
    ret = select(mCtrlSockId + 1, &rFds, NULL, NULL, &tm);
    if(ret <= 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "select failed! " \
            "ret=%d, errno=%d, desc=[%s]\n", ret, errno, strerror(errno));
        return -2;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "get the request from client.\n");

    //3.get memory for cipher request
    char * pCipherReq = NULL;
    pCipherReq = (char *)malloc(sizeof(char ) * cipherReqLen);
    if(NULL == pCipherReq)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "malloc failed! length=%d, errno=%d, desc=[%s]\n", 
            cipherReqLen, errno, strerror(errno));
        return -3;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "malloc succeed.\n");

    //4.read it
    int readLen = readn(mCtrlSockId, pCipherReq, cipherReqLen);
    if(readLen != cipherReqLen)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "readn failed! readLen=%d, cipherReqLen=%d\n",
            readLen, cipherReqLen);
        free(pCipherReq);
        pCipherReq = NULL;
        return -4;
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
        return -5;
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
        return -6;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "We get keyagree reqeust from client!\n");
    return 0;
}

/*
    read cipher request header, do decrypt, check if have body, read plain body;
    do this request;
    send response;
*/
int CliCtrl::doCtrlRequest()
{
    //1.get request header
    MOCLOUD_CTRL_REQUEST req;
    int ret = getCtrlReq(req);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "getCtrlReq failed! ret=%d\n", ret);
        return -1;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "getCtrlReq succeed.\n");

    //2.if have body, should get it.
    char * pBody = NULL;
    if(isHaveBody(req))
    {
        ret = getCtrlReqBody(req.bodyLen, pBody);        
        if(ret < 0)
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "getCtrlReqBody failed! ret=%d\n", ret);
            return -2;
        }
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "getCtrlReqBody succeed.\n");
    }

    //3.do this request, set ret to response
    MOCLOUD_CTRL_RESPONSE resp;
    memset(&resp, 0x00, sizeof(MOCLOUD_CTRL_RESPONSE));
    ret = doRequest(req, pBody, resp);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "doRequest failed! " \
            "ret=%d, req.cmdId=%d\n", ret, req.cmdId);
        free(pBody);
        pBody = NULL;
        return -3;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "doRequest succeed.\n");
    free(pBody);
    pBody = NULL;

    //4.encrypt response, then send it to client
    ret = sendCtrlResp2Cli(resp);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "sendCtrlResp2Cli failed! ret=%d\n", ret);
        return -4;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "sendCtrlResp2Cli succeed.\n");
    
    return 0;
}

int CliCtrl::getCtrlReq(MOCLOUD_CTRL_REQUEST & req)
{
    //TODO, 
    return 0;
}

bool CliCtrl::isHaveBody(MOCLOUD_CTRL_REQUEST & req)
{
    //TODO
    return false;
}

int CliCtrl::getCtrlReqBody(const int bodyLen, char * pBody)
{
    //TODO, 
    return 0;
}

int CliCtrl::doRequest(MOCLOUD_CTRL_REQUEST & req, const char * pBody, 
    MOCLOUD_CTRL_RESPONSE & resp)
{
    //TODO, 
    return 0;
}

int CliCtrl::sendCtrlResp2Cli(MOCLOUD_CTRL_RESPONSE & resp)
{
    //TODO, 
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




#ifndef __CLI_CTRL_H__
#define __CLI_CTRL_H__

#include "moThread.h"
#include "moCloudUtilsTypes.h"

using namespace std;

#include <string>
#include <list>

typedef enum
{
    CLI_STATE_IDLE,
    CLI_STATE_KEYAGREED,
    CLI_STATE_LOGIN,
    CLI_STATE_INVALID,
    CLI_STATE_MAX
}CLI_STATE;

class CliData : public MoThread
{
public:
    CliData();
    CliData(const int sockId, const int port, const string & ip, const MOCLOUD_FILEINFO_KEY & key,
        const size_t offset, const int fileId, int & readHdl);
    ~CliData();

public:
    virtual void run();
    virtual void dump();
    virtual int & getFd();
    virtual MOCLOUD_FILEINFO_KEY & getFileKey();

private:
    virtual int genEofRespHeader(MOCLOUD_DATA_HEADER & header);
    virtual int sendHeader(MOCLOUD_DATA_HEADER & header);
    virtual int sendBody(char * pData, const int length);
    virtual int genRespHeader(MOCLOUD_DATA_HEADER & header, const int len, const int unitId);

private:
    int mSockId;
    int mPort;
    string mIp;

    MOCLOUD_FILEINFO_KEY mKey;
    size_t mOffset;
    int mFileId;

    int mReadHdl;
};

class CliCtrl : public MoThread
{
public:
    CliCtrl();
    CliCtrl(const string & thrName);
    CliCtrl(const string & ip, const int ctrlSockId, const int ctrlSockPort, 
        const string & thrName = "ThrCliCtrl");
    ~CliCtrl();

public:
    bool operator == (const CliCtrl & other);

public:
    virtual void run();

public:
    virtual void setFilelistChangedFlag();
    virtual void clearFilelistChangedFlag();
    virtual int setState(const CLI_STATE state);
    virtual int setFilelistChangeValue(const int value);
    virtual int setDataSockId(const int sockId);
    virtual int setDataPort(const int port);

public:
    virtual CLI_STATE getState();
    virtual int getCtrlSockId();
    virtual string getIp();
    virtual int getCtrlPort();
    virtual int getFilelistChangedFlag();
    virtual int getFilelistChangedValue();
    virtual int getDataSockId();
    virtual int getDataPort();

private:
    virtual int doKeyAgree();
    virtual int getCryptInfo4Cli(MOCLOUD_CRYPT_INFO & cryptInfo);
    virtual int getKeyAgreeReqFromCli();
    virtual int sendKeyAgreeResp(MOCLOUD_CRYPT_INFO & cryptInfo);

    virtual int doCtrlRequest(bool & isGetReq);
    virtual int getCtrlReq(MOCLOUD_CTRL_REQUEST & req, bool & isGetReq);
    virtual int getCtrlReqBody(const int bodyLen, char ** ppBody);
    virtual int doRequest(MOCLOUD_CTRL_REQUEST & req, const char * pBody, 
        MOCLOUD_CTRL_RESPONSE & resp, char ** ppRespBody);
    virtual int sendCtrlResp2Cli(MOCLOUD_CTRL_RESPONSE & resp, char *pRespBody);

private:
    virtual int doHeartBeat(MOCLOUD_CTRL_REQUEST & req, MOCLOUD_CTRL_RESPONSE & resp);
    virtual int doSignUp(MOCLOUD_CTRL_REQUEST & req, const char * pBody, 
        MOCLOUD_CTRL_RESPONSE & resp);
    virtual int doLogIn(MOCLOUD_CTRL_REQUEST & req, const char * pBody, 
        MOCLOUD_CTRL_RESPONSE & resp);
    virtual int doLogOut(MOCLOUD_CTRL_REQUEST & req, MOCLOUD_CTRL_RESPONSE & resp);
    virtual int doByebye(MOCLOUD_CTRL_REQUEST & req, MOCLOUD_CTRL_RESPONSE & resp);
    virtual int doGetFilelist(MOCLOUD_CTRL_REQUEST & req, MOCLOUD_CTRL_RESPONSE & resp,
        char ** ppRespBody);
    virtual int doStartDwld(MOCLOUD_CTRL_REQUEST & req, const char * pBody, 
        MOCLOUD_CTRL_RESPONSE & resp);
    virtual int doStopDwld(MOCLOUD_CTRL_REQUEST & req, const char * pBody, 
        MOCLOUD_CTRL_RESPONSE & resp);

    virtual int getUserPasswd(const char * pBody, string & username, string & passwd);
    virtual int genResp(const int ret, const MOCLOUD_CMDID cmdId, MOCLOUD_CTRL_RESPONSE & resp);
    virtual int sendRespBody(const MOCLOUD_CTRL_RESPONSE & resp, char * pRespBody);
    virtual int sendFilelistBody(char * pRespBody, int bodyLen);
    virtual int getDwldReqInfo(const char * pBody, MOCLOUD_FILEINFO_KEY & key, 
        int & startOffset, int & fileId);

private:
    CLI_STATE mState;
    
    int mCtrlSockId;

    string mIp;

    int mCtrlPort;
    
    bool mIsFilelistChanged;
    int mFilelistChangedValue;

    int mDataSockId;
    int mDataPort;
    list<CliData *> mCliDataList;
    pthread_mutex_t mDwldMutex;

    MOCLOUD_CRYPT_INFO mCryptInfo;

    time_t mLastHeartbeatTime;
};

#endif
#ifndef __CLI_CTRL_H__
#define __CLI_CTRL_H__

#include "moThread.h"
#include "moCloudUtilsTypes.h"

using namespace std;

#include <string>

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
    CliData(const int sockId, const int port, const string & ip);
    ~CliData();

public:
    virtual void run();

public:
    /*
        read file being defined by @fileKey;
        start from @offset, read, util @stopRead() called or to the end of file;
    */
    virtual int startRead(const MOCLOUD_FILEINFO_KEY & fileKey, const size_t offset);

    /*
        When stop/pause, this function called;
    */
    virtual int stopRead(const MOCLOUD_FILEINFO_KEY & fileKey);

    virtual void dump();

private:
    int mSockId;
    int mPort;
    string mIp;
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

public:
    virtual CLI_STATE getState();
    virtual int getCtrlSockId();
    virtual string getIp();
    virtual int getCtrlPort();
    virtual int getFilelistChangedFlag();
    virtual int getFilelistChangedValue();

private:
    virtual int doKeyAgree();
    virtual int getCryptInfo4Cli(MOCLOUD_CRYPT_INFO & cryptInfo);
    virtual int getKeyAgreeReqFromCli();
    virtual int sendKeyAgreeResp(MOCLOUD_CRYPT_INFO & cryptInfo);

    virtual int doCtrlRequest(bool & isGetReq);
    virtual int getCtrlReq(MOCLOUD_CTRL_REQUEST & req, bool & isGetReq);
    virtual int getCtrlReqBody(const int bodyLen, char ** ppBody);
    virtual int doRequest(MOCLOUD_CTRL_REQUEST & req, const char * pBody, 
        MOCLOUD_CTRL_RESPONSE & resp);
    virtual int sendCtrlResp2Cli(MOCLOUD_CTRL_RESPONSE & resp);

private:
    virtual int doHeartBeat(MOCLOUD_CTRL_REQUEST & req, MOCLOUD_CTRL_RESPONSE & resp);
    virtual int doSignUp(MOCLOUD_CTRL_REQUEST & req, const char * pBody, 
        MOCLOUD_CTRL_RESPONSE & resp);
    virtual int doLogIn(MOCLOUD_CTRL_REQUEST & req, const char * pBody, 
        MOCLOUD_CTRL_RESPONSE & resp);
    virtual int doLogOut(MOCLOUD_CTRL_REQUEST & req, MOCLOUD_CTRL_RESPONSE & resp);
    virtual int doByebye(MOCLOUD_CTRL_REQUEST & req, MOCLOUD_CTRL_RESPONSE & resp);
    virtual int doGetFilelist(MOCLOUD_CTRL_REQUEST & req, MOCLOUD_CTRL_RESPONSE & resp);

    virtual int getUserPasswd(const char * pBody, string & username, string & passwd);
    virtual int genResp(const int ret, const MOCLOUD_CMDID cmdId, MOCLOUD_CTRL_RESPONSE & resp);
    virtual int sendRespBody(const MOCLOUD_CTRL_RESPONSE & resp);
    virtual int sendFilelistBody(const int filetype);

private:
    CLI_STATE mState;
    
    int mCtrlSockId;

    string mIp;

    int mCtrlPort;
    
    bool mIsFilelistChanged;
    int mFilelistChangedValue;

    CliData * pCliData;

    MOCLOUD_CRYPT_INFO mCryptInfo;

    time_t mLastHeartbeatTime;
};

#endif
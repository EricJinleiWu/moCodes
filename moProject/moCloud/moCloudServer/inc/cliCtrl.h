#ifndef __CLI_CTRL_H__
#define __CLI_CTRL_H__

#include "moThread.h"

using namespace std;

#include <string>

typedef enum
{
    CLI_STATE_IDLE,
    CLI_STATE_KEYAGREED,
    CLI_STATE_LOGIN,
    CLI_STATE_INVALID,
}CLI_STATE;

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
    virtual int setDataSockInfo(const int & dataSockId, const int dataPort);

public:
    virtual CLI_STATE getState();
    virtual int getCtrlSockId();
    virtual int getDataSockId();
    virtual string getIp();
    virtual int getCtrlPort();
    virtual int getDataPort();
    virtual int getFilelistChangedFlag();

private:
    CLI_STATE mState;
    
    int mCtrlSockId;
    int mDataSockId;

    string mIp;

    int mCtrlPort;
    int mDataPort;
    
    bool mIsFilelistChanged;
};

#endif
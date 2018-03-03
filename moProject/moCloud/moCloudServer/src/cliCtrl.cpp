#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>

#include "cliCtrl.h"
#include "moCloudUtilsTypes.h"

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
        //TODO, recv request from client, and do it.
        if(mState == CLI_STATE_IDLE)
        {
            ret = doKeyAgree();
            if(ret < 0)
            {
                moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
                    "doKeyAgree failed! ret=%d.\n", ret);
                setState(CLI_STATE_INVALID);
            }
        }
        else
        {
            doCtrlRequest();
        }
    }
}

int CliCtrl::doKeyAgree()
{
    return 0;
}

int CliCtrl::doCtrlRequest()
{
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




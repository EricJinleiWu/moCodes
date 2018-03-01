#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>

#include "cliCtrl.h"
#include "moCloudUtilsTypes.h"

CliCtrl::CliCtrl() : 
    MoThread(), 
    mState(CLI_STATE_IDLE), 
    mCtrlSockId(MOCLOUD_INVALID_SOCKID), mDataSockId(MOCLOUD_INVALID_SOCKID), 
    mIp("127.0.0.1"), 
    mCtrlPort(MOCLOUD_INVALID_PORT), mDataPort(MOCLOUD_INVALID_PORT), 
    mIsFilelistChanged(false)
{
    ;
}
CliCtrl::CliCtrl(const string & thrName) : 
    MoThread(thrName), 
    mState(CLI_STATE_IDLE), 
    mCtrlSockId(MOCLOUD_INVALID_SOCKID), mDataSockId(MOCLOUD_INVALID_SOCKID), 
    mIp("127.0.0.1"), 
    mCtrlPort(MOCLOUD_INVALID_PORT), mDataPort(MOCLOUD_INVALID_PORT), 
    mIsFilelistChanged(false)
{
    ;
}
CliCtrl::CliCtrl(const string & ip, const int ctrlSockId, const int ctrlSockPort, 
    const string & thrName) :
    MoThread(thrName), 
    mState(CLI_STATE_IDLE), 
    mCtrlSockId(ctrlSockId), mDataSockId(MOCLOUD_INVALID_SOCKID), 
    mIp(ip), 
    mCtrlPort(ctrlSockPort), mDataPort(MOCLOUD_INVALID_PORT), 
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
    while(1)
    {
        if(!getRunState())
        {
            moLoggerInfo(MOCLOUD_MODULE_LOGGER_NAME, 
                "thId=%lu, thName=%s, will exit now.\n",
                getThrId(), getThrName().c_str());
            break;
        }

        //TODO, recv request from client, and do it.
        
    }
}

void CliCtrl::setFilelistChangedFlag()
{
    mIsFilelistChanged = true;
}

void CliCtrl::clearFilelistChangedFlag()
{
    mIsFilelistChanged = false;
}

int CliCtrl::setDataSockInfo(const int & dataSockId, const int dataPort)
{
    mDataPort = dataPort;
    mDataSockId = dataSockId;
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

int CliCtrl::getDataSockId()
{
    return mDataSockId;
}

string CliCtrl::getIp()
{
    return mIp;
}

int CliCtrl::getCtrlPort()
{
    return mCtrlPort;
}

int CliCtrl::getDataPort()
{
    return mDataPort;
}

int CliCtrl::getFilelistChangedFlag()
{
    return mIsFilelistChanged;
}



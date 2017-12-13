#ifndef __CLI_MGR_H__
#define __CLI_MGR_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <time.h>

#include "moCpsUtils.h"

typedef enum
{
    CLIMGR_CLI_STATE_IDLE,  //A new client in
    CLIMGR_CLI_STATE_CTRLSOCKIN,    //ctrl socket being in
    CLIMGR_CLI_STATE_KEYAGREED, //do keyagree succeed
    CLIMGR_CLI_STATE_HEARTBEAT, //This client being init yet, now should check its heartbeat
    CLIMGR_CLI_STATE_STOPPED,   //connection will be closed
    CLIMGR_CLI_STATE_MAX
}CLIMGR_CLI_STATE;

typedef struct
{
    in_addr_t id;   //use ip address convert to this id, then use this id to be unique to each client
    char addr[MOCPS_IP_ADDR_MAXLEN];    //255.255.255.255 is the largest ip address
    int ctrlPort;
    int ctrlSockId;
    int dataPort;
    int dataSockId;
    CLIMGR_CLI_STATE state;
    clock_t lastHeartbeatTime;  //last heartbeat time, to check heartbeat timeout or not
}CLIMGR_CLIINFO;

typedef struct _CLIMGR_CLIINFO_NODE
{
    CLIMGR_CLIINFO info;
    struct _CLIMGR_CLIINFO_NODE * next;
}CLIMGR_CLIINFO_NODE;

/*
    Do init;
    Generate a list to save all client info;
    create a thread to check all clients' heartbeat;
*/
int cliMgrInit();

/*
    Do uninit;
*/
void cliMgrUnInit();

/*
    A new client in, should malloc a node for it, and save in local memory;
*/
int cliMgrInsertNewCli(const char * pAddr, const int ctrlPort, const int ctrlSockId);

/*
    Recv a heartbeat from a client, should refresh its heartbeat time;
*/
int cliMgrRefreshHeartbeat(const char * pAddr);

/*
    Set state, if state set to Heartbeat, should check its timeout;
*/
int cliMgrSetState(const char *pAddr, CLIMGR_CLI_STATE state);


int cliMgrSetDataPortValue(const char * pAddr, const int dataPort, const int dataSockId);


#ifdef __cplusplus
}
#endif

#endif






































#ifndef __MO_CPS_CLIENT_H__
#define __MO_CPS_CLIENT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "moCpsUtils.h"

#define MOCPS_CLI_ERR_OK            0
#define MOCPS_CLI_ERR_INPUT_NULL    -1
#define MOCPS_CLI_ERR_DUP_INIT      -2
#define MOCPS_CLI_ERR_PARSECONF_FAILED  -3
#define MOCPS_CLI_ERR_CREATESOCK_FAILED -4
#define MOCPS_CLI_ERR_CREATETHR_FAILED  -5
#define MOCPS_CLI_ERR_INVALID_INPUTPARAM    -6
#define MOCPS_CLI_ERR_DONOT_INIT    -7
#define MOCPS_CLI_ERR_INTERNAL  -8
#define MOCPS_CLI_ERR_MALLOC    -9
#define MOCPS_CLI_ERR_ENCRYPT   -10
#define MOCPS_CLI_ERR_SEND_REQUEST  -11
#define MOCPS_CLI_ERR_GET_RESPONSE  -12
#define MOCPS_CLI_ERR_CREATE_SOCKET -13
#define MOCPS_CLI_ERR_SETSOCKATTR   -14
#define MOCPS_CLI_ERR_CONN_FAILED   -15

/*
    A function pointer
*/
typedef int (*pDataCallbackFunc)(const char * data, const int length);

/*
    Do init to moCpsClient;
    @pConfiFilepath is the config file path, we should read config from it,
        like server ip, client ip, and so on, to create socket, do connect;
    @pFunc is a callback function, we will use this function when we should 
        send data to outside;

    Keyagree, start heartbeat thread, start recvData thread, all being done in this function;

    return 0 if succeed, 0- if failed;
*/
int moCpsCli_init(const char* pConfFilepath, const pDataCallbackFunc pFunc);

/*
    Do uninit to moCpsClient;
    Free resource, stop threads, and other operations;
*/
void moCpsCli_unInit();

/*
    Send request to moCpsServer;
    If @isNeedResp is NeedResponse, @pCtrlResp will save the response of this request;
    Or, @pCtrlResp is meaningless, can be NULL;
    return 0 if succeed, 0- if failed;
*/
int moCpsCli_sendRequest(const MOCPS_CMDID cmdId, const MOCPS_REQUEST_TYPE isNeedResp, 
    MOCPS_CTRL_RESPONSE * pCtrlResp);

#ifdef __cplusplus
}
#endif

#endif

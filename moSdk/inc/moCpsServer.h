#ifndef __MOCPS_SERVER_H__
#define __MOCPS_SERVER_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
    Do init to MoCpsServer;
    @pConfFilepath is the config file path; We should read its port and ip from this file;
    return 0 if succeed, else failed;
*/
int moCpsServ_init(const char *pConfFilepath);

/*
    Do uninit to MoCpsServer;
*/
void moCpsServ_unInit();

/*
    Start running;
    start recv request from clients, and deal with them;
    return 0 if succeed.
*/
int moCpsServ_startRunning();

/*
    Stop running;
    after this function, all requests from clients, will not be done;
    return 0 if succeed.
*/
int moCpsServ_stopRunning();

#ifdef __cplusplus
}
#endif

#endif



















#ifndef __MOCLOUD_CLIENT_LIB_H__
#define __MOCLOUD_CLIENT_LIB_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
    Do init to moCloudClient;

    1.read config file, get client ip(and port, not neccessary), get server ip and port;
    2.create socket;
    3.connect to server;
    4.do key agree;
    5.start heartbeat thread;
    6.get memory for ctrl orders, data must have another, but when we exec them, just do it then.

    @pCfgFilepath : the config file path;

    return : 
        0 : succeed;
        0-: failed;
*/
int moCloudClient_init(const char * pCfgFilepath);

/*
    Do uninit to moCloudClient;
    free all resource, stop all threads, and other uninit operations;
*/
void moCloudClient_unInit();

/*
    Sign up to server;

    @pUsrName : the username, must not duplicate with current names;
    @pPasswd : password, length in [6, 16] is OK;

    return :
        0 : succeed;
        0-: failed;
*/
int moCloudClient_signUp(const char * pUsrName, const char * pPasswd);

/*
    Sign up to server;

    @pUsrName : the username, must not duplicate with current names;
    @pPasswd : password, length in [6, 16] is OK;

    return :
        0 : succeed;
        0-: failed;
*/
int moCloudClient_signIn(const char * pUsrName, const char * pPasswd);

/*
    These functions to get files info;

    @pAllFileInfo save the results, its a list, being malloced here;
    So, its important for caller, to call moCloudClient_freeFilesInfo to free memory!!!

    return : 
        0 : succeed;
        0-: failed;
*/
int moCloudClient_getAllFileInfo(MOCLOUD_BASIC_FILEINFO * pAllFileInfo);
int moCloudClient_getOneTypeFileInfo(
    const MOCLOUD_FILETYPE type, MOCLOUD_BASIC_FILEINFO * pOneTypeFileInfo);

void moCloudClient_freeFilesInfo(MOCLOUD_BASIC_FILEINFO * pOneTypeFileInfo);

#if 0
int moCloudClient_startUploadFile();
int moCloudClient_uploadFile();
int moCloudClient_stopUploadFile();

int moCloudClient_startDownloadFile();
int moCloudClient_downloadFiles();
int moCloudClient_stopDownloadFile();

int moCloudClient_startPlayFiles();
int moCloudClient_playFiles();
int moCloudClient_stopPlayFiles();

int moCloudClient_deleteFiles();

#endif

#ifdef __cplusplus
}
#endif

#endif
#ifndef __MOCLOUD_CLIENT_LIB_H__
#define __MOCLOUD_CLIENT_LIB_H__

#ifdef __cplusplus
extern "C" {
#endif

#define MOCLOUDCLIENT_ERR_OK                        (0)
#define MOCLOUDCLIENT_ERR_INPUT_NULL                (MOCLOUDCLIENT_ERR_OK - 1)
#define MOCLOUDCLIENT_ERR_INIT_AGAIN                (MOCLOUDCLIENT_ERR_OK - 2)
#define MOCLOUDCLIENT_ERR_CFGFILE_WRONG_FORMAT      (MOCLOUDCLIENT_ERR_OK - 3)
#define MOCLOUDCLIENT_ERR_CREATE_SOCKET             (MOCLOUDCLIENT_ERR_OK - 4)
#define MOCLOUDCLIENT_ERR_CONNECT_SERVER            (MOCLOUDCLIENT_ERR_OK - 5)
#define MOCLOUDCLIENT_ERR_KEYAGREE                  (MOCLOUDCLIENT_ERR_OK - 6)
#define MOCLOUDCLIENT_ERR_START_HEARTBEAT_THR       (MOCLOUDCLIENT_ERR_OK - 7)

/*
    Do init to moCloudClient;

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
    LogIn to server;

    @pUsrName : the username, must not duplicate with current names;
    @pPasswd : password, length in [6, 16] is OK;

    return :
        0 : succeed;
        0-: failed;
*/
int moCloudClient_logIn(const char * pUsrName, const char * pPasswd);

/*
    LogOut from server;

    LogOut donot need username and password, because we just support one user
        login to server currently;

    return :
        0 : succeed;
        0-: failed;
*/
int moCloudClient_logOut();

/*
    These functions to get files info;

    @pAllFileInfo save the results, its a list, being malloced here;
    So, its important for caller, to call moCloudClient_freeFilesInfo to free memory!!!
    And I defined, @pAllFileInfo is in meaning, if its NULL, means donot have any file in this type;

    return : 
        0 : succeed;
        0-: failed;
*/
int moCloudClient_getAllFileInfo(MOCLOUD_BASIC_FILEINFO * pAllFileInfo);
int moCloudClient_getOneTypeFileInfo(
    const MOCLOUD_FILETYPE type, MOCLOUD_BASIC_FILEINFO * pOneTypeFileInfo);

void moCloudClient_freeFilesInfo(MOCLOUD_BASIC_FILEINFO * pFilesInfo);

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
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
#define MOCLOUDCLIENT_ERR_MALLOC_FAILED             (MOCLOUDCLIENT_ERR_OK - 8)
#define MOCLOUDCLIENT_ERR_INPUT_INVALID             (MOCLOUDCLIENT_ERR_OK - 9)
#define MOCLOUDCLIENT_ERR_INTERNAL_ERROR            (MOCLOUDCLIENT_ERR_OK - 10)
#define MOCLOUDCLIENT_ERR_SEND_REQUEST              (MOCLOUDCLIENT_ERR_OK - 11)
#define MOCLOUDCLIENT_ERR_GETRESP_FAILED            (MOCLOUDCLIENT_ERR_OK - 12)
#define MOCLOUDCLIENT_ERR_CLIDATAINIT_FAILED        (MOCLOUDCLIENT_ERR_OK - 13)
#define MOCLOUDCLIENT_ERR_DWLDTASKS_INIT            (MOCLOUDCLIENT_ERR_OK - 14)

typedef struct
{
    MOCLOUD_FILEINFO_KEY fileKey;
    size_t fileLength;
    char localFilepath[MOCLOUD_FILEPATH_MAXLEN];
    int unitId;
    char isDwlding; //1, dwlding now; 0, else;
}MOCLOUDCLIENT_DWLD_INFO;

typedef struct __MOCLOUDCLIENT_DWLD_INFO_NODE
{
    MOCLOUDCLIENT_DWLD_INFO dwldInfo;
    struct __MOCLOUDCLIENT_DWLD_INFO_NODE * next;
}MOCLOUDCLIENT_DWLD_INFO_NODE;

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
        0-: failed for internal error in client;
        0+: failed from server, like duplicated username, and so on;
*/
int moCloudClient_signUp(const char * pUsrName, const char * pPasswd);

/*
    LogIn to server;

    @pUsrName : the username, must not duplicate with current names;
    @pPasswd : password, length in [6, 16] is OK;

    return :
        0 : succeed;
        0-: failed for internal error in client;
        0+: failed from server, like donot exist username, and so on;
*/
int moCloudClient_logIn(const char * pUsrName, const char * pPasswd);

/*
    LogOut from server;

    LogOut donot need username and password, because we just support one user
        login to server currently;

    return :
        0 : succeed;
        0-: failed for internal error in client;
        0+: failed from server, like user donot login yet, and so on;
*/
int moCloudClient_logOut();

/*
    These functions to get files info list;

    @pFilelist save the results, its a list, being malloced here;
    So, its important for caller, to call moCloudClient_freeFilelist to free memory!!!
    And I defined, @pFilelist is in meaning, if its NULL, means donot have any file in this type;
    @type must be ALL or just one type, cannot be two or more currently;

    return : 
        0 : succeed;
        0-: failed;
*/
int moCloudClient_getFilelist(MOCLOUD_BASIC_FILEINFO_NODE ** ppFilelist, const MOCLOUD_FILETYPE type);
void moCloudClient_freeFilelist(MOCLOUD_BASIC_FILEINFO_NODE * pFilelist);

#if 0
int moCloudClient_startUploadFile();
int moCloudClient_uploadFile();
int moCloudClient_stopUploadFile();
#endif

/*
    Get all uncompleted dwld tasks;
    return a pointer, which point to a forward list;
    if return NULL, means donot find uncompleted tasks;
    must call moCloudClient_freeUncompletedDwldTasks() to free resource!
*/
MOCLOUDCLIENT_DWLD_INFO_NODE * moCloudClient_getAllDwldTasks();
MOCLOUDCLIENT_DWLD_INFO_NODE * moCloudClient_getDwldingTasks();
void moCloudClient_freeDwldTasks(MOCLOUDCLIENT_DWLD_INFO_NODE * pTasks);

int moCloudClient_startDownloadFile();
int moCloudClient_stopDownloadFile();
int moCloudClient_pauseDownloadFile();

#if 0
int moCloudClient_startPlayFiles();
int moCloudClient_playFiles();
int moCloudClient_stopPlayFiles();

int moCloudClient_deleteFiles();

#endif

#ifdef __cplusplus
}
#endif

#endif
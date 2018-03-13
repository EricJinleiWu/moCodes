#ifndef __MOCLOUD_UTILS_TYPE_H__
#define __MOCLOUD_UTILS_TYPE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>

#include "moCrypt.h"
#include "moLogger.h"
#include "moUtils.h"

#define MOCLOUD_MODULE_LOGGER_NAME    "MOCLOUD"

#define MOCLOUD_MARK_MAXLEN   16
#define MOCLOUD_MARK_CLIENT   "MOCLOUD_CLIENT"
#define MOCLOUD_MARK_SERVER   "MOCLOUD_SERVER"

#define MOCLOUD_USER_PASSWD_FORMAT  "username=%s, password=%s"

#define MOCLOUD_HEARTBEAT_INTEVAL     3   //in seconds

#define MOCLOUD_IP_ADDR_MAXLEN    16
#define MOCLOUD_INVALID_SOCKID    (-1)
#define MOCLOUD_INVALID_PORT      (0)

#define MOCLOUD_INVALID_THR_ID    (-1)

//#define MOCLOUD_DATA_BODY_CHUNK_MAXSIZE   (1024 * 1024)   //1M

#define MOCLOUD_DIRPATH_MAXLEN  256
#define MOCLOUD_FILENAME_MAXLEN 256
#define MOCLOUD_FILEPATH_MAXLEN (MOCLOUD_DIRPATH_MAXLEN + MOCLOUD_FILENAME_MAXLEN)

#define MOCLOUD_USERNAME_MAXLEN 32
#define MOCLOUD_PASSWD_MINLEN   6
#define MOCLOUD_PASSWD_MAXLEN   16


/*
    We use a signal to stop our threads;
    this signal I choosed SIGALRM;
    when I want to stop a thread, send a SIGALRM signal to it by pthread_kill();
*/
#define MOCLOUD_STOP_THR_SIG  SIGALRM

/*
    The cmdid we support currently
*/
typedef enum
{
    MOCLOUD_CMDID_KEYAGREE = 0,

    MOCLOUD_CMDID_HEARTBEAT = 10,
    MOCLOUD_CMDID_SIGNUP,
    MOCLOUD_CMDID_LOGIN,
    MOCLOUD_CMDID_LOGOUT,
    MOCLOUD_CMDID_BYEBYE,

    MOCLOUD_CMDID_GETFILELIST = 20,

    //Cmd id for download file from Server
    MOCLOUD_CMDID_SEND_DWLDPORT = 30,
    MOCLOUD_CMDID_DWLD_START,
    MOCLOUD_CMDID_DWLD_PAUSE,
    MOCLOUD_CMDID_DWLD_STOP,

    //Cmd id for upload file to server
    MOCLOUD_CMDID_UPLD_START = 40,
    MOCLOUD_CMDID_UPLD_PAUSE,
    MOCLOUD_CMDID_UPLD_STOP,

    //Cmd id for play video
    MOCLOUD_CMDID_PLAY_VIDEO_START = 50,
    MOCLOUD_CMDID_PLAY_VIDEO_PAUSE,
    MOCLOUD_CMDID_PLAY_VIDEO_STOP,

    //Cmd id for play audio
    MOCLOUD_CMDID_PLAY_AUDIO_START = 60,
    MOCLOUD_CMDID_PLAY_AUDIO_PAUSE,
    MOCLOUD_CMDID_PLAY_AUDIO_STOP,

    //Cmd id for play pictures
    MOCLOUD_CMDID_PLAY_PICTURE = 70,
    
    MOCLOUD_CMDID_MAX
}MOCLOUD_CMDID;

/*
    The crypt algos we support now.
*/
typedef enum
{
    MOCLOUD_CRYPT_ALGO_IDLE,
    MOCLOUD_CRYPT_ALGO_DES,
    MOCLOUD_CRYPT_ALGO_DES3,
    MOCLOUD_CRYPT_ALGO_RC4,
    MOCLOUD_CRYPT_ALGO_RSA, //just use it to do KeyAgree now.
    MOCLOUD_CRYPT_ALGO_MAX
}MOCLOUD_CRYPT_ALGO;

typedef enum
{
    MOCLOUD_REQUEST_TYPE_NOTNEED_RESPONSE,
    MOCLOUD_REQUEST_TYPE_NEED_RESPONSE
}MOCLOUD_REQUEST_TYPE;

typedef enum
{
    MOCLOUD_FILETYPE_VIDEO = 1,  //video, can play in player, has suffix with ".mp4" and so on
    MOCLOUD_FILETYPE_AUDIO = 2,  //audio, can play in player, has suffix with ".mp3" and so on
    MOCLOUD_FILETYPE_PIC = 4,    //picture, has suffix with ".jpg" and so on
    MOCLOUD_FILETYPE_OTHERS = 8,  //other files, can be read in common file mode
    MOCLOUD_FILETYPE_ALL = 15,   //all types
    MOCLOUD_FILETYPE_MAX    //max value, to limit the range
}MOCLOUD_FILETYPE;

typedef enum
{
    MOCLOUD_FILE_STATE_NORMAL,  //Normal state, can read, download, play;
    MOCLOUD_FILE_STATE_UPLOADING,   //uploading by someone, cannot do any operations;
    MOCLOUD_FILE_STATE_DELETING,    //deleting by admin, file cannot do any operations;
    MOCLOUD_FILE_STATE_DOWNLOADING, //someones downloading it;
    MOCLOUD_FILE_STATE_MAX
}MOCLOUD_FILE_STATE;

typedef enum
{
    MOCLOUD_HEARTBEAT_RET_FAILED,
    MOCLOUD_HEARTBEAT_RET_OK,
    MOCLOUD_HEARTBEAT_RET_FILELIST_CHANGED
}MOCLOUD_HEARTBEAT_RET;

typedef enum
{
    MOCLOUD_SIGNUP_RET_OK,
    MOCLOUD_SIGNUP_RET_ERR_DUP_USERNAME,
    MOCLOUD_SIGNUP_RET_ERR_OTHERS
}MOCLOUD_SIGNUP_RET;

typedef enum
{
    MOCLOUD_LOGIN_RET_OK,
    MOCLOUD_LOGIN_RET_HAS_LOGIN,
    MOCLOUD_LOGIN_RET_USERNAME_DONOT_EXIST,
    MOCLOUD_LOGIN_RET_PASSWD_WRONG,
    MOCLOUD_LOGIN_RET_OTHERS
}MOCLOUD_LOGIN_RET;

typedef enum
{
    MOCLOUD_LOGOUT_RET_OK,
    MOCLOUD_LOGOUT_RET_DONOT_LOGIN,
    MOCLOUD_LOGOUT_RET_USERNAME_DONOT_EXIST,
    MOCLOUD_LOGOUT_RET_OTHERS
}MOCLOUD_LOGOUT_RET;

typedef enum
{
    MOCLOUD_GETFILELIST_ERR_OK,
    MOCLOUD_GETFILELIST_ERR_DONOT_LOGIN,
    MOCLOUD_GETFILELIST_ERR_TYPE_INVALID,
    MOCLOUD_GETFILELIST_ERR_OTHERS,
}MOCLOUD_GETFILELIST_ERR;

/*
    The struct for keyagree reqeust from client to server;
*/
typedef struct
{
    char mark[MOCLOUD_MARK_MAXLEN]; //"MOCLOUD_CLIENT"
    MOCLOUD_CMDID cmdId;    //MOCLOUD_CMDID_KEYAGREE
    MOUTILS_CHECK_CRCVALUE crc32;
}MOCLOUD_KEYAGREE_REQUEST;


/*
    The struct for keyagree response from server to client;
*/
typedef union
{
    char desKey[MOCRYPT_DES_KEYLEN];
    char des3Key[MOCRYPT_DES_KEYLEN * 3];   //can just use 16bytes
    char rc4Key[MOCRYPT_RC4_KEY_MAX_LEN];
    char rsaKey[128];   //128bytes=1024bits, 1024RSA
}MOCLOUD_CRYPT_KEY_INFO; 

typedef struct
{
	MOCLOUD_CRYPT_ALGO cryptAlgoNo;
	MOCLOUD_CRYPT_KEY_INFO cryptKey;
	int keyLen; 
}MOCLOUD_CRYPT_INFO;

typedef struct
{
    char mark[MOCLOUD_MARK_MAXLEN]; //"MOCLOUD_SERVER"
    MOCLOUD_CMDID cmdId;    //MOCLOUD_CMDID_KEYAGREE
    int ret;    //KeyAgree succeed or failed
    MOCLOUD_CRYPT_INFO info;
    MOUTILS_CHECK_CRCVALUE crc32;
}MOCLOUD_KEYAGREE_RESPONSE;

/*
    The additional info of ctrl request;
*/
typedef union
{
    int iInfo[4];
    char cInfo[16];
}MOCLOUD_ADDITIONAL_INFO;

/*
    The struct for ctrl orders;
    like getFileList, sendHeartBeat, sendbyebye, and so on;
*/
typedef struct
{
    char mark[MOCLOUD_MARK_MAXLEN]; //"MOCLOUD_CLIENT"
    MOCLOUD_REQUEST_TYPE isNeedResp;
    MOCLOUD_CMDID cmdId;
    MOCLOUD_ADDITIONAL_INFO addInfo;
    int bodyLen;    //If donot have body, this set to 0; else, after this request, send a body in this length;
    MOUTILS_CHECK_CRCVALUE crc32;
}MOCLOUD_CTRL_REQUEST;

/*
    The response from server, just ctrl orders;
*/
typedef struct
{
	char mark[MOCLOUD_MARK_MAXLEN];		//"MOCLOUD_SERVER"
	MOCLOUD_CMDID cmdId;
    int ret;    //The ret for this order
    MOCLOUD_ADDITIONAL_INFO addInfo;
	int bodyLen;
	MOUTILS_CHECK_CRCVALUE crc32;
}MOCLOUD_CTRL_RESPONSE;

typedef struct
{
    MOCLOUD_FILETYPE filetype;
    char filename[MOCLOUD_FILENAME_MAXLEN];
}MOCLOUD_FILEINFO_KEY;

/*
    The basic info for a file;
*/
typedef struct
{
    MOCLOUD_FILEINFO_KEY key;
    
    size_t filesize; //in bytes
    char ownerName[MOCLOUD_USERNAME_MAXLEN];    //who upload this file
    MOCLOUD_FILE_STATE state;
}MOCLOUD_BASIC_FILEINFO;

typedef struct _MOCLOUD_BASIC_FILEINFO_NODE
{
    MOCLOUD_BASIC_FILEINFO info;
    struct _MOCLOUD_BASIC_FILEINFO_NODE * next;
}MOCLOUD_BASIC_FILEINFO_NODE;


#if 0
//This will be used when file transmiting, donot being used currently verson;
/*
    To body, we will transport it in following rule:
        split to several chunks, then head+chunk being sent from server to client each time;
*/
typedef struct
{
	char mark[MOCLOUD_MARK_MAXLEN];	//MOCLOUD_SERVER
	MOCLOUD_CMDID cmdId;
    //The total len of this file
    unsigned long long totalLen;
    //start with 0, chunk id
    unsigned int chunkId;	
    //except the last chunk, all other chunks, has size MOCLOUD_DATA_BODY_CHUNK_MAXSIZE
    //To the last chunk, this size being valid, and less than MOCLOUD_DATA_BODY_CHUNK_MAXSIZE
    unsigned int curChunkSize;
    //use check sum algo. to check its right or not;
	unsigned char checksum;
}MOCLOUD_DATA_RESPONSE_HEADER;
#endif


#ifdef __cplusplus
}
#endif

#endif

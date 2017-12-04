#ifndef __MO_CPS_UTILS_H__
#define __MO_CPS_UTILS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "moCrypt.h"
#include "moUtils.h"

#define MOCPS_MARK_CLIENT   "MOCPS_CLIENT"
#define MOCPS_MARK_SERVER   "MOCPS_SERVER"
#define MOCPS_MARK_MAXLEN   16
#define MOCPS_HEARTBEAT_INTEVAL     3   //in seconds
#define MOCPS_MODULE_LOGGER_NAME    "MOCPS"
#define MOCPS_IP_ADDR_MAXLEN    16
#define MOCPS_INVALID_SOCKID    (-1)
#define MOCPS_DATA_BODY_BLK_MAXSIZE 1024

typedef enum
{
    MOCPS_CMDID_KEYAGREE,
    MOCPS_CMDID_HEARTBEAT,
    MOCPS_CMDID_BYEBYE,
    MOCPS_CMDID_SEND_DATAPORT,
    MOCPS_CMDID_GETFILEINFO,    //Get the files info in moCpsServer;
    MOCPS_CMDID_GETDATA,
    MOCPS_CMDID_MAX
}MOCPS_CMDID;

typedef enum
{
    MOCPS_CRYPT_ALGO_DES,
    MOCPS_CRYPT_ALGO_DES3,
    MOCPS_CRYPT_ALGO_AES,
    MOCPS_CRYPT_ALGO_RC4,
    MOCPS_CRYPT_ALGO_MAX
}MOCPS_CRYPT_ALGO;

typedef enum
{
    MOCPS_REQUEST_TYPE_NEED_RESPONSE,
    MOCPS_REQUEST_TYPE_NOTNEED_RESPONSE
}MOCPS_REQUEST_TYPE;

typedef struct
{
	char mark[16];		//MOCPS_CLIENT
	MOCPS_REQUEST_TYPE isNeedResp;	//1, need response
	MOCPS_CMDID cmdId;
    char res[4];    //reserved
}MOCPS_CTRL_REQUEST_BASIC;

typedef struct
{
    MOCPS_CTRL_REQUEST_BASIC basicInfo;
	MOUTILS_CHECK_CRCVALUE crc32;	//crc32 value, to check data
}MOCPS_CTRL_REQUEST;


typedef union
{
    char desKey[MOCRYPT_DES_KEYLEN];
    char des3Key[MOCRYPT_DES_KEYLEN * 3];   //can just use 16bytes
//    char aesKey[16];    //TODO, this algo. should be done after
    char rc4Key[MOCRYPT_RC4_KEY_MAX_LEN];
}MOCPS_CRYPT_KEY_INFO; //TODO, must assure the key max length of each algo.

typedef struct
{
	MOCPS_CRYPT_ALGO cryptAlgoNo;	//AlgoDes, AlgoDes3, AlgoRc4, and so on
	MOCPS_CRYPT_KEY_INFO cryptKey;  //The key
	unsigned char keyLen;		//The length of key 
}MOCPS_CRYPT_INFO;

typedef struct
{
	char mark[MOCPS_MARK_MAXLEN];		//MOCPS_SERVER
	MOCPS_CMDID cmdId;		//CMD_KEYAGREE
	MOCPS_CRYPT_INFO cryptInfo;
    MOUTILS_CHECK_CRCVALUE crc32;
}MOCPS_KEYAGREE_RESPONSE;

typedef struct
{
	char mark[MOCPS_MARK_MAXLEN];		//MOCPS_SERVER
	MOCPS_CMDID cmdId;
	char res[3];		//reserved
	int bodyLen;		//The valid data length in body
	MOUTILS_CHECK_CRCVALUE crc32;		//crc32 to check data
}MOCPS_CTRL_RESPONSE_BASIC;

typedef struct
{
    MOCPS_CTRL_RESPONSE_BASIC basicInfo;
    char * pBody;   //When bodyLen!=0, we should malloc a memory for this pBody to save the body
}MOCPS_CTRL_RESPONSE;

typedef struct
{
	char mark[MOCPS_MARK_MAXLEN];	//MOCPS_SERVER
	MOCPS_CMDID cmdId;
    //start with 0, sequence id
    unsigned int seqId;
	unsigned long long totalLen;	//the total length of file, in bytes
	//We split a file to several blocks to transmite, each block except the last, has length of 1K(1024bytes), this index can merge these blocks to a file in right format;
	//start with 0
	unsigned long int curBlkIdx;	
	unsigned int curBlkLen;		//just in last block, this is valid, other blocks has length 1024bytes
	unsigned char checksum;		//use check sum algo. to check its right or not;
}MOCPS_DATA_RESPONSE_HEADER;

/* 
    Check input @pIp is a valid ip address or not;
    A valid ip address in format like aaa.bbb.ccc.ddd;
    return 0 if invalid, 1 valid;
*/
int isValidIpAddr(const char * pIp);

/*
    Set the socket(with id @sockId) to nonBlock mode;
    return 0 if succeed, 0- if failed.
*/
int setSock2NonBlock(const int sockId);

/*
    Read function with socket;
    The same as recv();
    return the length being read, if donot equal with @len, error ocurred;
*/
int readn(const int sockId, char* buf, const int len);

/*
    Write function with socket;
    The same as send();
    return the length being write, if donot equal with @len, error ocurred;
*/
int writen(const int sockId, const char* buf, const int len);

/*
    Split a int value to char[4];
*/
int splitInt2Char(const int src, char dst[4]);

/*
    Merge char[4] to a int value; 
    it is reverse process to splitInt2Char;
*/
int mergeChar2Int(const char src[4], int *dst);

#ifdef __cplusplus
}
#endif

#endif

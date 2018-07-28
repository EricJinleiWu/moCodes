#ifndef __UTILS_H__
#define __UTILS_H__

#define dbgDebug(format, ...) printf("[%s, %d--debug] : "format, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define dbgWarn(format, ...) printf("[%s, %d--warn] : "format, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define dbgInfo(format, ...) printf("[%s, %d--info] : "format, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define dbgError(format, ...) printf("[%s, %d--error] : "format, __FUNCTION__, __LINE__, ##__VA_ARGS__)

#define FACE_SERVER_HTTP_PORT   8088

#define INVALID_THR_ID  (-1)
#define DEFAULT_THR_NAME    "defThrName"

#define DEFAULT_CAPT_DIRPATH    "./captFiles"
#define DEFAULT_BASE_DIRPATH    "./baseFiles"
#define DEFAULT_DISPLAY_MAX_NUM 9

#define MAX_FILENAME_LEN    256
#define CAPT_FILE_FORMAT    "%ld_w%d_h%d_ip%lu_capture.jpg"
#define CAPT_FILE_PARAM_NUM 4
#define YUV_FILE_FORMAT    "%ld_w%d_h%d_ip%lu_capture.yuv"

#define REQ_FILENAME_FORMAT "%ld_w%d_h%d_l%lld.jpg"
#define REQ_FILENAME_PARAM_NUM  4

#define BASE_FILE_FORMAT    "baseFace_w%d_h%d_n%s.jpg"
#define BASE_FILE_PARAM_NUM 3
#define BASE_TMPYUV_FILENAME_FORMAT "baseFace_w%d_h%d_n%s_tmp.yuv"

#define UNKNOWN_PEOPLE_NAME "UnKnown"
#define SIMI_LIMIT_VALUE    0.6

/* 
    Check input @pIp is a valid ip address or not;
    A valid ip address in format like aaa.bbb.ccc.ddd;
    return 0 if invalid, 1 valid;
*/
int isValidIpAddr(const char * pIp);

/*
    The @port should in [1024, 65535] if the process is popular process;
*/
int isValidUserPort(const int port);

/*
    Set the socket(with id @sockId) to nonBlock mode;
    return 0 if succeed, 0- if failed.
*/
int setSock2NonBlock(const int sockId);

/*
    Set a socket to reusable;
*/
int setSockReusable(const int sockId);

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


#endif

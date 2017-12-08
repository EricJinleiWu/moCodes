#ifndef __FILE_MGR_H__
#define __FILE_MGR_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "moCpsUtils.h"

/*
    Do init, must input a directory to this module, we will do this directory;
*/
int fmInit(const char * pDirpath);

/*
    Do uninit, free our resources;
*/
void fmUnInit();

/*
    Get the length of filelist, caller must use this function firstly, then 
    malloc memory in this size, then call @fmGetFilelist to get file info.
*/
int fmGetFileinfoLength(int * pLen);

/*
    Get file list info from this directory;
*/
int fmGetFileinfo(char * pFileinfo, const int len);

/*
    Read file;
*/
int fmReadFile(const MOCPS_BASIC_FILEINFO fileInfo, const int offset, const int length, char *pBuf);

#ifdef __cplusplus
}
#endif

#endif


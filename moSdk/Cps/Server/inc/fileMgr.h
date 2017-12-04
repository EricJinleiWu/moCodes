#ifndef __FILE_MGR_H__
#define __FILE_MGR_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
    Do init, must input a directory to this module, we will do this directory;
*/
int fmInit(const char * pDirpath);

/*
    Do uninit, free our resources;
*/
void fmUnInit();

/*
    Get file list info from this directory;
*/
int fmGetDirInfo();

/*
    Read file;
*/
int fmReadFile(const char *pFilepath, const int offset, const int length, char *pBuf);

#ifdef __cplusplus
}
#endif

#endif


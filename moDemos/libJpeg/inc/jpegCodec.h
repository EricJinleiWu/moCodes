#ifndef __JPG_CODEC_H__
#define __JPG_CODEC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "jpeglib.h"
#include "jerror.h"

#define JPEGCODEC_ERR_OK                        0
#define JPEGCODEC_ERR_INPUTNULL                 -1
#define JPEGCODEC_ERR_OPENFILE                  -2
#define JPEGCODEC_ERR_INVALID_INPUTPARAM        -3
#define JPEGCODEC_ERR_DONOT_SUPPORT_FEATURES    -4
#define JPEGCODEC_ERR_INTERNAL                  -5  //some libJpeg API calling return error
#define JPEGCODEC_ERR_INVALID_JPG_CONTENT       -6  //input file or memory, contents donot valid jpeg format

#define MAX_FILEPATH_LEN    256

/*
    this pointer to define a function, this function will be called when libJpeg error ocurred;
*/
typedef int (*doErrCallback)();

typedef enum
{
    JPEGCODEC_DATA_POLICY_FILE,
    JPEGCODEC_DATA_POLICY_MEM
}JPEGCODEC_DATA_POLICY;

typedef struct
{
    unsigned char * pBuf;
    unsigned long int bufLen;
}JPEGCODEC_MEM_CONTENT;

typedef union
{
    JPEGCODEC_MEM_CONTENT mem;
    char filepath[MAX_FILEPATH_LEN];
}JPEGCODEC_DATA_CONTENT;

/*
    decompress a jpeg;

    support : from a jpeg file / a memory which save a jpeg; to a jpeg file;
    donot support currently : to a memory which save a jpeg;
        maybe because decompress is too memory waste to save it;

    if @pErrFunc is NULL, will use default libJpeg error function; else, when error ocurred, 
    @pErrFunc will be called;

    return 0 if succeed, 0- else;
*/
int jpegcodecDecompress(const J_COLOR_SPACE jcs, doErrCallback pErrFunc,
    const JPEGCODEC_DATA_POLICY srcDataPolicy, JPEGCODEC_DATA_CONTENT srcDataContent, 
    const JPEGCODEC_DATA_POLICY dstDataPolicy, JPEGCODEC_DATA_CONTENT dstDataContent);

/*
    TODO
*/
int jpegcodecCompress(const J_COLOR_SPACE jcs, doErrCallback pErrFunc, 
    const JPEGCODEC_DATA_POLICY srcDataPolicy, JPEGCODEC_DATA_CONTENT srcDataContent, 
    const JPEGCODEC_DATA_POLICY dstDataPolicy, JPEGCODEC_DATA_CONTENT dstDataContent);


#ifdef __cplusplus
}
#endif

#endif

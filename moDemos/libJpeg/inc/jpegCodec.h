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
#define JPEGCODEC_ERR_READFILE                  -7  //readFile function failed! this function just read file content to a memory
#define JPEGCODEC_ERR_MALLOC                    -8  //malloc failed

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

typedef enum
{
    INOUT_COLOR_SPACE_RGB888,
    INOUT_COLOR_SPACE_YUV444,
    INOUT_COLOR_SPACE_YUV420P,

    INOUT_COLOR_SPACE_MAX
}INOUT_COLOR_SPACE;

/*
    decompress a jpeg;

    support : from a jpeg file / a memory which save a jpeg; to a dst file;
    donot support currently : to a memory which save a jpeg;
        maybe because decompress is too memory waste to save it;

    colorSpace: The dst color space;
    pErrFunc: The function which will be called when error ocurred in libJpeg, this can be NULL, if NULL, when error ocurred, 
                libjpeg will abort the process who call it;
    srcDataPolicy: src file or memory;
    srcDataContent: if @srcDataPolicy == JPEGCODEC_DATA_POLICY_FILE, @srcDataContent is the filepath of srcfile;
                    else, @srcDataContent is the memory pointer of src memory;
    dstDataPolicy: dst file or memory; donot used currently;
                    because it needed too much memory......
    dstDataConent: same with @srcDataContent;

    if @pErrFunc is NULL, will use default libJpeg error function; else, when error ocurred, 
    @pErrFunc will be called;

    return 0 if succeed, 0- else;
*/
int jpegcodecDecompress(const INOUT_COLOR_SPACE colorSpace, doErrCallback pErrFunc,
    const JPEGCODEC_DATA_POLICY srcDataPolicy, JPEGCODEC_DATA_CONTENT srcDataContent, 
    /*const JPEGCODEC_DATA_POLICY dstDataPolicy, */JPEGCODEC_DATA_CONTENT dstDataContent);

/*
    compress to jpeg;

    @parameters same with jpegcodecDecompress();

    return 0 if succeed, 0- else;
*/
int jpegcodecCompress(doErrCallback pErrFunc, const INOUT_COLOR_SPACE colorSpace, 
    const JPEGCODEC_DATA_POLICY srcDataPolicy, JPEGCODEC_DATA_CONTENT srcDataContent, 
    const int srcWidth, const int srcHeight, const int quality,
    JPEGCODEC_DATA_CONTENT dstDataContent);


#ifdef __cplusplus
}
#endif

#endif

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>

#include "jpegCodec.h"

#include <setjmp.h>


#define dbgDebug(format, ...) printf("[%s, %d--debug] : "format, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define dbgError(format, ...) printf("[%s, %d--error] : "format, __FUNCTION__, __LINE__, ##__VA_ARGS__)

typedef struct 
{
    FILE * fp;  //if filepath being used, @fp is needed, else this will meaningless
    
}TASKINFO;


struct my_error_mgr {
  struct jpeg_error_mgr pub;	/* "public" fields */

  jmp_buf setjmp_buffer;	/* for return to caller */
};

typedef struct my_error_mgr * my_error_ptr;

static doErrCallback gpErrCallbackFunc = NULL;

/*
 * Here's the routine that will replace the standard error_exit method:
 */
void my_error_exit (j_common_ptr cinfo)
{
    /* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
    my_error_ptr myerr = (my_error_ptr) cinfo->err;

    /* Always display the message. */
    /* We could postpone this until after returning, if we chose. */
    (*cinfo->err->output_message) (cinfo);

    if(gpErrCallbackFunc)   gpErrCallbackFunc();

    /* Return control to the setjmp point */
    longjmp(myerr->setjmp_buffer, 1);
}

static char * getJcsDesc(const J_COLOR_SPACE jcs)
{
    static char desc[JCS_BG_YCC + 1][16] = {
        "unknown",
        "grayscale",
        "rgb",
        "yuv",
        "cmyk",
        "ycck",
        "bg_rgb",
        "bg_ycc"
        };
    switch(jcs)
    {
        case JCS_GRAYSCALE:
        case JCS_RGB:
        case JCS_YCbCr:
        case JCS_CMYK:
        case JCS_YCCK:
        case JCS_BG_RGB:
        case JCS_BG_YCC:
            return desc[jcs];
        default:
            return desc[0];
    }
}

static char * getDataPolicyDesc(const JPEGCODEC_DATA_POLICY policy)
{
    static char desc[JPEGCODEC_DATA_POLICY_MEM + 2][16] = {
        "DataFromFile",
        "DataFromMemory",
        "UnknownPolicy"
        };
    switch(policy)
    {
        case JPEGCODEC_DATA_POLICY_MEM:
        case JPEGCODEC_DATA_POLICY_FILE:
            return desc[policy];
        default:
            return desc[JPEGCODEC_DATA_POLICY_MEM + 1];
    }
}

static int setSrcToDecompressInfo(struct jpeg_decompress_struct * pCinfo, TASKINFO * pTaskInfo, 
    const JPEGCODEC_DATA_POLICY srcDataPolicy, JPEGCODEC_DATA_CONTENT srcDataContent)
{
    if(NULL == pCinfo || NULL == pTaskInfo)
    {
        dbgError("Input param is NULL.\n");
        return JPEGCODEC_ERR_INPUTNULL;
    }

    //To avoid handlers missing, this is necessary to us.
    if(pTaskInfo->fp != NULL)
    {
        fclose(pTaskInfo->fp);
        pTaskInfo->fp = NULL;
    }
    
    if(srcDataPolicy == JPEGCODEC_DATA_POLICY_FILE)
    {
        FILE * srcFp = NULL;
        srcFp = fopen(srcDataContent.filepath, "rb");
        if(NULL == srcFp)
        {
            dbgError("open file [%s] for read failed! errno=%d, desc=[%s]\n", srcDataContent.filepath, errno, strerror(errno));
            return JPEGCODEC_ERR_OPENFILE;
        }
        //set this filehandler to jpeglib
        jpeg_stdio_src(pCinfo, srcFp);  //return void
        pTaskInfo->fp = srcFp;
    }
    else
    {
        jpeg_mem_src(pCinfo, srcDataContent.mem.pBuf, srcDataContent.mem.bufLen);  //return void
    }

    dbgDebug("srcDataPolicy=%d(%s), set source to cinfo succeed.\n", srcDataPolicy, getDataPolicyDesc(srcDataPolicy));
    return JPEGCODEC_ERR_OK;
}

static int isSupportFeatures(const J_COLOR_SPACE jcs, const JPEGCODEC_DATA_POLICY dstDataPolicy)
{
    int ret = 1;
    switch(jcs)
    {
        case JCS_CMYK:
        case JCS_YCCK:
        case JCS_BG_RGB:
        case JCS_BG_YCC:
            ret = 0;
            break;
        default:
            break;
    }

    if(ret)
    {
        ret = dstDataPolicy == JPEGCODEC_DATA_POLICY_MEM ? 0 : 1;
    }

    return ret;
}

/*
    donot support these features now:
        jcs in : JCS_CMYK || JCS_YCCK || JCS_BG_RGB || JCS_BG_YCC;
        dstDataPolicy in : JPEGCODEC_DATA_POLICY_MEM;
*/
int jpegcodecDecompress(const J_COLOR_SPACE jcs, doErrCallback pErrFunc, 
    const JPEGCODEC_DATA_POLICY srcDataPolicy, JPEGCODEC_DATA_CONTENT srcDataContent, 
    const JPEGCODEC_DATA_POLICY dstDataPolicy, JPEGCODEC_DATA_CONTENT dstDataContent)
{
    if(jcs <= JCS_UNKNOWN || jcs >JCS_BG_YCC)
    {
        dbgError("Input jcs=%d, donot in valid range : (%d, %d]\n", jcs, JCS_UNKNOWN, JCS_BG_YCC);
        return JPEGCODEC_ERR_INVALID_INPUTPARAM;
    }
    if(srcDataPolicy > JPEGCODEC_DATA_POLICY_MEM || dstDataPolicy > JPEGCODEC_DATA_POLICY_MEM)
    {
        dbgError("Input srcDataPolicy=%d, dstDataPolicy=%d, donot valid!\n", srcDataPolicy, dstDataPolicy);
        return JPEGCODEC_ERR_INVALID_INPUTPARAM;
    }
    dbgDebug("Input jcs=%d(%s), srcDataPolicy=%d(%s), dstDataPolicy=%d(%s)\n", 
        jcs, getJcsDesc(jcs), srcDataPolicy, getDataPolicyDesc(srcDataPolicy),
        dstDataPolicy, getDataPolicyDesc(dstDataPolicy));

    if(!isSupportFeatures(jcs, dstDataPolicy))
    {
        dbgError("jcs=%d, dstDataPolicy=%d, some param donot in our support features currently.\n", jcs, dstDataPolicy);
        return JPEGCODEC_ERR_DONOT_SUPPORT_FEATURES;
    }
        
    struct jpeg_decompress_struct cinfo;

    //if @pErrFunc is NULL, default will be used;
    struct jpeg_error_mgr jerr;
    struct my_error_mgr myErrMgr;
    if(pErrFunc == NULL)
    {
        cinfo.err = jpeg_std_error(&jerr);
    }
    else
    {
        /* We set up the normal JPEG error routines, then override error_exit. */
        cinfo.err = jpeg_std_error(&myErrMgr.pub);
        myErrMgr.pub.error_exit = my_error_exit;
        /* Establish the setjmp return context for my_error_exit to use. */
        if (setjmp(myErrMgr.setjmp_buffer)) 
        {
            /* If we get here, the JPEG code has signaled an error.
             * We need to clean up the JPEG object, close the input file, and return.
             */
            jpeg_destroy_decompress(&cinfo);
            return JPEGCODEC_ERR_INTERNAL;
        }
        gpErrCallbackFunc = pErrFunc;
        dbgDebug("set error handler succeed.\n");
    }
    jpeg_create_decompress(&cinfo); //return void
    //set src to this @cinfo
    TASKINFO taskInfo;
    memset(&taskInfo, 0x00, sizeof(TASKINFO));
    int ret = setSrcToDecompressInfo(&cinfo, &taskInfo, srcDataPolicy, srcDataContent);
    if(ret != 0)
    {
        dbgError("setSrcToDecompressInfo failed! ret=%d\n", ret);
        return ret;
    }
    dbgDebug("step1 : set source to cinfo succeed.\n");
    
    //read header, this can help codec to do works later
    ret = jpeg_read_header(&cinfo, TRUE);
    if(ret != JPEG_HEADER_OK)
    {
        dbgError("jpeg_read_header failed! ret=%d\n", ret);
        jpeg_destroy_decompress(&cinfo);
        if(taskInfo.fp != NULL)
        {
            fclose(taskInfo.fp);
            taskInfo.fp = NULL;
        }
        return JPEGCODEC_ERR_INVALID_JPG_CONTENT;
    }
    dbgDebug("step2 : jpeg_read_header succeed.\n");
    
    //modify output color space here, this must be done before call @jpeg_start_decompress()
    cinfo.out_color_space = jcs;
    //start decompression, this return bool
    if(!jpeg_start_decompress(&cinfo))
    {
        dbgError("jpeg_start_decompress failed!\n");
        jpeg_destroy_decompress(&cinfo);
        if(taskInfo.fp != NULL)
        {
            fclose(taskInfo.fp);
            taskInfo.fp = NULL;
        }
        return JPEGCODEC_ERR_INTERNAL;
    }
    dbgDebug("step3 : jpeg_start_decompress succeed.\n");

    //open dst file, currently I donot support dst memory, so just file is OK.
    FILE * dstFp = NULL;
    dstFp = fopen(dstDataContent.filepath, "wb");
    if(dstFp == NULL)
    {
        dbgError("open file [%s] for write failed! errno=%d, desc=[%s]\n", dstDataContent.filepath, errno, strerror(errno));
        jpeg_finish_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);
        if(taskInfo.fp != NULL)
        {
            fclose(taskInfo.fp);
            taskInfo.fp = NULL;
        }
        return JPEGCODEC_ERR_OPENFILE;
    }
    dbgDebug("open file [%s] for write succeed.\n", dstDataContent.filepath);
    //calc stride, output_components will be set automatically, specially, to GRAYSCALE, this will be set to 1, others being set to 3
    int row_stride = cinfo.output_width * cinfo.output_components;
    dbgDebug("width=%d, components=%d, row_stride=%d\n", cinfo.output_width, cinfo.output_components, row_stride);
    JSAMPARRAY sampArray;
    sampArray = (*cinfo.mem->alloc_sarray)( (j_common_ptr)&cinfo, JPOOL_IMAGE, row_stride, 1);
    //read data, and do it
    while(cinfo.output_scanline < cinfo.output_height)
    {
        jpeg_read_scanlines(&cinfo, sampArray, 1);  //read JDIMENSION(unsigned int), not need to get ret value;
        fwrite(sampArray[0], 1, row_stride, dstFp);
        fflush(dstFp);
    }
    dbgDebug("step4 : all lines being read, and write to dst file.\n");

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);    
    fclose(dstFp);
    dstFp = NULL;
    if(taskInfo.fp != NULL)
    {
        fclose(taskInfo.fp);
        taskInfo.fp = NULL;
    }

    return JPEGCODEC_ERR_OK;
}
    
int jpegcodecCompress(const J_COLOR_SPACE jcs, doErrCallback pErrFunc, 
    const JPEGCODEC_DATA_POLICY srcDataPolicy, JPEGCODEC_DATA_CONTENT srcDataContent, 
    const JPEGCODEC_DATA_POLICY dstDataPolicy, JPEGCODEC_DATA_CONTENT dstDataContent)
{
    //TODO,
    return 0;
}


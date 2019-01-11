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

static int isSupportFeatures(const INOUT_COLOR_SPACE colorSpace)
{
    int ret = 0;
    switch(colorSpace)
    {
        case INOUT_COLOR_SPACE_RGB888:
        case INOUT_COLOR_SPACE_YUV420P:
        case INOUT_COLOR_SPACE_YUV444:
            ret = 1;
            break;
        default:
            break;
    }

    return ret;
}

static int getJcs(J_COLOR_SPACE *pJcs, int *pIsStanJcs, const INOUT_COLOR_SPACE colorSpace)
{
    int ret = 0;
    switch(colorSpace)
    {
        case INOUT_COLOR_SPACE_RGB888:
            *pJcs = JCS_RGB;
            *pIsStanJcs = 1;
            ret = 0;
            break;
        case INOUT_COLOR_SPACE_YUV444:
            *pJcs = JCS_YCbCr;
            *pIsStanJcs = 1;
            ret = 0;
            break;
        case INOUT_COLOR_SPACE_YUV420P:
            *pJcs = JCS_YCbCr;
            *pIsStanJcs = 0;
            ret = 0;
            break;
        default:
            dbgError("Input colorSpace=%d, invalid value.\n", colorSpace);
            ret = -1;
            break;
    }
    return ret;
}

static int doDecompress(struct jpeg_decompress_struct cinfo, const INOUT_COLOR_SPACE colorSpace, FILE * dstFp)
{
    int ret = JPEGCODEC_ERR_OK;

    //To libjpeg, we need use JCS_COLOR_SPACE
    J_COLOR_SPACE jcs;
    int isStanJcs = 0;  //is standard JCS
    ret = getJcs(&jcs, &isStanJcs, colorSpace);
    if(ret != 0)
    {
        dbgError("getJcs failed! ret=%d, colorSpace=%d\n", ret, colorSpace);
        return JPEGCODEC_ERR_INVALID_INPUTPARAM;
    }
    dbgDebug("getJcs succeed. colorSpace=%d, jcs=%d, isStanJcs=%d\n", colorSpace, jcs, isStanJcs);

    if(isStanJcs)
    {
        //modify output color space here, this must be done before call @jpeg_start_decompress()
        cinfo.out_color_space = jcs;
        //start decompression, this return bool
        if(!jpeg_start_decompress(&cinfo))
        {
            dbgError("jpeg_start_decompress failed!\n");
            return JPEGCODEC_ERR_INTERNAL;
        }
        dbgDebug("jpeg_start_decompress succeed.\n");

        //calc stride, output_components will be set automatically, specially, to GRAYSCALE, this will be set to 1, others being set to 3
        int row_stride = cinfo.output_width * cinfo.output_components;
        dbgDebug("cinfo.output_width=%d, cinfo.output_height=%d, components=%d, row_stride=%d\n", cinfo.output_width, cinfo.output_height, cinfo.output_components, row_stride);
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
    }
    else
    {
        //TODO, do it here.

        dbgError("I donot find method to do this part yet....\n");
        return JPEGCODEC_ERR_DONOT_SUPPORT_FEATURES;
    }

    return JPEGCODEC_ERR_OK;
}

/*
    donot support these features now:
        dstDataPolicy in : JPEGCODEC_DATA_POLICY_MEM;
*/
int jpegcodecDecompress(const INOUT_COLOR_SPACE colorSpace, doErrCallback pErrFunc, 
    const JPEGCODEC_DATA_POLICY srcDataPolicy, JPEGCODEC_DATA_CONTENT srcDataContent, 
    JPEGCODEC_DATA_CONTENT dstDataContent)
{
    if(colorSpace >= INOUT_COLOR_SPACE_MAX)
    {
        dbgError("Input colorSpace=%d, donot in valid range : [%d, %d)\n", colorSpace, 0, INOUT_COLOR_SPACE_MAX);
        return JPEGCODEC_ERR_INVALID_INPUTPARAM;
    }
    if(srcDataPolicy > JPEGCODEC_DATA_POLICY_MEM)
    {
        dbgError("Input srcDataPolicy=%d, donot valid!\n", srcDataPolicy);
        return JPEGCODEC_ERR_INVALID_INPUTPARAM;
    }
    dbgDebug("Input srcDataPolicy=%d(%s)\n", srcDataPolicy, getDataPolicyDesc(srcDataPolicy));

    if(!isSupportFeatures(colorSpace))
    {
        dbgError("jcs=%d, some param donot in our support features currently.\n", colorSpace);
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
    dbgDebug("step3 : open file [%s] for write succeed.\n", dstDataContent.filepath);

    ret = doDecompress(cinfo, colorSpace, dstFp);
    if(ret != 0)
    {
        dbgError("doDecompress failed! ret=%d\n", ret);
    }
    else
    {
        dbgDebug("doDecompress succeed.\n");
    }
    jpeg_destroy_decompress(&cinfo);    
    fclose(dstFp);
    dstFp = NULL;
    if(taskInfo.fp != NULL)
    {
        fclose(taskInfo.fp);
        taskInfo.fp = NULL;
    }

    return ret;
}

static unsigned char * readFile(const char * pFilepath)
{
    if(NULL == pFilepath)
    {
        dbgError("Input param is NULL.\n");
        return NULL;
    }

    struct stat curStat;
    int ret = stat(pFilepath, &curStat);
    if(ret != 0)
    {
        dbgError("stat failed! ret=%d, errno=%d, desc=[%s]\n", ret, errno, strerror(errno));
        return NULL;
    }
    int filelength = curStat.st_size;

    //get file content
    FILE * fp = NULL;
    fp = fopen(pFilepath, "rb");
    if(NULL == fp)
    {
        dbgError("open file [%s] for read failed! errno=%d, desc=[%s]\n", pFilepath, errno, strerror(errno));
        return NULL;
    }
    
    JSAMPLE * pImageBuffer = NULL;
    pImageBuffer = (JSAMPLE *)malloc(sizeof(JSAMPLE) * filelength);
    if(NULL == pImageBuffer)
    {
        dbgError("malloc for image_buffer failed! errno=%d, desc=[%s]\n", errno, strerror(errno));
        fclose(fp);
        fp = NULL;
        return NULL;
    }
    fread(pImageBuffer, 1, filelength, fp);
    fclose(fp);
    fp = NULL;

    dbgDebug("readFile to content succeed.\n");
    return pImageBuffer;
}


static int doCompressYuv420p(struct jpeg_compress_struct cinfo, unsigned char * pImageBuffer, 
    const int srcWidth, const int srcHeight, const int quality)
{
    //should padding data if necessary
    int padImageWidth = srcWidth;
    if(srcWidth % 8 != 0)    padImageWidth = (((srcWidth / 8) + 1) * 8);
    int padImageHeight = srcHeight;
    if(srcHeight % 8 != 0)    padImageHeight = (((srcHeight / 8) + 1) * 8);
    dbgDebug("srcWidth=%d, srcHeight=%d, padImageWidth=%d, padImageHeight=%d\n", srcWidth, srcHeight, padImageWidth, padImageHeight);
    
    cinfo.in_color_space = JCS_YCbCr;
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);
    dbgDebug("set jpeg lib params over. quality=%d\n", quality);

    //to yuv420p, we should malloc some memory firstly
    JSAMPROW *rpY = NULL, *rpU = NULL, *rpV = NULL;
    rpY = (JSAMPROW * )malloc(sizeof(JSAMPROW) * padImageHeight);
    if(rpY == NULL) {dbgError("malloc failed! errno=%d, desc=[%s]\n", errno, strerror(errno)); return JPEGCODEC_ERR_MALLOC;}
    rpU = (JSAMPROW * )malloc(sizeof(JSAMPROW) * (padImageHeight / 2));
    if(rpU == NULL) {free(rpY); rpY = NULL; dbgError("malloc failed! errno=%d, desc=[%s]\n", errno, strerror(errno)); return JPEGCODEC_ERR_MALLOC;}
    rpV = (JSAMPROW * )malloc(sizeof(JSAMPROW) * (padImageHeight / 2));
    if(rpV == NULL) {free(rpY); rpY = NULL; free(rpU); rpU = NULL; dbgError("malloc failed! errno=%d, desc=[%s]\n", errno, strerror(errno)); return JPEGCODEC_ERR_MALLOC;}
    dbgDebug("rpY & rpU & rpV all being malloced!\n");

    //4. start compress now.
    cinfo.raw_data_in = TRUE;
    cinfo.jpeg_color_space = JCS_YCbCr;
    cinfo.do_fancy_downsampling = FALSE;
    cinfo.comp_info[0].h_samp_factor = cinfo.comp_info[0].v_samp_factor = 2;
    cinfo.comp_info[1].h_samp_factor = cinfo.comp_info[1].v_samp_factor = 1;
    cinfo.comp_info[2].h_samp_factor = cinfo.comp_info[2].v_samp_factor = 1;
    jpeg_start_compress(&cinfo, TRUE);
    dbgDebug("jpeg_start_compress over.\n");
    //5.start write data to dst file
    int k = 0;
    for(k = 0; k < padImageHeight; k += 2)
    {
        rpY[k] = pImageBuffer + k * srcWidth;
        rpY[k + 1] = pImageBuffer + (k + 1) * srcWidth;
        rpU[k / 2] = pImageBuffer + srcWidth * srcHeight + (k / 2) * (srcWidth%2==0 ? srcWidth/2 : (srcWidth + 1) / 2);
        rpV[k / 2] = pImageBuffer + (srcWidth%2==0?srcWidth:(srcWidth+1)) * (srcHeight%2==0?srcHeight:(srcHeight+1)) * 5 / 4 + (k / 2) * (srcWidth%2==0 ? srcWidth/2 : (srcWidth + 1) / 2);
    }
    dbgDebug("rpY & rpU & rpV all being set value yet.\n");
    JSAMPIMAGE pp[3];
    for(k = 0; k < padImageHeight; k += 2 * DCTSIZE)
    {
        pp[0] = &rpY[k];
        pp[1] = &rpU[k / 2];
        pp[2] = &rpV[k / 2];
        jpeg_write_raw_data(&cinfo, pp, 2 * DCTSIZE);
        dbgDebug("k = %d, image_height=%d\n", k, srcHeight);
    }
    
    jpeg_finish_compress(&cinfo);
    
    //finally, should free all resources
    free(rpY);
    rpY = NULL;
    free(rpU);
    rpU = NULL;
    free(rpV);
    rpV = NULL;
    
    return JPEGCODEC_ERR_OK;
}

    
int jpegcodecCompress(doErrCallback pErrFunc, const INOUT_COLOR_SPACE colorSpace, 
    const JPEGCODEC_DATA_POLICY srcDataPolicy, JPEGCODEC_DATA_CONTENT srcDataContent, 
    const int srcWidth, const int srcHeight, const int quality,
    JPEGCODEC_DATA_CONTENT dstDataContent)
{
    if(colorSpace >= INOUT_COLOR_SPACE_MAX)
    {
        dbgError("Input colorSpace=%d, donot in valid range : [%d, %d)\n", colorSpace, 0, INOUT_COLOR_SPACE_MAX);
        return JPEGCODEC_ERR_INVALID_INPUTPARAM;
    }
    if(srcDataPolicy > JPEGCODEC_DATA_POLICY_MEM)
    {
        dbgError("Input srcDataPolicy=%d, donot valid!\n", srcDataPolicy);
        return JPEGCODEC_ERR_INVALID_INPUTPARAM;
    }
    dbgDebug("Input srcDataPolicy=%d(%s)\n", srcDataPolicy, getDataPolicyDesc(srcDataPolicy));

    if(!isSupportFeatures(colorSpace))
    {
        dbgError("jcs=%d, some param donot in our support features currently.\n", colorSpace);
        return JPEGCODEC_ERR_DONOT_SUPPORT_FEATURES;
    }

    unsigned char * pImageBuffer = NULL;
    if(srcDataPolicy == JPEGCODEC_DATA_POLICY_FILE)
    {
        pImageBuffer = readFile(srcDataContent.filepath);
        if(NULL == pImageBuffer)
        {
            dbgError("readFile failed!\n");
            return JPEGCODEC_ERR_READFILE;
        }
    }
    else    pImageBuffer = srcDataContent.mem.pBuf;
    
    //start libJpeg api calling
    struct jpeg_compress_struct cinfo;
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

    //1.create compress info
    jpeg_create_compress(&cinfo);

    //2.set output handler
    FILE * fpOut = NULL;
    fpOut = fopen(dstDataContent.filepath, "wb");
    if(NULL == fpOut)
    {
        dbgError("open file [%s] for write failed! errno=%d, desc=[%s]\n", dstDataContent.filepath, errno, strerror(errno));
        if(srcDataPolicy == JPEGCODEC_DATA_POLICY_FILE)     {free(pImageBuffer); pImageBuffer = NULL;}
        return JPEGCODEC_ERR_OPENFILE;
    }
    jpeg_stdio_dest(&cinfo, fpOut);
    dbgDebug("jpeg_stdio_dest succeed.\n");
    
    //3.set params now.
    cinfo.image_height = srcHeight;
    cinfo.image_width = srcWidth;
    cinfo.input_components = 3;
    J_COLOR_SPACE jcs;
    int isStanJcs = 0;
    int ret = getJcs(&jcs, &isStanJcs, colorSpace);
    if(ret != 0)
    {
        dbgError("getJcs failed! ret=%d, colorSpace=%d\n", ret, colorSpace);
        if(srcDataPolicy == JPEGCODEC_DATA_POLICY_FILE)     {free(pImageBuffer); pImageBuffer = NULL;}
        return JPEGCODEC_ERR_INTERNAL;
    }
    dbgDebug("getJcs succeed. colorSpace=%d, jcs=%d, isStanJcs=%d\n", colorSpace, jcs, isStanJcs);

    if(isStanJcs)   //just RGB being test by these codes
    {
        cinfo.in_color_space = jcs;
        jpeg_set_defaults(&cinfo);
        jpeg_set_quality(&cinfo, quality, TRUE);

        //4. start compress now.
        jpeg_start_compress(&cinfo, TRUE);
        //5.start write data to dst file
        int row_stride = srcWidth * 3;   //each pixel has 3 bytes, R & G & B;
        JSAMPROW row_pointer[1];
        while(cinfo.next_scanline < cinfo.image_height)
        {
            row_pointer[0] = &pImageBuffer[cinfo.next_scanline * row_stride];
            jpeg_write_scanlines(&cinfo, row_pointer, 1);
        }
        
        jpeg_finish_compress(&cinfo);
    }
    else    //just yuv420P being test by these codes
    {
        if(colorSpace == INOUT_COLOR_SPACE_YUV420P) ret = doCompressYuv420p(cinfo, pImageBuffer, srcWidth, srcHeight, quality);
        //TODO, others being done here.

        if(ret != 0)
        {
            dbgError("do compress failed! colorSpace=%d, jcs=%d, ret=%d\n", colorSpace, jcs, ret);            
            jpeg_finish_compress(&cinfo);
            fflush(fpOut);
            fclose(fpOut);
            fpOut = NULL;
            if(srcDataPolicy == JPEGCODEC_DATA_POLICY_FILE)     {free(pImageBuffer); pImageBuffer = NULL;}
            return ret;
        }
        dbgDebug("do compress succeed.\n");
    }

    fflush(fpOut);
    fclose(fpOut);
    fpOut = NULL;
    if(srcDataPolicy == JPEGCODEC_DATA_POLICY_FILE)     {free(pImageBuffer); pImageBuffer = NULL;}

    jpeg_destroy_compress(&cinfo);
    
    return JPEGCODEC_ERR_OK;
}




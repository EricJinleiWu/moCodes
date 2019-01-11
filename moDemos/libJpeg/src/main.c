#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include "jpeglib.h"
#include "jerror.h"
#include "jpegCodec.h"

#define dbgDebug(format, ...) printf("[%s, %d--debug] : "format, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define dbgError(format, ...) printf("[%s, %d--error] : "format, __FUNCTION__, __LINE__, ##__VA_ARGS__)


static int getFileSize(const char * filepath)
{
    if(NULL == filepath)
    {
        dbgError("Input param is NULL.\n");
        return -1;
    }

    struct stat curStat;
    int ret = stat(filepath, &curStat);
    if(ret < 0)
    {
        dbgError("stat failed! ret=%d, errno=%d, desc=[%s]\n", ret, errno, strerror(errno));
        return -2;
    }
    
    return curStat.st_size;
}

/*
    YUVYUVYUVYUV.... -> YYYY..UUUU..VVVV..
    packed --> planar
*/
static int convertYuv444PackedToPlaner(const char * srcPackedYuv, const char * dstPlanerYuv)
{
    if(NULL == srcPackedYuv || NULL == dstPlanerYuv)
    {
        dbgError("input param is NULL.\n");
        return -1;
    }
    
    int fileSize = getFileSize(srcPackedYuv);
    if(fileSize < 0)
    {
        dbgError("get file size for [%s] failed!\n", srcPackedYuv);
        return -2;
    }
    if(fileSize % 3 != 0)
    {
        dbgError("fileSize=%d, cannot divide 3, I think, yuv444 format should has size divide 3;\n", fileSize);
        return -3;
    }
    
    int eachLen = fileSize / 3;
    //open two files firstly
    FILE * fp = NULL;
    fp = fopen(srcPackedYuv, "rb");
    if(fp == NULL)
    {
        dbgError("open file [%s] for read failed! errno=%d, desc=[%s]\n", srcPackedYuv, errno, strerror(errno));
        return -4;
    }
    FILE * dstFp = NULL;
    dstFp = fopen(dstPlanerYuv, "wb");
    if(dstFp == NULL)
    {
        dbgError("open file [%s] for write failed! errno=%d, desc=[%s]\n", dstPlanerYuv, errno, strerror(errno));
        fclose(fp);
        fp = NULL;
        return -5;
    }
    //read Y U V looply, and write to file
    int i = 0, yuvIdx = 0;
    for(yuvIdx = 0; yuvIdx < 3; yuvIdx++)
    {
        fseek(fp, yuvIdx, SEEK_SET);
        for(i = 0; i < eachLen; i++)
        {
            char value[1];
            fread(value, 1, 1, fp);
            fwrite(value, 1, 1, dstFp);
            fflush(dstFp);
            fseek(fp, 2, SEEK_CUR);
        }
    }
    //close files
    fclose(fp);
    fp = NULL;
    fclose(dstFp);
    dstFp = NULL;

    dbgDebug("convert from packed to planer for yuv 444 succeed.\n");
    return 0;
}

/*
    get file content, used for memory source;
*/
static char * getFileContent(const char * filepath, int * pFilesize)
{
    if(NULL == filepath || NULL == pFilesize)
    {
        dbgError("Input param is NULL.\n");
        return NULL;
    }

    FILE * fp = NULL;
    fp = fopen(filepath, "rb");
    if(fp == NULL)
    {
        dbgError("open file [%s] for read failed! errno=%d, desc=[%s]\n", filepath, errno, strerror(errno));
        return NULL;
    }
    dbgDebug("open file [%s] for read succeed.\n", filepath);

    fseek(fp, 0, SEEK_END);
    int filesize = ftell(fp);
    char * ret = NULL;
    ret = (char *)malloc(sizeof(char) * filesize);
    if(ret == NULL)
    {
        dbgError("malloc failed! errno=%d, desc=[%s]\n", errno, strerror(errno));
        fclose(fp);
        fp = NULL;
        return NULL;
    }
    dbgDebug("malloc succeed. size=%d\n", filesize);

    rewind(fp);
    fread(ret, 1, filesize, fp);
    *pFilesize = filesize;
//    dumpArray(ret, 10);

    fclose(fp);
    fp = NULL;

    return ret;
}

static int convertBgr2Rgb_8u(const char * bgrFilepath, const char * rgbFilepath)
{
    int filesize = getFileSize(bgrFilepath);
    if(filesize % 3 != 0)
    {
        dbgError("bgr file [%s] has size %d, donot valid value!\n", bgrFilepath, filesize);
        return -1;
    }

    FILE * fpSrc = NULL;
    fpSrc = fopen(bgrFilepath, "rb");
    if(NULL == fpSrc)
    {
        dbgError("open file [%s] for read failed! errno=%d, desc=[%s]\n", bgrFilepath, errno, strerror(errno));
        return -2;
    }
    FILE * fpDst = NULL;
    fpDst = fopen(rgbFilepath, "wb");
    if(NULL == fpDst)
    {
        dbgError("open file [%s] for write failed! errno=%d, desc=[%s]\n", rgbFilepath, errno, strerror(errno));
        fclose(fpSrc);
        fpSrc = NULL;
        return -3;
    }

    char src[3] = {0x00}, tmp[3] = {0x00};
    int i = 0;
    for(i = 0; i < filesize; i += 3)
    {
        fread(src, 1, 3, fpSrc);
        tmp[0] = src[2];
        tmp[1] = src[1];
        tmp[2] = src[0];
        fwrite(tmp, 1, 3, fpDst);
        fflush(fpDst);
    }

    dbgDebug("write from bgr to rgb ok.\n");
    fclose(fpSrc);
    fpSrc = NULL;
    fclose(fpDst);
    fpDst = NULL;

    return 0;
}

int tstErrFunc()
{
    printf("enter tstErrFunc now.\n");
    return 0;
}

int main(int argc, char **argv)
{
    int ret = 0;

#if 1
    JPEGCODEC_DATA_CONTENT srcContent, dstContent;
    memset(&srcContent, 0x00, sizeof(JPEGCODEC_DATA_CONTENT));
    strcpy(srcContent.filepath, "./jpgFiles/1.jpg");
    
    memset(&dstContent, 0x00, sizeof(JPEGCODEC_DATA_CONTENT));    
    strcpy(dstContent.filepath, "./dstFiles/1.rgb888");
    jpegcodecDecompress(INOUT_COLOR_SPACE_RGB888, tstErrFunc, JPEGCODEC_DATA_POLICY_FILE, srcContent, dstContent);
    
    memset(&dstContent, 0x00, sizeof(JPEGCODEC_DATA_CONTENT));    
    strcpy(dstContent.filepath, "./dstFiles/1.yuv444");
    jpegcodecDecompress(INOUT_COLOR_SPACE_YUV444, tstErrFunc, JPEGCODEC_DATA_POLICY_FILE, srcContent, dstContent);



    memset(&srcContent, 0x00, sizeof(JPEGCODEC_DATA_CONTENT));
    strcpy(srcContent.filepath, "./dstFiles/1.yuv444");
    memset(&dstContent, 0x00, sizeof(JPEGCODEC_DATA_CONTENT));    
    strcpy(dstContent.filepath, "./dstFiles/1.yuv444.jpeg");
    jpegcodecCompress(tstErrFunc, INOUT_COLOR_SPACE_YUV444, JPEGCODEC_DATA_POLICY_FILE, srcContent, 250, 250, 90, dstContent);
    
    memset(&srcContent, 0x00, sizeof(JPEGCODEC_DATA_CONTENT));
    strcpy(srcContent.filepath, "./dstFiles/1.rgb888");
    memset(&dstContent, 0x00, sizeof(JPEGCODEC_DATA_CONTENT));    
    strcpy(dstContent.filepath, "./dstFiles/1.rgb888.jpeg");
    jpegcodecCompress(tstErrFunc, INOUT_COLOR_SPACE_RGB888, JPEGCODEC_DATA_POLICY_FILE, srcContent, 250, 250, 90, dstContent);


    memset(&srcContent, 0x00, sizeof(JPEGCODEC_DATA_CONTENT));
    strcpy(srcContent.filepath, "./dstFiles/Kate_Capshaw_0001_yuv420p_w256_h256.yuv");
    memset(&dstContent, 0x00, sizeof(JPEGCODEC_DATA_CONTENT));    
    strcpy(dstContent.filepath, "./dstFiles/Kate_Capshaw_0001_yuv420p_w256_h256.jpeg");
    jpegcodecCompress(tstErrFunc, INOUT_COLOR_SPACE_YUV420P, JPEGCODEC_DATA_POLICY_FILE, srcContent, 256, 256, 90, dstContent);
    
    memset(&srcContent, 0x00, sizeof(JPEGCODEC_DATA_CONTENT));
    strcpy(srcContent.filepath, "./dstFiles/Kate_Capshaw_0001_yuv420p_w251_h251.yuv");
    memset(&dstContent, 0x00, sizeof(JPEGCODEC_DATA_CONTENT));    
    strcpy(dstContent.filepath, "./dstFiles/Kate_Capshaw_0001_yuv420p_w251_h251.jpeg");
    jpegcodecCompress(tstErrFunc, INOUT_COLOR_SPACE_YUV420P, JPEGCODEC_DATA_POLICY_FILE, srcContent, 251, 251, 90, dstContent);
    
    memset(&srcContent, 0x00, sizeof(JPEGCODEC_DATA_CONTENT));
    strcpy(srcContent.filepath, "./dstFiles/Kate_Capshaw_0001_yuv420p_w250_h250.yuv");
    memset(&dstContent, 0x00, sizeof(JPEGCODEC_DATA_CONTENT));    
    strcpy(dstContent.filepath, "./dstFiles/Kate_Capshaw_0001_yuv420p_w250_h250.jpeg");
    jpegcodecCompress(tstErrFunc, INOUT_COLOR_SPACE_YUV420P, JPEGCODEC_DATA_POLICY_FILE, srcContent, 250, 250, 90, dstContent);
    
    memset(&srcContent, 0x00, sizeof(JPEGCODEC_DATA_CONTENT));
    strcpy(srcContent.filepath, "./dstFiles/Kate_Capshaw_0001_yuv420p_w250_h256.yuv");
    memset(&dstContent, 0x00, sizeof(JPEGCODEC_DATA_CONTENT));    
    strcpy(dstContent.filepath, "./dstFiles/Kate_Capshaw_0001_yuv420p_w250_h256.jpeg");
    jpegcodecCompress(tstErrFunc, INOUT_COLOR_SPACE_YUV420P, JPEGCODEC_DATA_POLICY_FILE, srcContent, 250, 256, 90, dstContent);
    
    memset(&srcContent, 0x00, sizeof(JPEGCODEC_DATA_CONTENT));
    strcpy(srcContent.filepath, "./dstFiles/Kate_Capshaw_0001_yuv420p_w251_h256.yuv");
    memset(&dstContent, 0x00, sizeof(JPEGCODEC_DATA_CONTENT));    
    strcpy(dstContent.filepath, "./dstFiles/Kate_Capshaw_0001_yuv420p_w251_h256.jpeg");
    jpegcodecCompress(tstErrFunc, INOUT_COLOR_SPACE_YUV420P, JPEGCODEC_DATA_POLICY_FILE, srcContent, 251, 256, 90, dstContent);
    
    memset(&srcContent, 0x00, sizeof(JPEGCODEC_DATA_CONTENT));
    strcpy(srcContent.filepath, "./dstFiles/Kate_Capshaw_0001_yuv420p_w250_h251.yuv");
    memset(&dstContent, 0x00, sizeof(JPEGCODEC_DATA_CONTENT));    
    strcpy(dstContent.filepath, "./dstFiles/Kate_Capshaw_0001_yuv420p_w250_h251.jpeg");
    jpegcodecCompress(tstErrFunc, INOUT_COLOR_SPACE_YUV420P, JPEGCODEC_DATA_POLICY_FILE, srcContent, 250, 251, 90, dstContent);
    
    memset(&srcContent, 0x00, sizeof(JPEGCODEC_DATA_CONTENT));
    strcpy(srcContent.filepath, "./dstFiles/Kate_Capshaw_0001_yuv420p_w251_h250.yuv");
    memset(&dstContent, 0x00, sizeof(JPEGCODEC_DATA_CONTENT));    
    strcpy(dstContent.filepath, "./dstFiles/Kate_Capshaw_0001_yuv420p_w251_h250.jpeg");
    jpegcodecCompress(tstErrFunc, INOUT_COLOR_SPACE_YUV420P, JPEGCODEC_DATA_POLICY_FILE, srcContent, 251, 250, 90, dstContent);
    
#endif


    return 0;
}

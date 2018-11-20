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
    memset(&dstContent, 0x00, sizeof(JPEGCODEC_DATA_CONTENT));

    //from file decompress to file
    strcpy(srcContent.filepath, "./jpgFiles/1.jpg");
    strcpy(dstContent.filepath, "./dstFiles/1.yuv");
    ret = jpegcodecDecompress(JCS_YCbCr, tstErrFunc, JPEGCODEC_DATA_POLICY_FILE, srcContent, JPEGCODEC_DATA_POLICY_FILE, dstContent);   //yuv444packed
    printf(">>>>>> from jpg file, to yuv file, ret=%d\n", ret);
    strcpy(dstContent.filepath, "./dstFiles/1.rgb");
    ret = jpegcodecDecompress(JCS_RGB, tstErrFunc, JPEGCODEC_DATA_POLICY_FILE, srcContent, JPEGCODEC_DATA_POLICY_FILE, dstContent); //bgr888
    printf(">>>>>> from jpg file, to rgb file, ret=%d\n", ret);
    strcpy(dstContent.filepath, "./dstFiles/1.grayscale");
    ret = jpegcodecDecompress(JCS_GRAYSCALE, tstErrFunc, JPEGCODEC_DATA_POLICY_FILE, srcContent, JPEGCODEC_DATA_POLICY_FILE, dstContent);
    printf(">>>>>> from jpg file, to grayscale file, ret=%d\n", ret);

    //from memory decomporess to file
    int filesize = 0;
    char * filecontent = getFileContent("./jpgFiles/1.jpg", &filesize);
    if(NULL == filecontent)
    {
        dbgError("getFileContent failed!\n");
        return -1;
    }
    dbgDebug("getFileContent succeed.\n");
    
    srcContent.mem.pBuf = (unsigned char *)filecontent;
    srcContent.mem.bufLen = filesize;
    strcpy(dstContent.filepath, "./dstFiles/1_fromMemory.yuv");
    ret = jpegcodecDecompress(JCS_YCbCr, NULL, JPEGCODEC_DATA_POLICY_MEM, srcContent, JPEGCODEC_DATA_POLICY_FILE, dstContent);
    printf(">>>>>> from memory, to yuv file, ret=%d\n", ret);
    strcpy(dstContent.filepath, "./dstFiles/1_fromMemory.rgb");
    ret = jpegcodecDecompress(JCS_RGB, NULL, JPEGCODEC_DATA_POLICY_MEM, srcContent, JPEGCODEC_DATA_POLICY_FILE, dstContent);
    printf(">>>>>> from memory, to rgb file, ret=%d\n", ret);
    strcpy(dstContent.filepath, "./dstFiles/1_fromMemory.grayscale");
    ret = jpegcodecDecompress(JCS_GRAYSCALE, NULL, JPEGCODEC_DATA_POLICY_MEM, srcContent, JPEGCODEC_DATA_POLICY_FILE, dstContent);
    printf(">>>>>> from memory, to grayscale file, ret=%d\n", ret);

    free(filecontent);
    filecontent = NULL;
#endif

#if 0
    convertBgr2Rgb_8u("./dstFiles/1.rgb", "./dstFiles/1_convert.rgb");
#endif

    return 0;
}

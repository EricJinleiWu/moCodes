#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <pthread.h>

#include "jpg2yuv.h"
#include "utils.h"
#include "decodeVideo2Pic.h"

/*
    tst the case : jpg convert to yuv;
*/
static void tstJpg2Yuv(const char * jpgFilepath, const char * yuvFilepath)
{   
    if(NULL == jpgFilepath || NULL == yuvFilepath)
    {
        dbgError("Input param is NULL.\n");
        return ;
    }

    if(0 != access(jpgFilepath, 0))
    {
        dbgError("jpgFile [%s] donot exist, cannot convert it.\n", jpgFilepath);
        return ;
    }

    int ret = jpg2yuv(jpgFilepath, yuvFilepath);
    if(ret != 0)
    {
        dbgError("jpg2yuv failed! ret=%d, jpgFilepath=[%s], yuvFilepath=[%s]\n",
            ret, jpgFilepath, yuvFilepath);
        return ;
    }
    dbgDebug("jpg2yuv succeed.\n");
}

static void tstDecodeVideo2Pic(const char * videoFilepath, const char * outPicDir, const OUTPUT_PICTYPE picType)
{
    if(NULL == videoFilepath || NULL == outPicDir)
    {
        dbgError("Input param is NULL.\n");
        return ;
    }
    if(0 != access(videoFilepath, 0) || 0 != access(outPicDir, 0))
    {
        dbgError("videoFilepath=[%s], outPicDir=[%s], must exist before convert.\n",
            videoFilepath, outPicDir);
        return ;
    }

    int ret = decodeVideo2Pic(videoFilepath, outPicDir, picType);    
    if(ret != 0)
    {
        dbgError("decodeVideo2Pic failed! ret=%d, videoFilepath=[%s], outPicDir=[%s]\n",
            ret, videoFilepath, outPicDir);
        return ;
    }
    dbgDebug("decodeVideo2pic succeed.\n");
}

int main(int argc, char **argv)
{
//    tstJpg2Yuv("./files/Kaio_Almeida_0001.jpg", "./files/Kaio_Almeida_0001.yuv");
    tstJpg2Yuv("./files/Iframe.data", "./files/Iframe.yuv");

//    tstDecodeVideo2Pic("./H264Files/slamtv10.264", "./outPicFiles/slamtv10.264/", OUTPUT_PICTYPE_PGM);

}


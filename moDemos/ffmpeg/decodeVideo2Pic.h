#ifndef __DECODE_VIDEO_2_PIC_H__
#define __DECODE_VIDEO_2_PIC_H__

#include "utils.h"

/*
    decode a video 2 pictures;
    @videoFilepath is the filepath of video;
    @outPicDirPath is the output directory which will save all pictures;
    @outPicType is the type of pictures;

    return 0 if succeed, 0- failed;
*/
int decodeVideo2Pic(const char * videoFilepath, const char * outPicDirPath, const OUTPUT_PICTYPE outPicType);

#endif
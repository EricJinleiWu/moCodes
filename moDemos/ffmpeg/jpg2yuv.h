#ifndef __MYTEST_JPG2YUV_H__
#define __MYTEST_JPG2YUV_H__

/*
    convert a jpg file to yuv420p;

    @jpgFilepath : the jpg file path;
    @yuvFilepath : the yuv file which will be wrote;

    return :
        0 : succeed;
        0-: failed;
*/
int jpg2yuv(const char * jpgFilepath, const char * yuvFilepath);

#endif
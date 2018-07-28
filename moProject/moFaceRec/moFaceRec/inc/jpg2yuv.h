#ifndef __JPG_2_YUV_H__
#define __JPG_2_YUV_H__


extern "C" {
#include "libavformat/avformat.h"
}

using namespace std;
#include <string>

class Jpg2Yuv
{
public:
    Jpg2Yuv();
    Jpg2Yuv(const Jpg2Yuv &other);
    ~Jpg2Yuv();

public:
    /*
        convert @jpgFilepath to @yuvFilepath;
        currently, convert to yuv420p;
        return 0 if succeed, 0- else;
    */
    virtual int convert(const string & jpgFilepath, const string & yuvFilepath);

private:
    /*
        write yuv file;
        return 0 if succeed, 0- else;
    */
    virtual int writeFrame2YuvFile(AVFrame *pFrame, AVCodecContext *pCodecCtx, const string &yuvFilepath);
};

#endif
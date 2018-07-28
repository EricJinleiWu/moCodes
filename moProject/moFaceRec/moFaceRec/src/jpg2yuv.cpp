#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <sys/types.h>
#include <time.h>

extern "C" {
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
}

using namespace std;
#include <list>
#include <map>
#include <string>

#include "jpg2yuv.h"
#include "utils.h"

Jpg2Yuv::Jpg2Yuv()
{
    ;
}

Jpg2Yuv::Jpg2Yuv(const Jpg2Yuv & other)
{
    ;
}

Jpg2Yuv::~Jpg2Yuv()
{
    ;
}

int Jpg2Yuv::convert(const string & jpgFilepath, const string & yuvFilepath)
{
    if(0 != access(jpgFilepath.c_str(), 0) || 0 == access(yuvFilepath.c_str(), 0))
    {
        dbgError("jpgFilepath=[%s], must exist; yuvFilepath=[%s], must donot exist.\n",
            jpgFilepath.c_str(), yuvFilepath.c_str());
        return -1;
    }
    dbgDebug("jpgFilepath=[%s], yuvFilepath=[%s], start to convert now.\n",
        jpgFilepath.c_str(), yuvFilepath.c_str());

    //Use ffmepg to do this work
    av_register_all();
    AVFormatContext * pFmtCtx = NULL;
    int ret = avformat_open_input(&pFmtCtx, jpgFilepath.c_str(), NULL, NULL);
    if(ret != 0)
    {
        dbgError("avformat_open_input failed! ret=%d, jpgFilepath=[%s]\n", ret, jpgFilepath.c_str());
        return -2;
    }
    dbgDebug("step1 : avformat_open_input succeed.\n");
    //Find stream info, we just have a picture, can be parsed as a I frame    
    ret = avformat_find_stream_info(pFmtCtx, NULL);
    if(ret != 0)
    {
        dbgError("avformat_find_stream_info failed! ret=%d\n", ret);
        avformat_close_input(&pFmtCtx);
        pFmtCtx = NULL;
        return -3;
    }
    dbgDebug("step2 : avformat_find_stream_info succeed, nb_streams=%d.\n", pFmtCtx->nb_streams);
    //A picture, donot have audio, do a check here.
    if(pFmtCtx->nb_streams != 1 || pFmtCtx->streams[0]->codecpar->codec_type != AVMEDIA_TYPE_VIDEO)
    {
        dbgError("pFmtCtx->nb_streams=%d, pFmtCtx->streams[0]->codecpar->codec_type=%d, donot a picture!\n",
            pFmtCtx->nb_streams, pFmtCtx->streams[0]->codecpar->codec_type);
        avformat_close_input(&pFmtCtx);
        pFmtCtx = NULL;
        return -4;
    }
    dbgDebug("step3 : check picture params succeed.\n");
    //find the decoder for it.    
    AVCodec * pCodec = avcodec_find_decoder(pFmtCtx->streams[0]->codecpar->codec_id);
    if(NULL == pCodec)
    {
        dbgError("avcodec_find_decoder failed! codec_id=%d\n", pFmtCtx->streams[0]->codecpar->codec_id);
        avformat_close_input(&pFmtCtx);
        pFmtCtx = NULL;
        return -5;
    }
    dbgDebug("step4 : avcodec_find_decoder succeed, pCodecContext->codec_id=%d\n", pFmtCtx->streams[0]->codecpar->codec_id);
    //alloc for codec context
    AVCodecContext * pCodecContext = NULL;
    pCodecContext = avcodec_alloc_context3(pCodec);
    if(NULL == pCodecContext)
    {
        dbgError("avcodec_alloc_context3 failed!\n");
        avformat_close_input(&pFmtCtx);
        pFmtCtx = NULL;
        return -6;
    }
    dbgDebug("step5 : avcodec_alloc_context3 succeed.\n");
    //codec context should be set values    
    ret = avcodec_parameters_to_context(pCodecContext, pFmtCtx->streams[0]->codecpar);
    if(ret != 0)
    {
        dbgError("avcodec_parameters_to_context failed! ret=%d\n", ret);
        avcodec_free_context(&pCodecContext);
        pCodecContext = NULL;
        avformat_close_input(&pFmtCtx);
        pFmtCtx = NULL;
        return -7;
    }
    dbgDebug("step6 : avcodec_parameters_to_context succeed.\n");
    //open codec by params
    ret = avcodec_open2(pCodecContext, pCodec, NULL);
    if(ret != 0)
    {
        dbgError("avcodec_open2 failed, ret=%d\n", ret);
        avcodec_free_context(&pCodecContext);
        pCodecContext = NULL;
        avformat_close_input(&pFmtCtx);
        pFmtCtx = NULL;
        return -8;
    }
    dbgDebug("step7 : avcodec_open2 succeed.\n");
    //we need a packet and a frame structure, to do convert    
    AVPacket * pPacket = NULL;
    pPacket = av_packet_alloc();
    ret = av_read_frame(pFmtCtx, pPacket);
    if(ret != 0)
    {
        dbgError("av_read_frame failed, ret=%d\n", ret);
        avcodec_free_context(&pCodecContext);
        pCodecContext = NULL;
        avformat_close_input(&pFmtCtx);
        pFmtCtx = NULL;
        return -9;
    }
    dbgDebug("step8 : av_read_frame succeed, packet.size=%d.\n", pPacket->size);
    AVFrame * pFrame = NULL;
    pFrame = av_frame_alloc();
    if(NULL == pFrame)
    {
        dbgError("av_frame_alloc failed!\n");
        av_packet_free(&pPacket);    //av_free_packet has deprecated
        pPacket = NULL;
        avcodec_free_context(&pCodecContext);
        pCodecContext = NULL;
        avformat_close_input(&pFmtCtx);
        pFmtCtx = NULL;
        return -10;
    }
    dbgDebug("step9 : av_frame_alloc succeed.\n");
    //all preparations being done, start to do convert now!
    ret = avcodec_send_packet(pCodecContext, pPacket);
    if(ret != 0)
    {
        dbgError("avcodec_send_packet failed! ret=%d\n", ret);
        av_packet_free(&pPacket);    //av_free_packet has deprecated
        pPacket = NULL;
        avcodec_free_context(&pCodecContext);
        pCodecContext = NULL;
        avformat_close_input(&pFmtCtx);
        pFmtCtx = NULL;
        return -11;
    }
    dbgDebug("step10 : avcodec_send_packet succeed.\n");
    
    ret = avcodec_receive_frame(pCodecContext, pFrame);
    if(ret != 0)
    {
        dbgError("avcodec_receive_frame failed! ret=%d\n", ret);
        av_packet_free(&pPacket);    //av_free_packet has deprecated
        pPacket = NULL;
        avcodec_free_context(&pCodecContext);
        pCodecContext = NULL;
        avformat_close_input(&pFmtCtx);
        pFmtCtx = NULL;
        return -12;
    }
    dbgDebug("step11 : avcodec_receive_frame succeed. dump frame info below:\n");
    
    dbgDebug("pFrame->format=%d, pFrame->pict_type=%d, width=%d, height=%d, codecContext->width=%d, codecContext->height=%d, linesize=[%d, %d, %d]\n",
        pFrame->format, pFrame->pict_type, pFrame->width, pFrame->height, pCodecContext->width, pCodecContext->height,
        pFrame->linesize[0], pFrame->linesize[1], pFrame->linesize[2]);
    //write pFrame(decoded data) to yuv file now.    
    ret = writeFrame2YuvFile(pFrame, pCodecContext, yuvFilepath);
    if(ret != 0)
    {
        dbgError("writeFrame2YuvFile failed! ret=%d\n", ret);
        av_packet_free(&pPacket);    //av_free_packet has deprecated
        pPacket = NULL;
        avcodec_free_context(&pCodecContext);
        pCodecContext = NULL;
        avformat_close_input(&pFmtCtx);
        pFmtCtx = NULL;
        return -13;
    }
    dbgDebug("step12 : writeFrame2YuvFile succeed.\n");
    //free all resources
    av_frame_free(&pFrame);
    pFrame = NULL;
    av_packet_free(&pPacket);    //av_free_packet has deprecated
    pPacket = NULL;
    avcodec_free_context(&pCodecContext);
    pCodecContext = NULL;
    avformat_close_input(&pFmtCtx);
    pFmtCtx = NULL;
    dbgDebug("step13 : the end! all resource being free, convert over!\n");

    return 0;
}

/*
    Currently, just yuv420p is supported;
*/
int Jpg2Yuv::writeFrame2YuvFile(AVFrame *pFrame, AVCodecContext *pCodecCtx, const string &yuvFilepath)
{
    if(NULL == pFrame || NULL == pCodecCtx)
    {
        dbgError("Input param is NULL.\n");
        return -1;
    }

    if(pFrame->format != AV_PIX_FMT_YUVJ420P)
    {
        dbgError("pFrame->format=%d, donot AV_PIX_FMT_YUVJ420P(%d), donot deal with it now.\n",
            pFrame->format, AV_PIX_FMT_YUVJ420P);
        return -2;
    }
    dbgDebug("pFrame->format=%d, pFrame->width=%d, pFrame->height=%d\n", pFrame->format, pFrame->width, pFrame->height);

    FILE * fp = NULL;
    fp = fopen(yuvFilepath.c_str(), "wb");
    if(NULL == fp)
    {
        dbgError("Open yuv file [%s] for write failed! errno=%d, desc=[%s]\n",
            yuvFilepath.c_str(), errno, strerror(errno));
        return -3;
    }
    dbgDebug("yuvFile is [%s], open it for write succeed. Write it now.\n", yuvFilepath.c_str());

    int width = pCodecCtx->width, height = pCodecCtx->height;
    unsigned char * yBuf = pFrame->data[0], *uBuf = pFrame->data[1], *vBuf = pFrame->data[2];
    int yStride = pFrame->linesize[0], uStride = pFrame->linesize[1], vStride = pFrame->linesize[2];
    //Y firstly
    int i = 0;
    for(i = 0; i < height; i++)  //write in line
    {
        fwrite(yBuf + i * yStride, 1, width, fp);   //just write valid data to it
    }
    fflush(fp);
    dbgDebug("Write Yuv ---- Y being write.\n");
    //U secondly
    for(i = 0; i < height / 2; i++)  //write in line
    {
        fwrite(uBuf + i * uStride, 1, width / 2, fp);   //just write valid data to it
    }
    fflush(fp);
    dbgDebug("Write Yuv ---- U being write.\n");
    //V thirdly
    for(i = 0; i < height / 2; i++)  //write in line
    {
        fwrite(vBuf + i * vStride, 1, width / 2, fp);   //just write valid data to it
    }
    fflush(fp);
    dbgDebug("Write Yuv ---- V being write.\n");

    fclose(fp);
    fp = NULL;

    return 0;
}



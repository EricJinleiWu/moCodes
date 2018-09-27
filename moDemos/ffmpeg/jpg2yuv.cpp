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
}

#include "utils.h"

/*
    Save the @pFrame to yuv file;
    yuv file will be set in current directory, with name in format : timestamp_width_height.yuv;
    Currently, I think the jpg will be decode to AV_PIX_FMT_YUVJ420P, which is a yuv420p format,
    so I just deal with this format;
    return 0 if succeed, 0- if failed;
*/
static int saveFrame2YuvFile(const AVFrame * pFrame, const AVCodecContext * pCodecCtx, const char * pYuvFilepath)
{
    if(NULL == pFrame || NULL == pCodecCtx || NULL == pYuvFilepath)
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

    time_t curTimestamp = time(NULL);
    FILE * fp = NULL;
    fp = fopen(pYuvFilepath, "wb");
    if(NULL == fp)
    {
        dbgError("Open yuv file [%s] for write failed! errno=%d, desc=[%s]\n",
            pYuvFilepath, errno, strerror(errno));
        return -3;
    }
    dbgDebug("yuvFile is [%s], open it for write succeed. Write it now.\n", pYuvFilepath);

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
    dbgDebug("Y being write.\n");
    //U secondly
    for(i = 0; i < height / 2; i++)  //write in line
    {
        fwrite(uBuf + i * uStride, 1, width / 2, fp);   //just write valid data to it
    }
    fflush(fp);
    dbgDebug("U being write.\n");
    //V thirdly
    for(i = 0; i < height / 2; i++)  //write in line
    {
        fwrite(vBuf + i * vStride, 1, width / 2, fp);   //just write valid data to it
    }
    fflush(fp);
    dbgDebug("V being write.\n");

    fclose(fp);
    fp = NULL;

    return 0;
}

/*
    jpg --> yuv(420p);
*/
int jpg2yuv(const char * jpgFilepath, const char * pYuvFilepath)
{
    if(NULL == jpgFilepath || NULL == pYuvFilepath)
    {
        dbgError("Input param is NULL.\n");
        return -1;
    }
    dbgDebug("input param : jpgFilepath=[%s], pYuvFilepath=[%s]\n", jpgFilepath, pYuvFilepath);
    
    if(0 != access(jpgFilepath, 0))
    {
        dbgError("jpgfile [%s] donot exist! cannot convert to yuv.\n", jpgFilepath);
        return -2;
    }

    av_register_all();
    
    AVFormatContext * pFmtCtx = NULL;
    int ret = avformat_open_input(&pFmtCtx, jpgFilepath, NULL, NULL);
    if(ret != 0)
    {
        dbgError("avformat_open_input failed! ret=%d, jpgfilepath=[%s]\n", ret, jpgFilepath);
        return -3;
    }
    dbgDebug("avformat_open_input succeed.\n");

    ret = avformat_find_stream_info(pFmtCtx, NULL);
    if(ret != 0)
    {
        dbgError("avformat_find_stream_info failed! ret=%d\n", ret);
        avformat_close_input(&pFmtCtx);
        pFmtCtx = NULL;
        return -4;
    }
    dbgDebug("avformat_find_stream_info succeed, nb_streams=%d.\n", pFmtCtx->nb_streams);

    unsigned int i = 0;
    for(i = 0; i < pFmtCtx->nb_streams; i++)
    {
        if(pFmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            dbgDebug("Find video!\n");
            break;
        }
    }

    if(i >= pFmtCtx->nb_streams)
    {
        dbgError("i=%d, pFmtCtx->nb_streams=%d, donot find video in this file!\n", i, pFmtCtx->nb_streams);
        avformat_close_input(&pFmtCtx);
        pFmtCtx = NULL;
        return -5;
    }

    AVCodec * pCodec = avcodec_find_decoder(pFmtCtx->streams[i]->codecpar->codec_id);
    if(NULL == pCodec)
    {
        dbgError("avcodec_find_decoder failed! codec_id=%d\n", pFmtCtx->streams[i]->codecpar->codec_id);
        avformat_close_input(&pFmtCtx);
        pFmtCtx = NULL;
        return -6;
    }
    dbgDebug("avcodec_find_decoder succeed, pCodecContext->codec_id=%d\n", pFmtCtx->streams[i]->codecpar->codec_id);

    AVCodecContext * pCodecContext = NULL;
    pCodecContext = avcodec_alloc_context3(pCodec);
    if(NULL == pCodecContext)
    {
        dbgError("avcodec_alloc_context3 failed!\n");
        avformat_close_input(&pFmtCtx);
        pFmtCtx = NULL;
        return -6;
    }
    dbgDebug("avcodec_alloc_context3 succeed.\n");

    ret = avcodec_parameters_to_context(pCodecContext, pFmtCtx->streams[i]->codecpar);
    if(ret != 0)
    {
        dbgError("avcodec_parameters_to_context failed! ret=%d\n", ret);
        avcodec_free_context(&pCodecContext);
        pCodecContext = NULL;
        avformat_close_input(&pFmtCtx);
        pFmtCtx = NULL;
        return -7;
    }
    
    ret = avcodec_open2(pCodecContext, pCodec, NULL);
    if(ret != 0)
    {
        dbgError("avcodec_open2 failed, ret=%d\n", ret);
        avcodec_free_context(&pCodecContext);
        pCodecContext = NULL;
        avformat_close_input(&pFmtCtx);
        pFmtCtx = NULL;
        return -7;
    }
    dbgDebug("avcodec_open2 succeed.\n");

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
        return -8;
    }
    dbgDebug("av_read_frame succeed, packet.size=%d.\n", pPacket->size);
    
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
        return -9;
    }
    
    ret = avcodec_send_packet(pCodecContext, pPacket);
    dbgDebug("avcodec_send_packet return %d\n", ret);
    ret = avcodec_receive_frame(pCodecContext, pFrame);
    dbgDebug("avcodec_receive_frame return %d\n", ret);
    dbgDebug("pFrame->format=%d, pFrame->pict_type=%d, width=%d, height=%d, codecContext->width=%d, codecContext->height=%d, linesize=[%d, %d, %d]\n",
        pFrame->format, pFrame->pict_type, pFrame->width, pFrame->height, pCodecContext->width, pCodecContext->height,
        pFrame->linesize[0], pFrame->linesize[1], pFrame->linesize[2]);
    //write pFrame(decoded data) to yuv file now.
    ret = saveFrame2YuvFile(pFrame, pCodecContext, pYuvFilepath);
    if(ret != 0)
    {
        dbgError("saveFrame2YuvFile failed! ret=%d\n", ret);
    }
    else
    {
        dbgDebug("saveFrame2YuvFile succeed!\n");
    }
    
    //free all resources
    av_frame_free(&pFrame);
    pFrame = NULL;
    av_packet_free(&pPacket);    //av_free_packet has deprecated
    pPacket = NULL;
    avcodec_free_context(&pCodecContext);
    pCodecContext = NULL;
    avformat_close_input(&pFmtCtx);
    pFmtCtx = NULL;

    return 0;
}


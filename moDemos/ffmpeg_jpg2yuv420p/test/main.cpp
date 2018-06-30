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

using namespace std;
#include <list>
#include <map>

#define debug(format, ...) printf("[%s, %d--debug] : "format, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define warn(format, ...) printf("[%s, %d--warn] : "format, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define error(format, ...) printf("[%s, %d--error] : "format, __FUNCTION__, __LINE__, ##__VA_ARGS__)

#define MAX_FILEPATH_LEN    256

/*
    Save the @pFrame to yuv file;
    yuv file will be set in current directory, with name in format : timestamp_width_height.yuv;
    Currently, I think the jpg will be decode to AV_PIX_FMT_YUVJ420P, which is a yuv420p format,
    so I just deal with this format;
    return 0 if succeed, 0- if failed;
*/
static int saveFrame2YuvFile(const AVFrame * pFrame, const AVCodecContext * pCodecCtx)
{
    if(NULL == pFrame || NULL == pCodecCtx)
    {
        error("Input param is NULL.\n");
        return -1;
    }

    if(pFrame->format != AV_PIX_FMT_YUVJ420P)
    {
        error("pFrame->format=%d, donot AV_PIX_FMT_YUVJ420P(%d), donot deal with it now.\n",
            pFrame->format, AV_PIX_FMT_YUVJ420P);
        return -2;
    }
    debug("pFrame->format=%d, pFrame->width=%d, pFrame->height=%d\n", pFrame->format, pFrame->width, pFrame->height);

    time_t curTimestamp = time(NULL);
    char yuvFilepath[MAX_FILEPATH_LEN] = {0x00};
    memset(yuvFilepath, 0x00, MAX_FILEPATH_LEN);
    snprintf(yuvFilepath, MAX_FILEPATH_LEN, "%ld_%d_%d_420P.yuv", curTimestamp, pCodecCtx->width, pCodecCtx->height);
    yuvFilepath[MAX_FILEPATH_LEN - 1] = 0x00;
    FILE * fp = NULL;
    fp = fopen(yuvFilepath, "wb");
    if(NULL == fp)
    {
        error("Open yuv file [%s] for write failed! errno=%d, desc=[%s]\n",
            yuvFilepath, errno, strerror(errno));
        return -3;
    }
    debug("yuvFile is [%s], open it for write succeed. Write it now.\n", yuvFilepath);

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
    debug("Y being write.\n");
    //U secondly
    for(i = 0; i < height / 2; i++)  //write in line
    {
        fwrite(uBuf + i * uStride, 1, width / 2, fp);   //just write valid data to it
    }
    fflush(fp);
    debug("U being write.\n");
    //V thirdly
    for(i = 0; i < height / 2; i++)  //write in line
    {
        fwrite(vBuf + i * vStride, 1, width / 2, fp);   //just write valid data to it
    }
    fflush(fp);
    debug("V being write.\n");

    fclose(fp);
    fp = NULL;

    return 0;
}

/*
    jpg --> yuv;
*/
int main(int argc, char **argv)
{
    if(argc != 2)
    {
        fprintf(stderr, "Usage : %s jpgFilepath\n\n", argv[0]);
        return -1;
    }
    
    char jpgFilepath[MAX_FILEPATH_LEN] = {0x00};
    memset(jpgFilepath, 0x00, MAX_FILEPATH_LEN);
    strncpy(jpgFilepath, argv[1], MAX_FILEPATH_LEN);
    jpgFilepath[MAX_FILEPATH_LEN - 1] = 0x00;
    debug("Input argv[1]=[%s], Convert jpgFilepath=[%s]\n", argv[1], jpgFilepath);

    if(0 != access(jpgFilepath, 0))
    {
        error("jpgfile [%s] donot exist! cannot convert to yuv.\n", jpgFilepath);
        return -2;
    }

    av_register_all();
    
    AVFormatContext * pFmtCtx = NULL;
    int ret = avformat_open_input(&pFmtCtx, jpgFilepath, NULL, NULL);
    if(ret != 0)
    {
        error("avformat_open_input failed! ret=%d, jpgfilepath=[%s]\n", ret, jpgFilepath);
        return -3;
    }
    debug("avformat_open_input succeed.\n");

    ret = avformat_find_stream_info(pFmtCtx, NULL);
    if(ret != 0)
    {
        error("avformat_find_stream_info failed! ret=%d\n", ret);
        avformat_close_input(&pFmtCtx);
        pFmtCtx = NULL;
        return -4;
    }
    debug("avformat_find_stream_info succeed, nb_streams=%d.\n", pFmtCtx->nb_streams);

    unsigned int i = 0;
    for(i = 0; i < pFmtCtx->nb_streams; i++)
    {
        if(pFmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            debug("Find video!\n");
            break;
        }
    }

    if(i >= pFmtCtx->nb_streams)
    {
        error("i=%d, pFmtCtx->nb_streams=%d, donot find video in this file!\n", i, pFmtCtx->nb_streams);
        avformat_close_input(&pFmtCtx);
        pFmtCtx = NULL;
        return -5;
    }

    AVCodec * pCodec = avcodec_find_decoder(pFmtCtx->streams[i]->codecpar->codec_id);
    if(NULL == pCodec)
    {
        error("avcodec_find_decoder failed! codec_id=%d\n", pFmtCtx->streams[i]->codecpar->codec_id);
        avformat_close_input(&pFmtCtx);
        pFmtCtx = NULL;
        return -6;
    }
    debug("avcodec_find_decoder succeed, pCodecContext->codec_id=%d\n", pFmtCtx->streams[i]->codecpar->codec_id);

    AVCodecContext * pCodecContext = NULL;
    pCodecContext = avcodec_alloc_context3(pCodec);
    if(NULL == pCodecContext)
    {
        error("avcodec_alloc_context3 failed!\n");
        avformat_close_input(&pFmtCtx);
        pFmtCtx = NULL;
        return -6;
    }
    debug("avcodec_alloc_context3 succeed.\n");
    ret = avcodec_parameters_to_context(pCodecContext, pFmtCtx->streams[i]->codecpar);
    if(ret != 0)
    {
        error("avcodec_parameters_to_context failed! ret=%d\n", ret);
        avcodec_free_context(&pCodecContext);
        pCodecContext = NULL;
        avformat_close_input(&pFmtCtx);
        pFmtCtx = NULL;
        return -7;
    }

    ret = avcodec_open2(pCodecContext, pCodec, NULL);
    if(ret != 0)
    {
        error("avcodec_open2 failed, ret=%d\n", ret);
        avcodec_free_context(&pCodecContext);
        pCodecContext = NULL;
        avformat_close_input(&pFmtCtx);
        pFmtCtx = NULL;
        return -7;
    }
    debug("avcodec_open2 succeed.\n");

    AVPacket * pPacket = NULL;
    pPacket = av_packet_alloc();
    ret = av_read_frame(pFmtCtx, pPacket);
    if(ret != 0)
    {
        error("av_read_frame failed, ret=%d\n", ret);
        avcodec_free_context(&pCodecContext);
        pCodecContext = NULL;
        avformat_close_input(&pFmtCtx);
        pFmtCtx = NULL;
        return -8;
    }
    debug("av_read_frame succeed, packet.size=%d.\n", pPacket->size);
    
    AVFrame * pFrame = NULL;
    pFrame = av_frame_alloc();
    if(NULL == pFrame)
    {
        error("av_frame_alloc failed!\n");
        av_packet_free(&pPacket);    //av_free_packet has deprecated
        pPacket = NULL;
        avcodec_free_context(&pCodecContext);
        pCodecContext = NULL;
        avformat_close_input(&pFmtCtx);
        pFmtCtx = NULL;
        return -9;
    }

    ret = avcodec_send_packet(pCodecContext, pPacket);
    debug("avcodec_send_packet return %d\n", ret);
    ret = avcodec_receive_frame(pCodecContext, pFrame);
    debug("avcodec_receive_frame return %d\n", ret);
    debug("pFrame->format=%d, pFrame->pict_type=%d, width=%d, height=%d, codecContext->width=%d, codecContext->height=%d, linesize=[%d, %d, %d]\n",
        pFrame->format, pFrame->pict_type, pFrame->width, pFrame->height, pCodecContext->width, pCodecContext->height,
        pFrame->linesize[0], pFrame->linesize[1], pFrame->linesize[2]);
    //TODO, write pFrame(decoded data) to yuv file now.
    ret = saveFrame2YuvFile(pFrame, pCodecContext);
    if(ret != 0)
    {
        error("saveFrame2YuvFile failed! ret=%d\n", ret);
    }
    else
    {
        debug("saveFrame2YuvFile succeed!\n");
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


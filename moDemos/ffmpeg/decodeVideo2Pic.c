#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <sys/types.h>
#include <time.h>

#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"

#include "utils.h"

#include "jpeglib.h"
#include "jerror.h"

#define BUF_BLOCK_SIZE  (4096)
#define OUTPUT_FILENAME_FORMAT  "%d_w%d_h%d_s%d.%s"

static int gOutpicNo = 0;

static char * getPicTypeDesc(const OUTPUT_PICTYPE type)
{
    static char picTypeDesc[OUTPUT_PICTYPE_MAX + 1][32] = {
        "pgm", "png", "jpg", "unknown"};
    switch(type)
    {
        case OUTPUT_PICTYPE_PGM:
        case OUTPUT_PICTYPE_PNG:
        case OUTPUT_PICTYPE_JPG:
            return picTypeDesc[type];
        default:
            return picTypeDesc[OUTPUT_PICTYPE_MAX];
    }
}

static int savePgm(const char * picFilepath, AVFrame * pFrame)
{
    if(NULL == picFilepath || NULL == pFrame)
    {
        dbgError("Input param is NULL.\n");
        return -1;
    }

    //open file for write
    FILE * fp = NULL;
    fp =  fopen(picFilepath, "w");
    if(fp == NULL)
    {
        dbgError("open file [%s] for write failed! errno=%d, desc=[%s]\n",
            picFilepath, errno, strerror(errno));
        return -2;
    }
    
    fprintf(fp, "P5\n%d %d\n%d\n", pFrame->width, pFrame->height, 255);

    unsigned char *pData = pFrame->data[0];
    
    int i = 0;
    for(i = 0; i < pFrame->height; i++)
    {
        fwrite(pData + i * pFrame->linesize[0], 1, pFrame->width, fp);
        fflush(fp);
    }
    fclose(fp);
    fp = NULL;

    return 0;
}

//TODO, png donot support now.
static int savePng(const char * picFilepath, AVFrame * pFrame)
{
    dbgError("donot support png now.\n");
    return -1;
}




static int doCompressYuv420p(struct jpeg_compress_struct cinfo, unsigned char * pImageBuffer, 
    const int srcWidth, const int srcHeight, const int quality)
{
    #define PADDING_SIZE    (2 * DCTSIZE)

    //should padding data if necessary
    int padImageWidth = srcWidth;
    if(srcWidth % PADDING_SIZE != 0)    padImageWidth = (((srcWidth / PADDING_SIZE) + 1) * PADDING_SIZE);    //donot being used now. and this padding is right or not I donot assure now.
    int padImageHeight = srcHeight;
    if(srcHeight % PADDING_SIZE != 0)    padImageHeight = (((srcHeight / PADDING_SIZE) + 1) * PADDING_SIZE);
    dbgDebug("srcWidth=%d, srcHeight=%d, padImageWidth=%d, padImageHeight=%d\n", srcWidth, srcHeight, padImageWidth, padImageHeight);
    
    cinfo.in_color_space = JCS_YCbCr;
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);
    dbgDebug("set jpeg lib params over. quality=%d\n", quality);

    //to yuv420p, we should malloc some memory firstly
    JSAMPROW *rpY = NULL, *rpU = NULL, *rpV = NULL;
    rpY = (JSAMPROW * )malloc(sizeof(JSAMPROW) * padImageHeight);
    if(rpY == NULL) {dbgError("malloc failed! errno=%d, desc=[%s]\n", errno, strerror(errno)); return -1;}
    rpU = (JSAMPROW * )malloc(sizeof(JSAMPROW) * (padImageHeight / 2));
    if(rpU == NULL) {free(rpY); rpY = NULL; dbgError("malloc failed! errno=%d, desc=[%s]\n", errno, strerror(errno)); return -2;}
    rpV = (JSAMPROW * )malloc(sizeof(JSAMPROW) * (padImageHeight / 2));
    if(rpV == NULL) {free(rpY); rpY = NULL; free(rpU); rpU = NULL; dbgError("malloc failed! errno=%d, desc=[%s]\n", errno, strerror(errno)); return -3;}
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
    for(k = 0; k <= padImageHeight; k += PADDING_SIZE)
    {
        pp[0] = &rpY[k];
        pp[1] = &rpU[k / 2];
        pp[2] = &rpV[k / 2];
        jpeg_write_raw_data(&cinfo, pp, PADDING_SIZE);
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
    
    return 0;
}

/* yuv420p --> jpg; */
static int saveJpgFromYuv420p(const char * dstJpgFilepath, AVFrame * pFrame)
{
    if(dstJpgFilepath == NULL || pFrame == NULL)
    {
        dbgError("Input param is NULL.\n"); return -1;
    }

    //start libJpeg api calling
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error(&jerr);

    //1.create compress info
    jpeg_create_compress(&cinfo);

    //2.set output handler
    FILE * fpOut = NULL;
    fpOut = fopen(dstJpgFilepath, "wb");
    if(NULL == fpOut)
    {
        dbgError("open file [%s] for write failed! errno=%d, desc=[%s]\n", dstJpgFilepath, errno, strerror(errno));
        return -2;
    }
    jpeg_stdio_dest(&cinfo, fpOut);
    dbgDebug("jpeg_stdio_dest succeed.\n");
    
    //3.set params now.
    cinfo.image_height = pFrame->height;
    cinfo.image_width = pFrame->width;
    cinfo.input_components = 3;
    J_COLOR_SPACE jcs = JCS_YCbCr;

    //do compress now
    int ret = doCompressYuv420p(cinfo, pFrame->data[0], pFrame->width, pFrame->height, 95);
    if(ret != 0)
    {
        dbgError("doCompressYuv420P failed! ret=%d\n", ret);
        fclose(fpOut);  fpOut = NULL;
        return -3;
    }
    dbgDebug("doCompressYuv420p succeed.\n");

    fflush(fpOut);
    fclose(fpOut);
    fpOut = NULL;
    jpeg_destroy_compress(&cinfo);

    return 0;
}


//save jpg I use libJpeg
static int saveJpg(const char * picFilepath, AVFrame * pFrame)
{
    if(NULL == picFilepath || NULL == pFrame)
    {
        dbgError("Input param is NULL.\n");
        return -1;
    }

    FILE * fp = NULL;
    fp = fopen(picFilepath, "wb");
    if(NULL == fp)
    {
        dbgError("open file [%s] for write failed! errno=%d, desc=[%s]\n", picFilepath, errno, strerror(errno));
        return -2;
    }

    //the data we needed to do
    unsigned char * pBuffer = pFrame->data[0];
    JSAMPROW rowPtr[1];

    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, fp);
    //set our parameters to cinfo
    cinfo.image_width = pFrame->width;
    cinfo.image_height = pFrame->height;
    cinfo.input_components = 3;
//    cinfo.in_color_space = JCS_YCbCr;
    cinfo.in_color_space = JCS_RGB;
    //set default values
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, 80, TRUE);
    jpeg_start_compress(&cinfo, TRUE);
    //stride we can get from linesize
    int stride = pFrame->linesize[0];   //cinfo.image_width * 3;
    
    while(cinfo.next_scanline < pFrame->height)
    {
        rowPtr[0] = &pBuffer[cinfo.next_scanline * stride];
        (void)jpeg_write_scanlines(&cinfo, rowPtr, 1);
    }
    jpeg_finish_compress(&cinfo);
    fclose(fp);
    fp = NULL;
    jpeg_destroy_compress(&cinfo);

    return 0;
}

static int savePic(AVFrame * pFrame, const char * outPicDirPath, const OUTPUT_PICTYPE outPicType)
{
    if(NULL == pFrame || NULL == outPicDirPath)
    {
        dbgError("Input param is NULL.\n");
        return -1;
    }

    //output file name format : timestamp_w%d_h%d_s%d.pgm/png;
    char picFilename[MAX_FILEPATH_LEN] = {0x00};
    sprintf(picFilename, OUTPUT_FILENAME_FORMAT, gOutpicNo++, pFrame->width, pFrame->height, pFrame->linesize[0], getPicTypeDesc(outPicType));
    char picFilepath[MAX_FILEPATH_LEN] = {0x00};
    snprintf(picFilepath, MAX_FILEPATH_LEN, "%s/%s", outPicDirPath, picFilename);
    picFilepath[MAX_FILEPATH_LEN - 1] = 0x00;
    dbgDebug("picFilepath=[%s]\n", picFilepath);

    int ret = 0;
    switch(outPicType)
    {
        case OUTPUT_PICTYPE_PGM:
            ret = savePgm(picFilepath, pFrame);
            break;
        case OUTPUT_PICTYPE_PNG:
            ret = savePng(picFilepath, pFrame);
            break;
        case OUTPUT_PICTYPE_JPG:
            if(pFrame->format == AV_PIX_FMT_YUV420P)
                ret = saveJpgFromYuv420p(picFilepath, pFrame);
            else
                ret = saveJpg(picFilepath, pFrame);
            break;
        default:
            dbgError("Input output picture type = %d, donot support it now.\n", outPicType);
            ret = -2;
            break;
    }
    
    return ret;
}

static int decode2Pic(AVCodecContext * pCodecCtx, AVPacket * pPacket, AVFrame * pFrame, 
    const char * outPicDirPath, const OUTPUT_PICTYPE outPicType)
{
    if(NULL == pCodecCtx || NULL == pFrame || NULL == outPicDirPath)
    {
        dbgError("Input param is NULL.\n");
        return -1;
    }
    if(outPicType >= OUTPUT_PICTYPE_MAX)
    {
        dbgError("Input outPicType=%d, invalid! max value we allowed is %d\n",
            outPicType, OUTPUT_PICTYPE_MAX - 1);
        return -2;
    }

    int ret = avcodec_send_packet(pCodecCtx, pPacket);
    if(ret < 0)
    {
        dbgError("avcodec_send_packet failed! ret=%d\n", ret);
        return -3;
    }

    while(ret >= 0)
    {
        ret = avcodec_receive_frame(pCodecCtx, pFrame);
        if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            dbgDebug("avcodec_receive_frame succeed, exit now.\n");
            break;
        }
        else if(ret < 0)
        {
            dbgError("avcodec_receive_frame failed! ret=%d\n", ret);
            return -4;
        }

        ret = savePic(pFrame, outPicDirPath, outPicType);
        if(ret < 0)
        {
            dbgError("save output picture failed! ret=%d\n", ret);
            return -4;
        }
    }
    dbgDebug("decode to picture being done.\n");
    return 0;
}

int decodeVideo2Pic(const char * videoFilepath, const char * outPicDirPath, const OUTPUT_PICTYPE outPicType)
{
    if(NULL == videoFilepath || NULL == outPicDirPath)
    {
        dbgError("Input param is NULL.\n");
        return -1;
    }
    if(0 != access(videoFilepath, 0) || 0 != access(outPicDirPath, 0))
    {
        dbgError("videoFilepath=[%s], outPicDirPath=[%s], must exist!\n",
            videoFilepath, outPicDirPath);
        return -2;
    }
    if(outPicType >= OUTPUT_PICTYPE_MAX)
    {
        dbgError("outPicType=%d, cannot larger than the max value %d.\n",
            outPicType, OUTPUT_PICTYPE_MAX - 1);
        return -3;
    }
    dbgDebug("Input params : videoFilepath=[%s], outPicDirPath=[%s], outPicType=[%s]\n",
        videoFilepath, outPicDirPath, getPicTypeDesc(outPicType));

    gOutpicNo = 0;

    av_register_all();

    //avformat, to get video format infomation
    AVFormatContext * pFmtCtx = NULL;
    int ret = avformat_open_input(&pFmtCtx, videoFilepath, NULL, NULL);
    if(ret != 0)
    {
        dbgError("avformat_open_input failed! ret=%d, videoFilepath=[%s]\n",
            ret, videoFilepath);
        return -4;
    }
    dbgDebug("step1 : avformat_open_input succeed.\n");
    ret = avformat_find_stream_info(pFmtCtx, NULL);
    if(ret != 0)
    {
        dbgError("avformat_find_stream_info failed! ret=%d\n", ret);
        avformat_close_input(&pFmtCtx);
        pFmtCtx = NULL;
        return -5;
    }
    dbgDebug("step2 : avformat_find_stream_info succeed.\n");

    //avcodec, to do codec operation
    AVCodec * pVideoCodec = NULL, *pAudioCodec = NULL;
    int i = 0, videoIdx = 0;
    for(i = 0; i < pFmtCtx->nb_streams; i++)
    {
        if(pFmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videoIdx = i;
            pVideoCodec = avcodec_find_decoder(pFmtCtx->streams[i]->codecpar->codec_id);
            if(NULL == pVideoCodec)
            {
                dbgError("avcodec_find_decoder for video failed!\n");
                avformat_close_input(&pFmtCtx);
                pFmtCtx = NULL;
                return -6;
            }
            else
                dbgDebug("avcodec_find_decoder for video succeed.\n");
        }
        else if(pFmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            dbgDebug("Audio type, donot do it now.\n");
//            pAudioCodec = avcodec_find_decoder(pFmtCtx->streams[i]->codecpar->codec_id);
        }
        else
        {
            dbgError("pFmtCtx->nb_streams=%d, i=%d, codec_type=%d, donot do it now.\n",
                pFmtCtx->nb_streams, i, pFmtCtx->streams[i]->codecpar->codec_type);
        }
    }
    dbgDebug("step3 : avcodec_find_decoder succeed.\n");
    AVCodecContext * pCodecCtx = NULL;
    pCodecCtx = avcodec_alloc_context3(pVideoCodec);
    if(NULL == pCodecCtx)
    {
        dbgError("avcodec_alloc_context3 failed!\n");
        avformat_close_input(&pFmtCtx);
        pFmtCtx = NULL;
        return -7;
    }
    dbgDebug("step4 : avcodec_alloc_context3 succeed.\n");
    ret = avcodec_parameters_to_context(pCodecCtx, pFmtCtx->streams[videoIdx]->codecpar);
    if(ret != 0)
    {
        dbgError("avcodec_parameters_to_context failed! ret=%d\n", ret);
        avcodec_free_context(&pCodecCtx);
        pCodecCtx = NULL;
        avformat_close_input(&pFmtCtx);
        pFmtCtx = NULL;
        return -8;
    }
    dbgDebug("step5 : avcodec_parameters_to_context succeed.\n");
    ret = avcodec_open2(pCodecCtx, pVideoCodec, NULL);
    if(ret != 0)
    {
        dbgError("avcodec_open2 failed! ret=%d\n", ret);
        avcodec_free_context(&pCodecCtx);
        pCodecCtx = NULL;
        avformat_close_input(&pFmtCtx);
        pFmtCtx = NULL;
        return -9;
    }
    dbgDebug("step6 : avcodec_open2 succeed.\n");
    //start do decode
    AVPacket * pPacket = NULL;
    pPacket = av_packet_alloc();
    if(pPacket == NULL)
    {
        dbgError("av_packet_alloc failed!\n");
        avcodec_free_context(&pCodecCtx);
        pCodecCtx = NULL;
        avformat_close_input(&pFmtCtx);
        pFmtCtx = NULL;
        return -10;
    }
    dbgDebug("step7 : av_packet_alloc succeed.\n");
    AVFrame * pFrame = NULL;
    pFrame = av_frame_alloc();
    if(pFrame == NULL)
    {
        dbgError("av_frame_alloc failed!");
        av_packet_free(&pPacket);
        pPacket = NULL;
        avcodec_free_context(&pCodecCtx);
        pCodecCtx = NULL;
        avformat_close_input(&pFmtCtx);
        pFmtCtx = NULL;
        return -11;
    }
    dbgDebug("step8 : av_frame_alloc succeed.\n");
    AVCodecParserContext * pParserCtx = NULL;
    pParserCtx = av_parser_init(pVideoCodec->id);
    if(pParserCtx == NULL)
    {
        dbgError("av_parser_init failed!\n");
        av_frame_free(&pFrame);
        pFrame = NULL;
        av_packet_free(&pPacket);
        pPacket = NULL;
        avcodec_free_context(&pCodecCtx);
        pCodecCtx = NULL;
        avformat_close_input(&pFmtCtx);
        pFmtCtx = NULL;
        return -12;
    }
    dbgDebug("step9 : av_parser_init succeed.\n");

    //the second phase
    //open file for read
    FILE * fp = NULL;
    fp = fopen(videoFilepath, "rb");
    if(NULL == fp)
    {
        dbgError("open file [%s] for read failed! errno=%d, desc=[%s]\n",
            videoFilepath, errno, strerror(errno));
        av_parser_close(pParserCtx);
        pParserCtx = NULL;
        av_frame_free(&pFrame);
        pFrame = NULL;
        av_packet_free(&pPacket);
        pPacket = NULL;
        avcodec_free_context(&pCodecCtx);
        pCodecCtx = NULL;
        avformat_close_input(&pFmtCtx);
        pFmtCtx = NULL;
        return -13;
    }
    dbgDebug("step10 : open video file [%s] for read succeed.\n", videoFilepath);
    //need a buffer to read file
    char buf[BUF_BLOCK_SIZE + AV_INPUT_BUFFER_PADDING_SIZE] = {0x00};
    int readLen = 0;
    //read file looply in block
    while(!feof(fp))
    {
        memset(buf, 0x00, BUF_BLOCK_SIZE + AV_INPUT_BUFFER_PADDING_SIZE);
        readLen = fread(buf, 1, BUF_BLOCK_SIZE, fp);
        if(readLen == 0)
        {
            dbgError("read a block failed!\n");
            fclose(fp);
            fp = NULL;
            av_parser_close(pParserCtx);
            pParserCtx = NULL;
            av_frame_free(&pFrame);
            pFrame = NULL;
            av_packet_free(&pPacket);
            pPacket = NULL;
            avcodec_free_context(&pCodecCtx);
            pCodecCtx = NULL;
            avformat_close_input(&pFmtCtx);
            pFmtCtx = NULL;
            return -14;
        }

        char * data = buf;
        int dataLen = readLen;
        while(dataLen > 0)
        {
            //just when we find a complete frame, @pPacket->size will be set to a plus integer
            ret = av_parser_parse2(
                pParserCtx, pCodecCtx, &pPacket->data, &pPacket->size, 
                (const uint8_t *)data, dataLen, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
            if(ret < 0)
            {
                dbgError("av_parser_parse2 failed! ret=%d\n", ret);
                fclose(fp);
                fp = NULL;
                av_parser_close(pParserCtx);
                pParserCtx = NULL;
                av_frame_free(&pFrame);
                pFrame = NULL;
                av_packet_free(&pPacket);
                pPacket = NULL;
                avcodec_free_context(&pCodecCtx);
                pCodecCtx = NULL;
                avformat_close_input(&pFmtCtx);
                pFmtCtx = NULL;
                return -15;
            }

            data += ret;
            dataLen -= ret;
            if(pPacket->size > 0)
            {
                //find a valid frame, can do it now.
                ret = decode2Pic(pCodecCtx, pPacket, pFrame, outPicDirPath, outPicType);
                if(ret != 0)
                {
                    dbgError("decode2Pic failed! ret=%d, outPicDirPath=[%s], outPicType=[%s]\n",
                        ret, outPicDirPath, getPicTypeDesc(outPicType));
                    
                    fclose(fp);
                    fp = NULL;
                    av_parser_close(pParserCtx);
                    pParserCtx = NULL;
                    av_frame_free(&pFrame);
                    pFrame = NULL;
                    av_packet_free(&pPacket);
                    pPacket = NULL;
                    avcodec_free_context(&pCodecCtx);
                    pCodecCtx = NULL;
                    avformat_close_input(&pFmtCtx);
                    pFmtCtx = NULL;
                    return -16;
                }
            }
        }
    }
    
    //TODO, why should send a NULL to decode2Pic?
    ret = decode2Pic(pCodecCtx, NULL, pFrame, outPicDirPath, outPicType);
    if(ret != 0)
    {
        dbgError("decode2Pic failed! ret=%d, outPicDirPath=[%s], outPicType=[%s]\n",
            ret, outPicDirPath, getPicTypeDesc(outPicType));
        
        fclose(fp);
        fp = NULL;
        av_parser_close(pParserCtx);
        pParserCtx = NULL;
        av_frame_free(&pFrame);
        pFrame = NULL;
        av_packet_free(&pPacket);
        pPacket = NULL;
        avcodec_free_context(&pCodecCtx);
        pCodecCtx = NULL;
        avformat_close_input(&pFmtCtx);
        pFmtCtx = NULL;
        return -17;
    }

    //All has been decoded to pictures
    dbgDebug("video [%s] has been decoded to pictures, they being set to [%s], check them now.\n",
        videoFilepath, outPicDirPath);
    
    fclose(fp);
    fp = NULL;
    av_parser_close(pParserCtx);
    pParserCtx = NULL;
    av_frame_free(&pFrame);
    pFrame = NULL;
    av_packet_free(&pPacket);
    pPacket = NULL;
    avcodec_free_context(&pCodecCtx);
    pCodecCtx = NULL;
    avformat_close_input(&pFmtCtx);
    pFmtCtx = NULL;

    return 0;
}


#ifndef __COMM_MSG_H__
#define __COMM_MSG_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "moCrypt.h"

/*
    Header can help us to know this is which command;
    If cmdId donot COMMMSG_RESPCMD_SENDFRAME and COMMMSG_RESPCMD_KEYAGREE, 
        header is enough for us to know which cmd is, we donot need content;
    else, we should get content, its length is defined in "ContLen";
*/
#define COMMMSG_HEADER "<?xml version=\"1.0\" encoding=\"gb2312\"?>" \
    "<CmdId>%04d<\CmdId>" \
    "<ContLen>%08d<\ContLen>"
#define COMMMSG_HEADER_LEN  85  //The length of COMMMSG_HEADER, if use a charArray to save it, should at least has length 86 to set "\0"

#define COMMMSG_RESP_CONT_MAXLEN     65535
#define COMMMSG_RESP_FRAMEVALUE_MAXLEN    64512  //65536-1024
/*
    frameType : 1,video; 2,audio;
    frameSeq : A sequence defined by server, to sign a frame
    frameLength : the length of this frame, maybe 500K+, so we should split to blocks to transport;
    BlkIdx : The index of this block, should use this to re-make a frame data;
    CurBlkLen : the length of this block, each block has size 64512, but the last one has different value;
    Body : The body of this frame;
*/
#define COMMMSG_RESP_FRAME_FORMAT    "<FrameType>%d<\FrameType>" \
    "<FrameSeq>%d<\FrameSeq>" \
    "<FrameLen>%d<\FrameLen>" \
    "<BlkIdx>%d<\BlkIdx>" \
    "<CurBlkLen>%d<\CurBlkLen>" \
    "<body>%s<\body>"

/*
    CryptAlgo : 1,RC4; 2,DES; 3,DES3; 4, AES;
    KeyLen : the length of key;
    Key : the key;
*/
#define COMMMSG_RESP_KEYAGREE_RET_FORMAT    "<CryptAlgo>%d<\CryptAlgo>" \
    "<KeyLen>%d<\KeyLen>" \
    "<Key>%s<\Key>"

typedef struct
{
    int cmdId;
    int contLen;
    char cont[COMMMSG_RESP_CONT_MAXLEN];
}COMMMSG_RESPINFO;

typedef struct
{
    char frameType;
    int frameSeq;
    int sumLen;
    int blkIdx;
    int blkLen;
    char body[COMMMSG_RESP_FRAMEVALUE_MAXLEN];
}COMMMSG_RESP_FRAMEINFO;

/* Request from client to server */
typedef enum
{
    COMMMSG_REQCMD_COMMON = 0,
        
    COMMMSG_REQCMD_HEARTBEAT = 1,   //send a heartbeat to server is neccessary
    COMMMSG_REQCMD_STOP = 2,        //moNvrClient will stop, tell server
    COMMMSG_REQCMD_STARTPLAY = 3,   //tell server, we will play now, please send frame to me 
    COMMMSG_REQCMD_STOPPLAY = 4,    //tell server, we will stop play, you can stop sending frame to me
    COMMMSG_REQCMD_KEYAGREE = 5,    //tell server, we need a key to do crypt to frame data;

    COMMMSG_REQCMD_MAX
}COMMMSG_REQCMD;

#define COMMMSG_REQCONT_HEARTBEAT         "hello"
#define COMMMSG_REQCONT_STOP              "byebye"
#define COMMMSG_REQCONT_STARTPLAY         "startPlay"
#define COMMMSG_REQCONT_STOPPLAY          "stopPlay"
#define COMMMSG_REQCONT_KEYAGREE          "keyAgree"

/* Response get from server */
typedef enum
{
    COMMMSG_RESPCMD_COMMON = 0,
        
    COMMMSG_RESPCMD_SENDFRAME = 1,
    COMMMSG_RESPCMD_STOPSEND = 2,
    COMMMSG_RESPCMD_SERVSTOP = 3,
    COMMMSG_RESPCMD_KEYAGREE = 4,   //the response is the key

    COMMMSG_RESPCMD_MAX
}COMMMSG_RESPCMD;

#define COMMMSG_RESPCONT_SENDFRAME       "sendFrame" //server send a frame to client
#define COMMMSG_RESPCONT_STOPSEND        "stopSendFrame" //server will stop send frame, after get stop play request from client, send this 
#define COMMMSG_RESPCONT_SERVSTOP        "servStop"  //server will stop, client should connect it later   
#define COMMMSG_RESPCONT_KEYAGREE        "keyAgree"  //key agreement being done, crypt algo. and key info being send in this response;

/*
    send heartbeat to server;
*/
int commmsgSendHeartbeat(const int sockId);

/*
    before moNvrClient stop, send a request to server, to tell her we will stop;
*/
int commmsgSendStopReq(const int sockId);

int commmsgSendStartPlayReq(const int sockId);

int commmsgSendStopPlayReq(const int sockId);

/*
    Server send a response, and parse it;
    @pRespStr is the response sent from server;
    @pRespInfo is the main info of response;
    @pFrameInfo is the info of frame, if @pRespInfo->cmdId==COMMMSG_RESPCONT_SENDFRAME, @pFrameInfo is valid,
        svae the frame data; else, its invalid, will do nothing to it.
*/
int commmsgParseResp(const char * pRespStr, COMMMSG_RESPINFO * pRespInfo, COMMMSG_RESP_FRAMEINFO * pFrameInfo);


/*
    Do key agreement to server;
    send a request of keyAgree, server will send a response in cipher which crypt by public key,
    then client can do crypt with private key, get its crypt algo. and key;
    the frame body will do crypt by this;

    TODO, this just a stub;
*/
int commmsgKeyAgree(const int sockId);

/*
    Do decrypt to cipher text;
    TODO, just a stub now, we should add a parameter which include the algo. and key;
*/
int commmsgDeCrypt(char * pSrc, char * pDst);

#ifdef __cplusplus
}
#endif

#endif

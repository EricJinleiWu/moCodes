#ifndef __MO_NVR_CLIENT_H__
#define __MO_NVR_CLIENT_H__

#ifdef __cplusplus
extern "C" {
#endif

#define MO_NVR_CLIENT_LOG_MODULE    "MONVRCLIENT"

#define MONVRCLI_RET_OK             0
#define MONVRCLI_ERR_INPUT_NULL     -1
#define MONVRCLI_ERR_INIT_AGAIN     -2  //moNvrClient has been inited, cannot init again.
#define MONVRCLI_ERR_INVALID_CONFFILE   -3  //config file donot in right format
#define MONVRCLI_ERR_CONNECT_FAILED -4  //connect to moNvrServer failed
#define MONVRCLI_ERR_INTERNAL       -5  //internal error, like start thread failed, and so on
#define MONVRCLI_ERR_KEYAGREE       -6  //do key agree to server failed
#define MONVRCLI_ERR_PLAY_AGAIN     -7  //Has been play now, cannot start play again.
#define MONVRCLI_ERR_CANNOT_STOPPLAY    -8  //donot play now, cannot stop play

/*
    A function pointer
*/
typedef int (*pMoNvrCliSendFrameCallback)(const int length, const unsigned char * data);



/*
    Do init to moNvrClient;

    @pConfFilepath is the filepath of config file, moNvrServer info, own ip and port, all being set here.
    @pFunc is the callback function, when we want to play video, should use this function to send frames to player.

    return 0 if succeed, 0- if failed;
*/
int moNvrCli_init(const char *pConfFilepath, pMoNvrCliSendFrameCallback pFunc);

/*
    Do uninit to moNvrClient;
*/
void moNvrCli_unInit();

/*
    Send a request to moNvrServer to get frames, and these frames should be send to player to play;
*/
int moNvrCli_startPlay();

/*
    Send a request to moNvrServer to stop sending frames;
*/
int moNvrCli_stopPlay();

#ifdef __cplusplus
}
#endif

#endif

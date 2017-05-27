#ifndef __CFT_PROT_H__
#define __CFT_PROT_H__

/*
    The length of src filepath, 512 bytes is enough;
*/
#define SRC_FILEPATH_LEN    512

/*
    The max length of password;
*/
#define PASSWD_MAX_LEN          32

//Error no
#define CFT_PROT_OK     0
#define CFT_PROT_INPUT_INVALID  (0 - 10001) //Input param NULL, or not in right range, and so on
#define CFT_PROT_INITED_YET     (0 - 10002) //cftProt has been inited, cannot init again.

/*
    The crypt algorithms being supported currently.
*/
typedef enum
{
    MO_CFT_ALGO_BASE64,
    MO_CFT_ALGO_RC4,
    MO_CFT_ALGO_DES,    /*ECB mode used defaultly*/
    MO_CFT_ALGO_3DES
}MO_CFT_ALGO;

/*
    The structure for a request send from UI module;
*/
typedef struct
{
    unsigned char srcFilepath[SRC_FILEPATH_LEN];
    unsigned char passwd[PASSWD_MAX_LEN];
    MO_CFT_ALGO algoNo;
}CFT_REQUEST_INFO;

/*
    Do init to cftProt module, must init a queue to save all request, and a thread to deal with these requests;

    return : 
        0 : succeed;
        0-: init failed, cannot do any crypt tasks;
*/
int cftProtInit(void);

/*
    Do uninit to cftProt module, free all resources, and kill thread;
*/
void cftProtUnInit(void);

/*
    An api to UI, can add a request;
    Can add several requests at one time, @reqNum is the number of requests;
*/
int cftProtAddRequest(const CFT_REQUEST_INFO * pRequests, const unsigned int reqNum);

#endif

#ifndef __CFT_PROT_H__
#define __CFT_PROT_H__

#include "moCrypt.h"

#define CFT_PROT_CIPHER_FILE_SUFFIX  ".cftCipher"
#define CFT_PROT_CIPHER_FILE_SUFFIX_LEN 10  //strlen(CFT_PROT_CIPHER_FILE_SUFFIX)
#define CFT_PROT_PLAIN_FILE_SUFFIX  ".cftPlain"
#define CFT_PROT_PLAIN_FILE_SUFFIX_LEN 9  //strlen(CFT_PROT_PLAIN_FILE_SUFFIX)

#define CFT_PROT_SRC_FILEPATH_LEN    MOCRYPT_FILEPATH_LEN

/*
    The max length of password;
*/
#define PASSWD_MAX_LEN          32

//Error no
#define CFT_PROT_OK     0
#define CFT_PROT_INPUT_INVALID  (0 - 10001) //Input param NULL, or not in right range, and so on
#define CFT_PROT_INITED_YET     (0 - 10002) //cftProt has been inited, cannot init again.
#define CFT_PROT_MALLOC_FAILED  (0 - 10003) //malloc failed
#define CFT_PROT_CREATE_TH_FAILED   (0 - 10004) //create thread failed
#define CFT_PROT_SEM_INIT_FAILED    (0 - 10005) //sem_init failed

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
    char srcFilepath[CFT_PROT_SRC_FILEPATH_LEN];
    char passwd[PASSWD_MAX_LEN];
    MO_CFT_ALGO algoNo;
    MOCRYPT_METHOD cryptMethod; //encrypt or decrypt
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

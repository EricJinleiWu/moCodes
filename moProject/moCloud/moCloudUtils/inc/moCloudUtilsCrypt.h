#ifndef __MOCLOUD_UTILS_CRYPT_H__
#define __MOCLOUD_UTILS_CRYPT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "moCrypt.h"

/*
    Get cipher txt length;

    @algoId : which crypt algo. being used;
    @srcLen : the length of plain txt;
    @pDstLen : the length of cipher txt;

    return : 
        0 : succeed;
        0-: failed;
*/
int moCloudUtilsCrypt_getCipherTxtLen(
    const MOCLOUD_CRYPT_ALGO algoId,
    const int srcLen,
    int * pDstLen);

/*
    Do crypt;

    @method : do encrypt or decrypt;
    @cryptInfo : The crypt algo., crypt key, and so on;
    @pSrc : The content being crypt;
    @srcLen : the length of @pSrc;
    @pDst : the dst content;
    @pDstLen : the length of dst content;

    return : 
        0 : succeed;
        0-: failed;
*/
int moCloudUtilsCrypt_doCrypt(
    const MOCRYPT_METHOD method,
    const MOCLOUD_CRYPT_INFO cryptInfo,
    const char * pSrc,
    const int srcLen,
    char * pDst,
    int * pDstLen);

#ifdef __cplusplus
}
#endif

#endif
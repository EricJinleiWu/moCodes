#ifndef __MO_CRYPT_H__
#define __MO_CRYPT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "moLogger.h"

#define MOCRYPT_LOGGER_MODULE_NAME  "MOCRYPT"

/*
    Define a function pointer here.
    this is a callback function, will be used to set progress to caller.
    If care about the progress of crypting, this function should be used.
*/
typedef void (*pProgBarCallbackFunc)(int);

#define MOCRYPT_FILEPATH_LEN    256
#define MOCRYPT_KEY_MAXLEN      1024    //The maxlen of a key, currently limit to 1024, 2048 is better in future to do RSA algo.

typedef enum
{
    MOCRYPT_METHOD_ENCRYPT,
    MOCRYPT_METHOD_DECRYPT
}MOCRYPT_METHOD;



/****************************************************************************************************/
/************************************ RC4 algorithm to do crypt *************************************/
/****************************************************************************************************/

#define MOCRYPT_RC4_KEY_MAX_LEN		256		//The max key length, cannot larger than this value
#define MOCRYPT_RC4_KEY_MIN_LEN		1		//The min key length, cannot less than this value

typedef struct
{
    char pSrcFilepath[MOCRYPT_FILEPATH_LEN];
    char pDstFilepath[MOCRYPT_FILEPATH_LEN];
    unsigned char pKey[MOCRYPT_RC4_KEY_MAX_LEN];
    unsigned int keyLen;
    pProgBarCallbackFunc pCallback;
}MOCRYPT_RC4_FILEINFO;

/*
    The errno in RC4
*/
#define MOCRYPT_RC4_ERR_OK      0
#define MOCRYPT_RC4_ERR_INPUTPARAMNULL  (0 - 20000) //Input param is NULL
#define MOCRYPT_RC4_ERR_FILENOTEXIST    (0 - 20001) //Input filepath donot point to a valid file
#define MOCRYPT_RC4_ERR_MALLOCFAILED    (0 - 20002) //malloc failed
#define MOCRYPT_RC4_ERR_INVALIDKEY      (0 - 20003) //The key to do crypt is invalid, for example, length too large;
#define MOCRYPT_RC4_ERR_CREATETHREADFAIL    (0 - 20004) //pthread_create failed!
#define MOCRYPT_RC4_ERR_GETFILEDIFFSTATEFAIL    (0 - 20005) //Check srcfilepath and dstfilepath same or diff failed!
#define MOCRYPT_RC4_ERR_GENTMPFILE      (0 - 20006) //When srcfilepath same with dstfilepath, we have to gen a tmp file, this error occurred when gen tmp file failed.
#define MOCRYPT_RC4_ERR_OPENFILEFAILED  (0 - 20007) //fopen a file failed
#define MOCRYPT_RC4_ERR_WRITEFILEFAIL   (0 - 20008) //fwrite failed!

/*
    Do crypt with RC4 algorithm, include encrypt and decrypt;

    @param:
    	key : The private key of RC4;
    	txt : If do encrypt, the plain text being contained in this var, and the cipher txt being set to it, too.
    			Decrypt will convert from cipher text to plain text;

    @return:
    	0 : crypt OK, cipher txt being find in @txt;
    	0- : crypt failed;
*/
int moCrypt_RC4_cryptString(const unsigned char *key, const unsigned int keylen, unsigned char *txt, int txtLen);

/*
    Do crypt with RC4 algorithm, include encrypt and decrypt, but the text is being read from file;
    The src file can be the same with dst file;

    @param:
        pCryptInfo : The info of crypting;
        Must be careful of a param in @pCryptInfo: pProBarFunc;
            pCryptInfo->pProBarFunc : The callback function which save the progress.
                If this param is NULL, crypt will be sync, and progress cannot be get util crypt end;
                else, crypt will be async, crypt result will be get append on @pCryptInfo->pProBarFunc, when progress to 100, crypt ok;
                    if progress to a 0-, crypt failed;

    @return:
    	0 : 
    	    If NULL == @pCryptInfo->pProBarFunc, crypt OK;
    	    else, crypt will be done, and crypt result must be get from @pProBarFunc;
    	0-: 
    	    If NULL == @pCryptInfo->pProBarFunc, crypt failed;
    	    else, crypt will not be done!
*/
int moCrypt_RC4_cryptFile(const MOCRYPT_RC4_FILEINFO *pCryptInfo);




/****************************************************************************************************/
/************************************ MD5 algorithm to do hash **************************************/
/****************************************************************************************************/

typedef struct
{
    unsigned char md5[16];   //md5 value is 128bits
}MO_MD5_VALUE;


//Error when MD5
#define MOCRYPT_MD5_ERR_OK           (0)             //No error
#define MOCRYPT_MD5_ERR_INPUTNULL    (0 - 21000)    //Input pointer is NULL
#define MOCRYPT_MD5_ERR_MALLOCFAIL   (0 - 21001)    //malloc failed
#define MOCRYPT_MD5_ERR_FILENOTEXIST (0 - 21002)    //Input a filepath to get its md5 value, but this filepath donot a valid path!
#define MOCRYPT_MD5_ERR_WRONGFILEMODE (0 - 21003)   //input filepath donot a file, is a directory or any other things.
#define MOCRYPT_MD5_ERR_OPENFILEFAIL (0 - 21004)    //fopen failed!

/*
    Dump the md5 value in hex mode;
*/
void moCrypt_MD5_dump(const MO_MD5_VALUE *pValue);

/*
    Do hash with MD5 algorithm to a string;

    @param:
    	txt : The content will do md5, this must be a string, '\0' will be a valid charactor;
        pValue: The md5 value will be set to this param;

    @return:
        0 : md5 value being get succeed;
        -1 : input param is NULL, cannot get md5 or set md5 value;
*/
int moCrypt_MD5_String(const unsigned char *txt, MO_MD5_VALUE *pValue);

/*
    Do hash with MD5 algorithm to a file;
    
    @param:
    	pSrcFilepath : The filepath will do md5;
        pValue : The md5 value will be set to this param;

    @return:
        0+ : The md5 vlaue get succeed, can be get from @value;
        0- : error, maybe open file, and so on, @value is meaningless;
*/
int moCrypt_MD5_File(const char *pSrcFilepath, MO_MD5_VALUE *pValue);



/****************************************************************************************************/
/************************************ BASE64 algorithm to do crypt **********************************/
/****************************************************************************************************/

#define MOCRYPT_BASE64_ERR_OK                (0)
#define MOCRYPT_BASE64_ERR_INPUTNULL         (0 - 22000)
#define MOCRYPT_BASE64_ERR_FILEOPENFAIL      (0 - 22001)
#define MOCRYPT_BASE64_ERR_MALLOCFAILED      (0 - 22002)
#define MOCRYPT_BASE64_ERR_CREATETHREADFAIL  (0 - 22003)
#define MOCRYPT_BASE64_ERR_GETFILEDIFFSTATEFAIL (0 - 22004)
#define MOCRYPT_BASE64_ERR_OTHER             (0 - 22005)

/*
    Do encrypt or decrypt to input string @src;

    @param : 
        method : encrypt or decrypt;
        src : the pointer of charactors which should be done;
        srcLen : The bytes being crypted from @src;

    @return :
        pointer : the pointer to dst characotr array;

    Be careful, after use this function, return value is a pointer being malloced, 
    you must free it by moCrypt_BASE64_free()!!
*/
unsigned char * moCrypt_BASE64_Chars(const MOCRYPT_METHOD method, 
    const unsigned char *src, const unsigned int srcLen, unsigned int *dstLen);

/*
    Dump the chars, because we donot limit '\0', so printf("%s") cannot be used, 
    this function can help us to see infomation;
*/
void moCrypt_BASE64_dumpChars(const unsigned char *pTxt, const unsigned int len);

/*
    free the memory which being pointed by @pChars;
*/
void moCrypt_BASE64_free(unsigned char * pChars);

/*
    Do encrypt or decrypt to input file @pSrcFilepath;

    @param : 
        method : encrypt or decrypt;
        pSrcFilepath : the pointer of charactors which should be done;
        pDstFilepath : The bytes being crypted from @src;
        pFunc : The callback function pointer, which will get progress info, if NULL input, will do MD5 directly, else, will use this function to 
                    set the progress.

    @return :
        0 : succeed;
        error : failed;

    srcFilepath and dstFilepath can be same;
*/
int moCrypt_BASE64_File(const MOCRYPT_METHOD method, const char * pSrcFilepath, 
    const char * pDstFilepath, pProgBarCallbackFunc pFunc);



/****************************************************************************************************/
/************************************ BASE64 algorithm to do crypt **********************************/
/****************************************************************************************************/

/*
    The errno in DES
*/
#define MOCRYPT_DES_ERR_OK      0
#define MOCRYPT_DES_ERR_INPUTNULL   (0 - 23000) //Input param is NULL
#define MOCRYPT_DES_ERR_MALLOCFAILED    (0 - 23001) //malloc for memory failed
#define MOCRYPT_DES_ERR_INVALID_KEY (0 - 23002) //Key for DES must in 8bytes, other length cannot be used in this algorithm.
#define MOCRYPT_DES_ERR_INPUTERROR   (0 - 23003) //Input param in error, like src string has length 0, and so on


#define MOCRYPT_DES_KEYLEN  8   //Key length, in bytes

/*
    Do crypt/decrypt to @pSrc, the result set to pDst;

    params:
        method: the crypt method, crypt or decrypt;
        pSrc: the src string, cannot be NULL;
        srcLen: the length of src, should not be 0;
        pKey: the key for crypt/decrypt;
        keyLen: the length of the key;
        pDst: the memory to save result;
        pDstLen: the length of @pDst;

    return 0 if crypt/decrypt OK, 0- else;
*/
int moCrypt_DES_ECB(const MOCRYPT_METHOD method, const unsigned char * pSrc, const unsigned int srcLen, 
    const unsigned char *pKey, const unsigned int keyLen, unsigned char * pDst, unsigned int *pDstLen);

/*
    To CBC mode with DES, we need an Iv(initial vector);
    @pIv must has length 8bytes or larger, valid length is 8bytes;
    this will generated in randomizally, can assure crypt security;

    params:
        pIv: the Iv value;

    return 0 if crypt OK, 0- else;
*/
int moCrypt_DES_CBC_getIv(unsigned char *pIv);

/*
    Do crypt with DES in CBC mode;

    params:
        method: the crypt method, crypt or decrypt;
        pSrc: the src string, cannot be NULL;
        srcLen: the length of src, should not be 0;
        pKey: the key for crypt/decrypt;
        keyLen: the length of the key;
        pIv: has length with 8bytes or larger;
        pDst: the memory to save result;
        pDstLen: the length of @pDst;

    return 0 if crypt/decrypt OK, 0- else;
    
*/
int moCrypt_DES_CBC(const MOCRYPT_METHOD method, const unsigned char * pSrc, const unsigned int srcLen, 
    const unsigned char *pKey, const unsigned int keyLen, const unsigned char *pIv, 
    unsigned char * pDst, unsigned int *pDstLen);

#ifdef __cplusplus
}
#endif

#endif

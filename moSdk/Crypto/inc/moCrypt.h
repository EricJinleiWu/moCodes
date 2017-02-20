/* 
	Wujl, create at 20160614, exec the RC4 algorithm;

	V1.0.0 : just support basic crypt, a plain text can be encrypt and decrypt;

	V1.0.1 : support crypt to files;
	        can deal with strings with length larger than 1024;

	V1.0.2 : Modify a bug : when cryptFile, if srcFilepath is abstract filepath, dstFilepath is not, but they are same file in fact, cannot deal;
	         20161222, Wujl;

    V1.1.0 : Support MD5 algorithm, can get md5 value of a string;
*/

#ifndef __MO_CRYPT_H__
#define __MO_CRYPT_H__

#ifdef __cplusplus
extern "C" {
#endif

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
    	key : The private key of RC4;
    	txt : If do encrypt, the plain text being contained in this var, and the cipher txt being set to it, too.
    			Decrypt will convert from cipher text to plain text;

    @return:
    	0 : crypt OK, cipher txt being find in @txt;
    	0- : crypt failed;
*/
int moCrypt_RC4_cryptFile(const char *srcFilepath, const char *dstFilepath, const unsigned char *key, const unsigned int keylen);


typedef struct
{
    unsigned char md5[16];   //md5 value is 128bits
}MO_MD5_VALUE;


//Error when MD5
#define MOCRYPTMD5_ERR_OK           (0)             //No error
#define MOCRYPTMD5_ERR_INPUTNULL    (0x00001000)    //Input pointer is NULL
#define MOCRYPTMD5_ERR_MALLOCFAIL   (0x00001001)    //Input pointer is NULL
#define MOCRYPTMD5_ERR_FILENOTEXIST (0x00001002)    //Input a filepath to get its md5 value, but this filepath donot a valid path!
#define MOCRYPTMD5_ERR_WRONGFILEMODE (0x00001003)   //input filepath donot a file, is a directory or any other things.
#define MOCRYPTMD5_ERR_OPENFILEFAIL (0x00001004)    //fopen failed!


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
        pValue: The md5 value will be set to this param;

    @return:
        0+ : The md5 vlaue get succeed, can be get from @value;
        0- : error, maybe open file, and so on, @value is meaningless;
*/
int moCrypt_MD5_File(const char *pSrcFilepath, MO_MD5_VALUE *pValue);


#ifdef __cplusplus
}
#endif

#endif
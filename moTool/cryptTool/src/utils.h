#ifndef __UTILS_H__
#define __UTILS_H__

#ifdef __cplusplus
extern "C" {
#endif

//Support how many crypt algorithms currently;
#define SUPPORT_ALGO_NUM    1

#define ALGO_NAME_LEN       8

#define FILEPATH_LEN 128

#define RC4_MAX_KEYLEN 256

typedef enum
{
    FALSE = 0,
    TRUE = 1
}BOOL;

/* check input algo name in valid range or not */
BOOL isValidAlgo(const char *algo);

typedef enum
{
    CRYPT_METHOD_IDLE = 0,
    CRYPT_METHOD_ENCRYPT = 1,
    CRYPT_METHOD_DECRYPT = 2
}CRYPT_METHOD;

typedef enum
{
    CRYPT_ALGO_RC4 = 0,
}CRYPT_ALGO;

typedef struct
{
    CRYPT_METHOD method;
    CRYPT_ALGO algoId;
    char srcFilepath[FILEPATH_LEN];
    char dstFilepath[FILEPATH_LEN];
    char *key;
    unsigned int keyLen;
}CRYPT_INFO;


/*
    Show the algorithms we supported currently;
*/
void showSupportAlgos(void);

/*
    Check input @id is valid or not;
*/
BOOL isValidAlgoId(const int id);

/*
    Convert from id to method;
*/
char *getCryptMethodById(const CRYPT_METHOD methodId);

/*
    Check input @cryptMethod is vald or not;
*/
BOOL isValidCryptMethod(const int cryptMethod);

/*
    Check input @path is a file path or not;
    If this path donot exist, or is not a file, or @path is NULL, will return FALSE all;
*/
BOOL isFileExist(const char *path);


/* 
    1.get algo and method;
    2.use different algos to do crypt;
*/
int doCrypt(const CRYPT_INFO *pInfo);

#ifdef __cplusplus
}
#endif

#endif

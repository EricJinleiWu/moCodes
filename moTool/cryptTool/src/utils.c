
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "utils.h"
#include "moLogger.h"

/* All algo header files should be contained */
#include "moCrypt.h"

/*
All crypt algorithm we supported;
*/
static const char gSupportAlgos[SUPPORT_ALGO_NUM][ALGO_NAME_LEN] = 
        {
            "RC4"
        };
static const char gErrorAlgoName[ALGO_NAME_LEN] = "error";

static const char gCryptMethods[3][8] = {"error", "encrypt", "decrypt"};

BOOL isValidAlgo(const char *algo)
{
    if(NULL == algo)
    {
        return FALSE;
    }
    
    int ret = FALSE;
    
    int i = 0;
    for(; i < SUPPORT_ALGO_NUM; i++)
    {
        if(0 == strcmp(algo, gSupportAlgos[i]))
        {
            ret = TRUE;
            break;
        }
    }

    return ret;
}

void showSupportAlgos(void)
{
    int i = 0;
    for(i = 0; i < SUPPORT_ALGO_NUM; i++)
    {
        printf("Id=%d, Algo=[%s]\n", i, gSupportAlgos[i]);
    }
    printf("\n");
}

BOOL isValidAlgoId(const int id)
{
    return (id >= 0 && id < SUPPORT_ALGO_NUM) ? TRUE : FALSE;
}

char *getCryptMethodById(const CRYPT_METHOD methodId)
{
    switch(methodId)
    {
        case CRYPT_METHOD_IDLE:
        case CRYPT_METHOD_ENCRYPT:
        case CRYPT_METHOD_DECRYPT:
            return gCryptMethods[methodId];
        default:
            return gCryptMethods[0];
    }
}

char *getAlgoNameById(const int algoId)
{
    if(isValidAlgoId(algoId))
        return gSupportAlgos[algoId];
    else
        return gErrorAlgoName;
}


BOOL isValidCryptMethod(const int cryptMethod)
{
    BOOL ret = FALSE;
    switch(cryptMethod)
    {
        case CRYPT_METHOD_ENCRYPT:
        case CRYPT_METHOD_DECRYPT:
            ret = TRUE;
            break;
        default:
            break;
    }
    return ret;
}

BOOL isFileExist(const char *path)
{
    if(NULL == path)
    {
        return FALSE;
    }

    struct stat curStat;
    int ret = stat(path, &curStat);
    if(0 != ret)
    {
        return FALSE;
    }

    if(S_ISREG(curStat.st_mode))
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

/*
    Register a callback, when progress changed, use this function to tell out;
*/
int registerProgressFunc()
{

}

/*
    1.get algo and method;
*/
int doCrypt(const CRYPT_INFO *pInfo)
{
    int ret = 0;
    switch(pInfo->algoId)
    {
        case CRYPT_ALGO_RC4:
        {
            //To RC4, key length canbe [1, 255], If larger than this range, should modify it.
            int keylen = 0;
            char key[RC4_MAX_KEYLEN] = {0x00};
            memset(key, 0x00, RC4_MAX_KEYLEN);
            if(pInfo->keyLen >= RC4_MAX_KEYLEN)
            {
                strncpy(key, pInfo->key, RC4_MAX_KEYLEN);
                keylen = RC4_MAX_KEYLEN;
            }
            else
            {
                strncpy(key, pInfo->key, pInfo->keyLen);
                keylen = pInfo->keyLen;
            }
            
            //To RC4, encrypt and decrypt using the same function
            ret = moCrypt_RC4_cryptFile(pInfo->srcFilepath, pInfo->dstFilepath, key, keylen);
            if(0 != ret)
            {
                printf("moCrypt_RC4_cryptFile failed! ret = %d\n", ret);
            }
        }
            break;

        //Other algos should be dealed with in each condition

        default:
            break;
    }

    return ret;
}






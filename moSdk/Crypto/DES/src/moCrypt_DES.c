#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

#include "moCrypt.h"
#include "moUtils.h"
#include "moCrypt_DES.h"
#include "moLogger.h"

#define DEBUG_MODE 1

static void dumpArrayInfo(const char * pArrayName,const unsigned char * pArray,const unsigned int len)
{
#if DEBUG_MODE
    printf("Dump array [%s] info start:\n\t", pArrayName);
    unsigned int i = 0;
    for(; i < len; i++)
    {
        printf("0x%04x ", *(pArray + i));
    }
    printf("\nDump array [%s] over.\n", pArrayName);
#endif
}

/*
    When convert user key from 8bytes to [16][48bits], should do replace firstly,
    replace will convert from 64bits to 56bits, and all bits change their positions,
    the rule to change position is in this table.
    start from 1;
*/
static const unsigned char gKeyExReplaceTable[KEYEX_RE_TABLE_LEN] = {
		57,49,41,33,25,17, 9, 1,58,50,42,34,26,18,
		10, 2,59,51,43,35,27,19,11, 3,60,52,44,36,
		63,55,47,39,31,23,15, 7,62,54,46,38,30,22,
		14, 6,61,53,45,37,29,21,13, 5,28,20,12, 4
};

/*
    do replacement selection, from 64bits to 56bits;
    pKey in 8bytes, pReKey in 7bytes;
*/
static int keyExReplace(const unsigned char * pKey, unsigned char *pReKey)
{
    if(NULL == pKey || NULL == pReKey)
    {
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "Input param is NULL.\n");
        return MOCRYPT_DES_ERR_INPUTNULL;
    }

    //Set pReKey to 0 firstly
    memset(pReKey, 0x00, KEYEX_RE_KEY_LEN);
    
    //If the bit is 1, set it to pReKey; or do nothing, because it is 0 now.
    int i = 0;
    for(i = 0; i < KEYEX_RE_TABLE_LEN; i++)
    {
        //In gKeyExReplaceTable, start from 1, should decrease firstly
        int bitPos = gKeyExReplaceTable[i] - 1;

        //calc the pos in key[8] in bytes and bits, start from left to right
        int bytesPos = bitPos / 8;
        bitPos %= 8;
        if(pKey[bytesPos] & (0x1 << (8 - bitPos)))
        {
            //This bit is 1, should set it to replaceKey
            int reBytesPos = i / 8;
            int reBitPos = i % 8;
            pReKey[reBytesPos] |= (0x1 << (8 - reBitPos));
        }
        else
        {
            //do nothing, because it is 0 now.
        }
    }
    
    return 0;
}

/*
    Split the replace key to two parts;
    @pReKey, the replace key which get from keyExReplace, with length 7bytes;
    @pLeft, the left part of reKey, with length 28bytes, each byte is 0 or 1, the value of corresponding bit in @pReKey;
    @pRight, the right part of reKey, with length 28bytes, each byte is 0 or 1, the value of corresponding bit in @pReKey;
*/
static int splitReKey(const unsigned char * pReKey, unsigned char *pLeft, 
    unsigned char *pRight)
{
    if(NULL == pReKey || NULL == pLeft || NULL == pRight)
    {
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "Input param is NULL.\n");
        return MOCRYPT_DES_ERR_INPUTNULL;
    }

    //set left part and right part to 0 for initition
    memset(pLeft, 0x00, KEYEX_RE_KEY_HALF_BITSLEN);
    memset(pRight, 0x00, KEYEX_RE_KEY_HALF_BITSLEN);

    //To left firstly
    int i = 0;
    for(i = 0; i < KEYEX_RE_KEY_HALF_BITSLEN; i++)
    {
        int bytesPos = i / 8;
        int bitsPos = i % 8;
        if(pReKey[bytesPos] & (1 << (8 - bitsPos)))
        {
            pLeft[i] = 1;
        }
    }

    //To the first four elements to right part
    for(i = 0; i < 4; i++)
    {
        int bytesPos = 3;
        int bitsPos = i + 4;
        if(pReKey[bytesPos] & (1 << (8 - bitsPos)))
        {
            pRight[i] = 1;
        }
    }

    //To the left 24 bytes in right part
    for(i = 0; i < KEYEX_RE_KEY_HALF_BITSLEN - 4; i++)
    {
        int bytesPos = i / 8 + 4;
        int bitsPos = i % 8;
        if(pReKey[bytesPos] & (1 << (8 - bitsPos)))
        {
            pRight[i + 4] = 1;
        }
    }
    
    return 0;
}

/*
    Left shift an array looply;
    @pArray: the array;
    @arrLen: the length of this array;
    @shiftNum: number will be shift;
*/
static int loopLeftShiftArray(unsigned char *pArray, const unsigned int arrLen, 
    const unsigned int shiftNum)
{
    if(NULL == pArray)
    {
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "Input param is NULL.\n");
        return MOCRYPT_DES_ERR_INPUTNULL;
    }

    if(0 == shiftNum % arrLen)
    {
        moLoggerInfo(MOCRYPT_LOGGER_MODULE_NAME, "shiftNum = %d, arrayLen = %d, array will not changed after do loopShift.\n",
            shiftNum, arrLen);
        return 0;
    }

    unsigned int realShiftNum = shiftNum;
    if(shiftNum > arrLen)
    {
        realShiftNum = shiftNum % arrLen;
    }
    moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, "Input shiftNum = %d, arrLen = %d, realShiftNum = %d\n",
        shiftNum, arrLen, realShiftNum);

    //start move them 
    unsigned char *pTmp = NULL;
    pTmp = (unsigned char *)malloc(arrLen * sizeof(unsigned char ));
    if(pTmp == NULL)
    {
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "Malloc failed!");
        return MOCRYPT_DES_ERR_MALLOCFAILED;
    }
    //init it
    memset(pTmp, 0x00, arrLen);

    //Do move, firstly, pArray[realShiftNum, arrLen) --> pTmp[0, arrLen - realShiftNum]
    unsigned int i = 0;
    for(i = realShiftNum; i < arrLen; i++)
    {
        pTmp[i - realShiftNum] = pArray[i];
    }

    //secondly, pArray[0, realShiftNum) --> pTmp(arrLen - realShiftNum, arrLen)
    for(i = 0; i < realShiftNum; i++)
    {
        pTmp[arrLen - realShiftNum + i] = pArray[i];
    }

    //Copy to pArray
    memcpy(pArray, pTmp, arrLen);

    //free memory
    free(pTmp);
    pTmp = NULL;
    
    return 0;
}

/*
    Join the left part and right part to a replaceKey;
    @pLeft: the left part, length 28bytes, each bytes value is 0/1;
    @pRight: the right part, length 28bytes, each bytes value is 0/1;
    @pReKey: the replaceKey, length 7bytes(56bits);
*/
static int joinHalf2ReKey(const unsigned char * pLeft, const unsigned char * pRight, 
    unsigned char *pReKey)
{
    if(NULL == pLeft || NULL == pRight || NULL == pReKey)
    {
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "Input param is NULL.\n");
        return MOCRYPT_DES_ERR_INPUTNULL;
    }

    memset(pReKey, 0x00, KEYEX_RE_KEY_LEN);

    //set @pLeft to @pReKey firstly
    int i = 0;
    for(i = 0; i < KEYEX_RE_KEY_HALF_BITSLEN; i++)
    {
        if(pLeft[i])
        {
            int bytesPos = i / 8;
            int bitsPos = i % 8;
            pReKey[bytesPos] |= (1 << (8 - bitsPos));
        }
    }

    //set the first 4 bytes of @pRight to @pReKey[3] as the last 4 bits
    for(i = 0; i < 4; i++)
    {
        if(pRight[i])
        {
            int bytesPos = 3;
            int bitsPos = i + 4;
            pReKey[bytesPos] |= (1 << (8 - bitsPos));
        }
    }

    //set the last 24bytes of @pRight to @pReKey as the last 3bytes
    for(i = 0; i < KEYEX_RE_KEY_HALF_BITSLEN - 4; i++)
    {
        if(pRight[i + 4])
        {
            int bytesPos = i / 8 + 4;
            int bitsPos = i % 8;
            pReKey[bytesPos] |= (1 << (8 - bitsPos));
        }
    }
    
    return 0;
}

/*
    Convert from replace key to compressed key, 56bits to 48bits;
    start from 1;
*/
static const unsigned char gCompressedTable[KEYEX_ELE_LEN * 8] = 
{
    14,17,11,24, 1, 5, 3,28,15, 6,21,10,
    23,19,12, 4,26, 8,16, 7,27,20,13, 2,
    41,52,31,37,47,55,30,40,51,45,33,48,
    44,49,39,56,34,53,46,42,50,36,29,32
};

/*
    Compress key from 56bits to 48bits;
*/
static int compReKey(const unsigned char *pReKey, unsigned char * pKeyEx)
{
    if(NULL == pReKey || NULL == pKeyEx)
    {
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "Input param is NULL.\n");
        return MOCRYPT_DES_ERR_INPUTNULL;
    }

    memset(pKeyEx, 0x00, KEYEX_ELE_LEN);

    //start compress
    int i = 0;
    for(i = 0; i < KEYEX_ELE_LEN * 8; i++)
    {
        int bitsPos = gCompressedTable[i] - 1;

        int bytesPos = bitsPos / 8;
        bitsPos %= 8;

        //If this bit in replaceKey is 1, should set it to @pKeyEx
        if(pReKey[bytesPos] & (1 << (8 - bitsPos)))
        {
            int exBytesPos = i / 8;
            int exBitsPos = i % 8;
            pKeyEx[exBytesPos] |= (1 << (8 - exBitsPos));
        }
    }
    
    return 0;
}

/*
    The table which save the loop number;
*/
static const unsigned char gLoopLeftShiftTable[KEYEX_ARRAY_LEN] = 
{
		1, 1, 2, 2, 2, 2, 2, 2, 1, 2, 2, 2, 2, 2, 2, 1
};

/*
    In one loop, get a keyEx value from replace key;
    @pLeft is the left half part of replace key, with length 28;
    @pRight is the right half part of replace key, with length 28;
    @loopCnt is the index of looping;
    @pCurKeyEx is the result of this loop;

    return : 
        0 : succeed;
        0-: error;
*/
static int keyExGetKey(const unsigned char *pLeft, const unsigned char *pRight, 
    const unsigned int loopCnt, unsigned char *pCurKeyEx)
{
    if(NULL == pLeft || NULL == pRight || NULL == pCurKeyEx)
    {
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "Input param is NULL.\n");
        return MOCRYPT_DES_ERR_INPUTNULL;
    }

    if(loopCnt >= KEYEX_ARRAY_LEN)
    {
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "loopCnt = %d, invalid value!\n", loopCnt);
        return -2;
    }

    //Should copy it to another var, we will not change @pLeft and @pRight all the time;
    unsigned char leftPart[KEYEX_RE_KEY_HALF_BITSLEN] = {0x00};
    memcpy(leftPart, pLeft, KEYEX_RE_KEY_HALF_BITSLEN);
    dumpArrayInfo("leftPart", pLeft, KEYEX_RE_KEY_HALF_BITSLEN);
    unsigned char rightPart[KEYEX_RE_KEY_HALF_BITSLEN] = {0x00};
    memcpy(rightPart, pRight, KEYEX_RE_KEY_HALF_BITSLEN);
    dumpArrayInfo("rightPart", pRight, KEYEX_RE_KEY_HALF_BITSLEN);

    //Get the loopNum we needed when loopLeftShift
    int loopNum = 0;
    unsigned int i = 0;
    for(; i <= loopCnt; i++)
    {
        loopNum += gLoopLeftShiftTable[i];
    }
    moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, "loopCnt = %d, loopNum = %d\n", 
        loopCnt, loopNum);
    //do shift
    loopLeftShiftArray(leftPart, KEYEX_RE_KEY_HALF_BITSLEN, loopCnt);
    dumpArrayInfo("leftPart", leftPart, KEYEX_RE_KEY_HALF_BITSLEN);
    loopLeftShiftArray(rightPart, KEYEX_RE_KEY_HALF_BITSLEN, loopCnt);
    dumpArrayInfo("rightPart", rightPart, KEYEX_RE_KEY_HALF_BITSLEN);
    moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, "Looply left shift over.\n");

    //join this two part to a 56bits data
    unsigned char curReKey[KEYEX_RE_KEY_LEN] = {0x00};
    joinHalf2ReKey(leftPart, rightPart, curReKey);
    dumpArrayInfo("CurReKey", curReKey, KEYEX_RE_KEY_LEN);
    moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, "Join left and right to new ReKey over.\n");

    //Compress replace from 56bits to 48bits
    compReKey(curReKey, pCurKeyEx);
    dumpArrayInfo("CurKeyEx", pCurKeyEx, KEYEX_ELE_LEN);
    moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, "compress key over.\n");
    
    return 0;
}

/* 
    Do key expansion is neccessary;
    To DES, input key is an array in char with length 8, its 8bytes in length;
    When crypting, this key must be expanded to keyEx[16][48bits];

    @pKey format : unsigned char key[8];
    @pKeyEx format : unsigned char keyEx[16][6];

    return : 
        0 : ok;
        0-: failed;
*/
static int keyExpand(const unsigned char *pKey, unsigned char **pKeyEx)
{
    if(NULL == pKey || NULL == pKeyEx)
    {
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "Input param is NULL.\n");
        return MOCRYPT_DES_ERR_INPUTNULL;
    }

    //Do replace, from 64bits to 56bits
    unsigned char replaceKey[KEYEX_RE_KEY_LEN] = {0x00};
    keyExReplace(pKey, replaceKey);
    moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, "Do replacement ok.\n");
    dumpArrayInfo("OriginalKey", pKey, MOCRYPT_DES_KEYLEN);
    dumpArrayInfo("ReplaceKey", replaceKey, KEYEX_RE_KEY_LEN);

    //split the replaceKey to 2 array, each has length 28
    unsigned char leftHalf[KEYEX_RE_KEY_HALF_BITSLEN] = {0x00};
    unsigned char rightHalf[KEYEX_RE_KEY_HALF_BITSLEN] = {0x00};
    splitReKey(replaceKey, leftHalf, rightHalf);
    dumpArrayInfo("LeftHalfReKey", leftHalf, KEYEX_RE_KEY_HALF_BITSLEN);
    dumpArrayInfo("RightHalfReKey", rightHalf, KEYEX_RE_KEY_HALF_BITSLEN);

    //looply, get keyEx
    int i = 0;
    for(; i < KEYEX_ARRAY_LEN; i++)
    {
        keyExGetKey(leftHalf, rightHalf, i, pKeyEx[i]);
    }
    moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, "Expand key being done.\n");
    
    return 0;
}


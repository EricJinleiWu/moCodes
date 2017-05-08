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

#define DEBUG_MODE 0

//void dumpArrayInfo(const char * pArrayName,const unsigned char * pArray,const unsigned int len)
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


/***********************************************************************************************
                                    Key expanding in DES
                                    64bit --> [16][48bit]
 ***********************************************************************************************/

/*
    When convert user key from 8bytes to [16][48bits], should do replace firstly,
    replace will convert from 64bits to 56bits, and all bits change their positions,
    the rule to change position is in this table.
    start from 1;
*/
static const unsigned char gKeyExReplaceTable[KEYEX_RE_TABLE_LEN] = 
{
    57, 49, 41, 33, 25, 17, 9, 1,
    58, 50, 42, 34, 26, 18, 10, 2,
    59, 51, 43, 35, 27, 19, 11, 3,
    60, 52, 44, 36,
    63, 55, 47, 39, 31, 23, 15, 7,
    62, 54, 46, 38, 30, 22, 14, 6,
    61, 53, 45, 37, 29, 21, 13, 5,
    28, 20, 12, 4
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
        if(pKey[bytesPos] & (1U << (8 - bitPos - 1)))
        {
            //This bit is 1, should set it to replaceKey
            int reBytesPos = i / 8;
            int reBitPos = i % 8;
            pReKey[reBytesPos] |= (0x1 << (8 - reBitPos - 1));
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
        if(pReKey[bytesPos] & (1U << (8 - bitsPos - 1)))
        {
            pLeft[i] = 1;
        }
    }

    //To the first four elements to right part
    for(i = 0; i < 4; i++)
    {
        int bytesPos = 3;
        int bitsPos = i + 4;
        if(pReKey[bytesPos] & (1U << (8 - bitsPos - 1)))
        {
            pRight[i] = 1;
        }
    }

    //To the left 24 bytes in right part
    for(i = 0; i < KEYEX_RE_KEY_HALF_BITSLEN - 4; i++)
    {
        int bytesPos = i / 8 + 4;
        int bitsPos = i % 8;
        if(pReKey[bytesPos] & (1U << (8 - bitsPos - 1)))
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
            pReKey[bytesPos] |= (1U << (8 - bitsPos - 1));
        }
    }

    //set the first 4 bytes of @pRight to @pReKey[3] as the last 4 bits
    for(i = 0; i < 4; i++)
    {
        if(pRight[i])
        {
            int bytesPos = 3;
            int bitsPos = i + 4;
            pReKey[bytesPos] |= (1U << (8 - bitsPos - 1));
        }
    }

    //set the last 24bytes of @pRight to @pReKey as the last 3bytes
    for(i = 0; i < KEYEX_RE_KEY_HALF_BITSLEN - 4; i++)
    {
        if(pRight[i + 4])
        {
            int bytesPos = i / 8 + 4;
            int bitsPos = i % 8;
            pReKey[bytesPos] |= (1U << (8 - bitsPos - 1));
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
    14, 17, 11, 24, 1,  5,  3, 28, 
    15, 6,  21, 10, 23, 19, 12, 4, 
    26,  8, 16,  7, 27, 20, 13, 2, 
    41, 52, 31, 37, 47, 55, 30, 40, 
    51, 45, 33, 48, 44, 49, 39, 56,
    34, 53, 46, 42, 50, 36, 29, 32
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
        if(pReKey[bytesPos] & (1U << (8 - bitsPos - 1)))
        {
            int exBytesPos = i / 8;
            int exBitsPos = i % 8;
            pKeyEx[exBytesPos] |= (1U << (8 - exBitsPos - 1));
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
    loopLeftShiftArray(leftPart, KEYEX_RE_KEY_HALF_BITSLEN, loopNum);
    dumpArrayInfo("leftPart", leftPart, KEYEX_RE_KEY_HALF_BITSLEN);
    loopLeftShiftArray(rightPart, KEYEX_RE_KEY_HALF_BITSLEN, loopNum);
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
static int keyExpand(const unsigned char *pKey, unsigned char pKeyEx[][KEYEX_ELE_LEN])
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
        keyExGetKey(leftHalf, rightHalf, i, (unsigned char *)&(pKeyEx[i]));
    }
    moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, "Expand key being done.\n");
    
    return 0;
}



/***********************************************************************************************
                                    A crypt round in DES
                                    In DES, 16 rounds needed
 ***********************************************************************************************/

static const unsigned char gRoundExReplaceTable[KEYEX_ELE_LEN * 8] = 
{
    32, 1,  2,  3,  4,  5,  4,  5, 
    6,  7,  8,  9,  8,  9,  10, 11,
    12, 13, 12, 13, 14, 15, 16, 17,
    16, 17, 18, 19, 20, 21, 20, 21,
    22, 23, 24, 25, 24, 25, 26, 27,
    28, 29, 28, 29, 30, 31, 32, 1
};

/*
    Do expand replace;
    @pOrg is the original part, length 32bits(4bytes);
    @pEx is the expand part, length 48bits(6bytes);
*/
static int roundExReplace(const unsigned char *pOrg, unsigned char *pEx)
{
    if(NULL == pOrg || NULL == pEx)
    {
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "Input param is NULL.\n");
        return MOCRYPT_DES_ERR_INPUTNULL;
    }

    memset(pEx, 0x00, KEYEX_ELE_LEN);

    //Do expanding with @gRoundExReplaceTable
    int i = 0;
    for(; i < KEYEX_ELE_LEN * 8; i++)
    {
        int pos = gRoundExReplaceTable[i] - 1;

        int bytesPos = pos / 8;
        int bitsPos = pos % 8;
        if(pOrg[bytesPos] & (1U << (8 - bitsPos - 1)))
        {
            int exBytesPos = i / 8;
            int exBitsPos = i % 8;
            pEx[exBytesPos] |= (1U << (8 - exBitsPos - 1));
        }
    }
    
    return 0;
}

/*
    @pXor = @pPart1 xor @pPart2;
    xor being done in bytes;
    @pXor, @pPart, @pPart2 must have same length : @len;
*/
static int roundXor(unsigned char *pXor, const unsigned char *pPart1, 
    const unsigned char *pPart2, const unsigned int len)
{
    if(NULL == pXor || NULL == pPart1 || NULL == pPart2 || 0 == len)
    {
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "Input param is NULL.\n");
        return MOCRYPT_DES_ERR_INPUTNULL;
    }

    //Do xor to each bytes
    unsigned int i = 0;
    for(; i < len; i++)
    {
        pXor[i] = pPart1[i] ^ pPart2[i];
    }
    
    return 0;
}

/*
    Sbox table;
*/
static const unsigned char gRoundSboxTable[SBOX_SUBTABLE_NUM][SBOX_SUBTABLE_LEN] = 
{
    {//S1
        14, 4,13, 1, 2,15,11, 8, 3,10, 6,12, 5, 9, 0, 7,
         0,15, 7, 4,14, 2,13, 1,10, 6,12,11, 9, 5, 3, 8,
         4, 1,14, 8,13, 6, 2,11,15,12, 9, 7, 3,10, 5, 0,
        15,12, 8, 2, 4, 9, 1, 7, 5,11, 3,14,10, 0, 6,13
    },
    {//S2
        15, 1, 8,14, 6,11, 3, 4, 9, 7, 2,13,12, 0, 5,10,
         3,13, 4, 7,15, 2, 8,14,12, 0, 1,10, 6, 9,11, 5,
         0,14, 7,11,10, 4,13, 1, 5, 8,12, 6, 9, 3, 2,15,
        13, 8,10, 1, 3,15, 4, 2,11, 6, 7,12, 0, 5,14, 9
    },
    {//S3
        10, 0, 9,14, 6, 3,15, 5, 1,13,12, 7,11, 4, 2, 8,
        13, 7, 0, 9, 3, 4, 6,10, 2, 8, 5,14,12,11,15, 1,
        13, 6, 4, 9, 8,15, 3, 0,11, 1, 2,12, 5,10,14, 7,
         1,10,13, 0, 6, 9, 8, 7, 4,15,14, 3,11, 5, 2,12
    },
    {//S4
         7,13,14, 3, 0, 6, 9,10, 1, 2, 8, 5,11,12, 4,15,
        13, 8,11, 5, 6,15, 0, 3, 4, 7, 2,12, 1,10,14, 9,
        10, 6, 9, 0,12,11, 7,13,15, 1, 3,14, 5, 2, 8, 4,
         3,15, 0, 6,10, 1,13, 8, 9, 4, 5,11,12, 7, 2,14
    },
    {//S5
         2,12, 4, 1, 7,10,11, 6, 8, 5, 3,15,13, 0,14, 9,
        14,11, 2,12, 4, 7,13, 1, 5, 0,15,10, 3, 9, 8, 6,
         4, 2, 1,11,10,13, 7, 8,15, 9,12, 5, 6, 3, 0,14,
        11, 8,12, 7, 1,14, 2,13, 6,15, 0, 9,10, 4, 5, 3
    },
    {//S6
        12, 1,10,15, 9, 2, 6, 8, 0,13, 3, 4,14, 7, 5,11,
        10,15, 4, 2, 7,12, 0, 5, 6, 1,13,14, 0,11, 3, 8,
         9,14,15, 5, 2, 8,12, 3, 7, 0, 4,10, 1,13,11, 6,
         4, 3, 2,12, 9, 5,15,10,11,14, 1, 7, 6, 0, 8,13
    },
    {//S7
         4,11, 2,14,15, 0, 8,13, 3,12, 9, 7, 5,10, 6, 1,
        13, 0,11, 7, 4, 0, 1,10,14, 3, 5,12, 2,15, 8, 6,
         1, 4,11,13,12, 3, 7,14,10,15, 6, 8, 0, 5, 9, 2,
         6,11,13, 8, 1, 4,10, 7, 9, 5, 0,15,14, 2, 3,12
    },
    {//S8
        13, 2, 8, 4, 6,15,11, 1,10, 9, 3,14, 5, 0,12, 7,
         1,15,13, 8,10, 3, 7, 4,12, 5, 6,11, 0,14, 9, 2,
         7,11, 4, 1, 9,12,14, 2, 0, 6,10,13,15, 3, 5, 8,
         2, 1,14, 7, 4,10, 8,13,15,12, 9, 0, 3, 5, 6,11
    }
};

/*
    convert @pBytes to @pBits;
    @pBytes has length 6bytes;
    @pBits has length 48bytes, each bytes has value 0 / 1;
*/
static int roundSboxSplitKey(const unsigned char *pBytes, unsigned char *pBits)
{
    if(NULL == pBytes || NULL == pBits)
    {
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "Input param is NULL.\n");
        return MOCRYPT_DES_ERR_INPUTNULL;
    }

    //init @pBits to 0
    memset(pBits, 0x00, KEYEX_ELE_LEN * 8);

    int i = 0, j = 0;
    for(i = 0; i < KEYEX_ELE_LEN; i++)
    {
        for(j = 0; j < 8; j++)
        {
            if(pBytes[i] & (1U << (8 - j - 1)))
            {
                pBits[i * 8 + j] = 1;
            }
        }
    }

    return MOCRYPT_DES_ERR_OK;
}

/*
    Do s-box to @pOrg, result set to @pDst;
    @pOrg in length 48bits;
    @pDst in length 32bits;

    Rules of S-box:
        1.@pOrg, split to bits format;
        2.6bits as a group, do 8 rounds loop:
            2.1.To current group, get 1th bit and 6th bit, generate a integer in [0, 3], named : rowNum;
            2.2.To current group, get 2th bit to 5th bit, generate an integer in [0, F], named : columnNum;
            2.3.Append on groutId(range from 1 to 8), get subtable in @gRoundSboxTable;
            2.4.Used @rowNum and @columnNum, find a value in @gRoundSboxTable;
            2.5.This is the value we needed, set it to pDst in right position;
*/
static int roundSbox(const unsigned char *pOrg, unsigned char *pDst)
{
    if(NULL == pOrg || NULL == pDst)
    {
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "Input param is NULL.\n");
        return MOCRYPT_DES_ERR_INPUTNULL;
    }

    memset(pDst, 0x00, UNIT_HALF_LEN_BYTES);

    //Do s-box
    //1.split to an array in 48 bytes, which value all 0/1
    unsigned char pOrgBits[KEYEX_ELE_LEN * 8] = {0x00};
    memset(pOrgBits, 0x00, KEYEX_ELE_LEN * 8);
    roundSboxSplitKey(pOrg, pOrgBits);
    moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, "S-BOX: step1, split over.\n");

    //2.Do loop
    int i = 0;
    for(; i < SBOX_SUBTABLE_NUM; i++)
    {
        //2.1.Get rowNum
        unsigned char rowNum = ((pOrgBits[0 + i * KEYEX_ELE_LEN]) << 1) | ((pOrgBits[5 + i * KEYEX_ELE_LEN]) << 0);
        moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, "0th value 0x%02x, 5th value 0x%02x, rowNum = 0x%02x\n",
            pOrgBits[0 + i * KEYEX_ELE_LEN], pOrgBits[5 + i * KEYEX_ELE_LEN], rowNum);
        //2.2.Get columnNum
        unsigned char columnNum = ((pOrgBits[1 + i * KEYEX_ELE_LEN]) << 3) |
                                    ((pOrgBits[2 + i * KEYEX_ELE_LEN]) << 2) | 
                                    ((pOrgBits[3 + i * KEYEX_ELE_LEN]) << 1) | 
                                    ((pOrgBits[4 + i * KEYEX_ELE_LEN]) << 0);
        moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, "1th value 0x%02x, 2th value 0x%02x, " \
            "3th value 0x%02x, 4th value 0x%02x, columnNum = 0x%02x\n",
            pOrgBits[1 + i * KEYEX_ELE_LEN], pOrgBits[2 + i * KEYEX_ELE_LEN],
            pOrgBits[3 + i * KEYEX_ELE_LEN], pOrgBits[4 + i * KEYEX_ELE_LEN],
            columnNum);
        //2.3.Get the index of subtable in s-box table
        moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, "s-box subtable index = %d\n", i);
        //2.4.find the value in s-box table
        unsigned char value = gRoundSboxTable[i][rowNum * 16 + columnNum];
        moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, "corresponding value = %d\n", value);
        //2.5.set to @pDst
        int idx = i / 2;
        if(i % 2 == 0)
        {
            //high part in an unsigned char
            pDst[idx] |= (value << 4);
        }
        else
        {
            //low part in an unsigned char
            pDst[idx] |= value;
        }
        moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, "pDst[%d] = 0x%02x\n",
            idx, pDst[idx]);
    }
    
    return 0;
}

/*
    The table being used in P-box;
    from 32bit to 32bit, just position being changed;
*/
static const unsigned char gRoundPboxTable[UNIT_HALF_LEN_BITS] = 
{
    16, 7,20,21,29,12,28,17, 
    1, 15,23,26, 5,18,31,10,
    2, 8,24,14,32,27, 3, 9,
    19,13,30, 6,22,11, 4,25
};

/*
    Do P-Box to @pValue;
    @pValue has length 32bits;
*/
static int roundPbox(unsigned char *pValue)
{
    if(NULL == pValue)
    {
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "Input param is NULL.\n");
        return MOCRYPT_DES_ERR_INPUTNULL;
    }

    //This tmp var to save the value being changed
    unsigned char tmp[UNIT_HALF_LEN_BYTES] = {0x00};
    memset(tmp, 0x00, UNIT_HALF_LEN_BYTES);

    //Do p-box
    int i = 0;
    for(; i < UNIT_HALF_LEN_BITS; i++)
    {
        int pos = gRoundPboxTable[i] - 1;

        int bytesPos = pos / 8;
        int bitsPos = pos % 8;
        if(pValue[bytesPos] & (1U << (8 - bitsPos - 1)))
        {
            int tmpBytesPos = i / 8;
            int tmpBitsPos = i % 8;
            tmp[tmpBytesPos] |= (1 << (8 - tmpBitsPos - 1));
        }
    }

    //copy value to @pValue
    memcpy(pValue, tmp, UNIT_HALF_LEN_BYTES);
    
    return 0;
}

/*
    This function do a round of crypt in DES, rule like : 
        L(i + 1) = Ri;
        R(i + 1) = Li ^ f(Ri, Ki);
    @pLeft is the left part in this round, length is 32bits(4bytes);
    @pRight is the right part in this round, length is 32bits(4bytes);
    After this round, result will be fresh to @pLeft and @pRight;
    @pCurKeyEx is the key which get from keyExpand, in length 48bits(6bytes);

    return : 
        0 : ok;
        0-: failed;
*/
static int cryptRound(unsigned char * pLeft, unsigned char * pRight, 
    const unsigned char * pCurKeyEx)
{
    if(NULL == pLeft || NULL == pRight || NULL == pCurKeyEx)
    {
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "Input param is NULL.\n");
        return MOCRYPT_DES_ERR_INPUTNULL;
    }

    //save org @pLeft and @pRight to local
    unsigned char orgLeft[UNIT_HALF_LEN_BYTES] = {0x00};
    memcpy(orgLeft, pLeft, UNIT_HALF_LEN_BYTES);
    unsigned char orgRight[UNIT_HALF_LEN_BYTES] = {0x00};
    memcpy(orgRight, pRight, UNIT_HALF_LEN_BYTES);

    //L(i + 1) = Ri
    memcpy(pLeft, orgRight, UNIT_HALF_LEN_BYTES);
    dumpArrayInfo("pLeft", pLeft, UNIT_HALF_LEN_BYTES);

    //R(i + 1) = Li ^ f(Ri, Ki)
    dumpArrayInfo("OrgRight", orgRight, UNIT_HALF_LEN_BYTES);
    //1.expand-replace to @orgRight, from 32bits to 48bits;
    unsigned char exOrgRight[KEYEX_ELE_LEN] = {0x00};
    roundExReplace(orgRight, exOrgRight);
    moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, "In this round, step1, expand-replace over.\n");
    dumpArrayInfo("exOrgRight", exOrgRight, KEYEX_ELE_LEN);

    //2.@orgRight = @orgRight xor @pCurKeyEx
    unsigned char xorOrgRight[KEYEX_ELE_LEN] = {0x00};
    roundXor(xorOrgRight, exOrgRight, pCurKeyEx, KEYEX_ELE_LEN);
    moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, "In this round, step2, exReplacePart xor Key over.\n");
    dumpArrayInfo("xorOrgRight", xorOrgRight, KEYEX_ELE_LEN);

    //3.To @orgRight, do S-box
    unsigned char sboxRight[UNIT_HALF_LEN_BYTES] = {0x00};
    roundSbox(xorOrgRight, sboxRight);
    moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, "In this round, step3, S-Box over.\n");
    dumpArrayInfo("sboxRight", sboxRight, UNIT_HALF_LEN_BYTES);

    //4.To @orgRight, do P-box
    roundPbox(sboxRight);
    moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, "In this round, step4, P-Box over.\n");
    dumpArrayInfo("pboxRight", sboxRight, UNIT_HALF_LEN_BYTES);
    
    //5.@pRight = orgLeft xor orgRight;
    roundXor(pRight, sboxRight, orgLeft, UNIT_HALF_LEN_BYTES);
    moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, "In this round, finally, right part being get over.\n");
    dumpArrayInfo("dstRight", pRight, UNIT_HALF_LEN_BYTES);
    
    return 0;
}



/***********************************************************************************************
                                    Main Crypt in DES
                                    IP-box, crypt rounds(16times), IP-inverse-box
 ***********************************************************************************************/

static const unsigned char gIpTable[UNIT_LEN_BITS] = 
{
    58,	50,	42,	34,	26,	18,	10,	2,	60,	52,	44,	36,	28,	20,	12,	4,
    62,	54,	46,	38,	30,	22,	14,	6,	64,	56,	48,	40,	32,	24,	16,	8,
    57,	49,	41,	33,	25,	17,	9,	1,	59,	51,	43,	35,	27,	19,	11,	3,
    61,	53,	45,	37,	29,	21,	13,	5,	63,	55,	47,	39,	31,	23,	15,	7
};

/*
    Do IP-table converse to @pSrc, result save in @pDst;
    @pSrc and @pDst all have length 8bytes;
*/
static int ipConv(const unsigned char *pSrc, unsigned char *pDst)
{
    if(NULL == pSrc || NULL == pDst)
    {
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "Input param is NULL.\n");
        return MOCRYPT_DES_ERR_INPUTNULL;
    }

    memset(pDst, 0x00, UNIT_LEN_BYTES);

    int i = 0;
    for(; i < UNIT_LEN_BITS; i++)
    {    
        int pos = gIpTable[i] - 1;

        int bytesPos = pos / 8;
        int bitsPos = pos % 8;
        if(pSrc[bytesPos] & (1U << (8 - bitsPos - 1)))
        {
            int dstBytesPos = i / 8;
            int dstBitsPos = i % 8;
            pDst[dstBytesPos] |= (1U << (8 - dstBitsPos - 1));
        }
    }

    return MOCRYPT_DES_ERR_OK;
}

static const unsigned char gIpInvTable[UNIT_LEN_BITS] = 
{    
    40, 8,48,16,56,24,64,32,39, 7,47,15,55,23,63,31,
    38, 6,46,14,54,22,62,30,37, 5,45,13,53,21,61,29,
    36, 4,44,12,52,20,60,28,35, 3,43,11,51,19,59,27,
    34, 2,42,10,50,18,58,26,33, 1,41, 9,49,17,57,25
};

/*
    Do IP-inverse-table converse to @pSrc, result save in @pDst;
*/
static int ipInvConv(const unsigned char *pSrc, unsigned char *pDst)
{
    if(NULL == pSrc || NULL == pDst)
    {
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "Input param is NULL.\n");
        return MOCRYPT_DES_ERR_INPUTNULL;
    }

    memset(pDst, 0x00, UNIT_LEN_BYTES);

    int i = 0;
    for(; i < UNIT_LEN_BITS; i++)
    {    
        int pos = gIpInvTable[i] - 1;

        int bytesPos = pos / 8;
        int bitsPos = pos % 8;
        if(pSrc[bytesPos] & (1U << (8 - bitsPos - 1)))
        {
            int dstBytesPos = i / 8;
            int dstBitsPos = i % 8;
            pDst[dstBytesPos] |= (1U << (8 - dstBitsPos - 1));
        }
    }

    return MOCRYPT_DES_ERR_OK;
}

/*
    Split @pSrc to 2 parts : @pLeftHalf, @pRightHalf;
    @pSrc is a unit, has length 8bytes;
    @pLeftHalf and @pRightHalf, has length 4bytes;
*/
static int splitUnit2Half(const unsigned char *pSrc, unsigned char *pLeftHalf, 
    unsigned char *pRightHalf)
{
    if(NULL == pSrc || NULL == pLeftHalf || NULL == pRightHalf)
    {
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "Input param is NULL.\n");
        return MOCRYPT_DES_ERR_INPUTNULL;
    }

    int i = 0;
    for(i = 0; i < UNIT_HALF_LEN_BYTES; i++)
    {
        pLeftHalf[i] = pSrc[i];
    }
    for(i = 0; i < UNIT_HALF_LEN_BYTES; i++)
    {
        pRightHalf[i] = pSrc[i + UNIT_HALF_LEN_BYTES];
    }

    return MOCRYPT_DES_ERR_OK;
}

/*
    Join @pLeftHalf and @pRightHalf to @pDst;
*/
static int joinHalf2Unit(const unsigned char *pLeftHalf, const unsigned char *pRightHalf, 
    unsigned char *pDst)
{
    if(NULL == pLeftHalf || NULL == pRightHalf || NULL == pDst)
    {
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "Input param is NULL.\n");
        return MOCRYPT_DES_ERR_INPUTNULL;
    }

    int i = 0;
    for(i = 0; i < UNIT_HALF_LEN_BYTES; i++)
    {
        pDst[i] = pLeftHalf[i];
    }
    for(i = 0; i < UNIT_HALF_LEN_BYTES; i++)
    {
        pDst[i + UNIT_HALF_LEN_BYTES] = pRightHalf[i];
    }

    return MOCRYPT_DES_ERR_OK;
}

/*
    do encrypt to a unit which has length 8bytes;

    rule:
        0.Do IP-table converse to @pSrcUnit;
        1.split @pSrcUnit to left and right with length 32bits(4bytes);
        2.Looply do cryptRound in 16times;
        3.join left and right to 64bits;
        4.Do IP-inverse-table converse;
        5.set result to @pDstUnit;
*/
static int unitEncrypt(const unsigned char *pSrcUnit, unsigned char keyEx[][KEYEX_ELE_LEN],
    unsigned char *pDstUnit)
{
    if(NULL == pSrcUnit || NULL == keyEx || NULL == pDstUnit)
    {
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "Input param is NULL.\n");
        return MOCRYPT_DES_ERR_INPUTNULL;
    }

    //IP-table converse
    unsigned char tmp[UNIT_LEN_BYTES] = {0x00};
    memset(tmp, 0x00, UNIT_LEN_BYTES);
    int ret = ipConv(pSrcUnit, tmp);
    if(ret != MOCRYPT_DES_ERR_OK)
    {
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "ipConv failed! ret = %d\n", ret);
        return ret;
    }
    moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, "ipConv succeed.\n");
    dumpArrayInfo("ipConvSrc", tmp, UNIT_LEN_BYTES);

    //split the tmp to left and right half
    unsigned char leftHalf[UNIT_HALF_LEN_BYTES] = {0x00};
    memset(leftHalf, 0x00, UNIT_HALF_LEN_BYTES);
    unsigned char rightHalf[UNIT_HALF_LEN_BYTES] = {0x00};
    memset(rightHalf, 0x00, UNIT_HALF_LEN_BYTES);
    ret = splitUnit2Half(tmp, leftHalf, rightHalf);
    if(ret != MOCRYPT_DES_ERR_OK)
    {
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "splitUnit2Half failed! ret = %d\n", ret);
        return ret;
    }
    moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, "splitUnit2Half succeed.\n");

    //Do cryptRound looply
    int loopCnt = 0;
    for(; loopCnt < UNIT_LOOP_CNT; loopCnt++)
    {
        ret = cryptRound(leftHalf, rightHalf, keyEx[loopCnt]);
        if(ret != MOCRYPT_DES_ERR_OK)
        {
            moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "cryptRound failed! loopCnt = %d, ret = %d\n",
                loopCnt, ret);
            return ret;
        }
    }
    moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, "cryptRound over.\n");

    //Should convert left to right, right to left!
    unsigned char tmpHalf[UNIT_HALF_LEN_BYTES] = {0x00};
    memcpy(tmpHalf, leftHalf, UNIT_HALF_LEN_BYTES);
    memcpy(leftHalf, rightHalf, UNIT_HALF_LEN_BYTES);
    memcpy(rightHalf, tmpHalf, UNIT_HALF_LEN_BYTES);

    //join this two parts to a unit
    ret = joinHalf2Unit(leftHalf, rightHalf, tmp);
    if(ret != MOCRYPT_DES_ERR_OK)
    {
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "joinHalf2Unit failed! ret = %d\n", ret);
        return ret;
    }
    moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, "joinHalf2Unit succeed.\n");
    dumpArrayInfo("joinedUnit", tmp, UNIT_LEN_BYTES);

    //Do IP-inverse-table converse
    ret = ipInvConv(tmp, pDstUnit);
    if(ret != MOCRYPT_DES_ERR_OK)
    {
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "ipInvConv failed! ret = %d\n", ret);
        return ret;
    }
    moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, "ipInvConv succeed.\n");
    dumpArrayInfo("dstUnit", pDstUnit, UNIT_LEN_BYTES);
    
    return MOCRYPT_DES_ERR_OK;
}

/*
    Do encrypt to @pSrc;
    @pKey has been checked, length is 8bytes;
*/
static int enCrypt(const unsigned char *pSrc, const unsigned int srcLen, 
    const unsigned char *pKey, unsigned char *pDst)
{
    //Do key-expanding firstly.
    unsigned char keyEx[KEYEX_ARRAY_LEN][KEYEX_ELE_LEN];
    memset(keyEx, 0x00, KEYEX_ARRAY_LEN * KEYEX_ELE_LEN);
    int ret = keyExpand(pKey, keyEx);
    if(ret != MOCRYPT_DES_ERR_OK)
    {
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "keyExpand failed! ret = %d\n", ret);
        return ret;
    }
    moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, "keyExpand succeed.\n");
    
    //Do crypt in a unit looply, a unit has length 8bytes
    unsigned int cnt = 0;
    while(cnt < srcLen / UNIT_LEN_BYTES)
    {
        //do crypt to this unit(length is 8bytes)
        ret = unitEncrypt(pSrc + cnt * UNIT_LEN_BYTES, keyEx, pDst + cnt * UNIT_LEN_BYTES);
        if(ret != MOCRYPT_DES_ERR_OK)
        {
            moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "unitEncrypt failed! cnt = %d, ret = %d\n",
                cnt, ret);
            break;
        }

        //to next unit
        cnt++;
    }
    moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, "enCrypt being done, ret = %d\n", ret);

    return ret;
}

/*
    do decrypt to a unit which has length 8bytes;

    rule:
        0.Do IP-table converse to @pSrcUnit;
        1.split @pSrcUnit to left and right with length 32bits(4bytes);
        2.Looply do cryptRound in 16times;
        3.join left and right to 64bits;
        4.Do IP-inverse-table converse;
        5.set result to @pDstUnit;
*/
static int unitDecrypt(const unsigned char *pSrcUnit, unsigned char keyEx[][KEYEX_ELE_LEN],
    unsigned char *pDstUnit)
{
    if(NULL == pSrcUnit || NULL == keyEx || NULL == pDstUnit)
    {
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "Input param is NULL.\n");
        return MOCRYPT_DES_ERR_INPUTNULL;
    }

    //IP-table converse
    unsigned char tmp[UNIT_LEN_BYTES] = {0x00};
    memset(tmp, 0x00, UNIT_LEN_BYTES);
    int ret = ipConv(pSrcUnit, tmp);
    if(ret != MOCRYPT_DES_ERR_OK)
    {
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "ipConv failed! ret = %d\n", ret);
        return ret;
    }
    moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, "ipConv succeed.\n");

    //split the tmp to left and right half
    unsigned char leftHalf[UNIT_HALF_LEN_BYTES] = {0x00};
    memset(leftHalf, 0x00, UNIT_HALF_LEN_BYTES);
    unsigned char rightHalf[UNIT_HALF_LEN_BYTES] = {0x00};
    memset(rightHalf, 0x00, UNIT_HALF_LEN_BYTES);
    ret = splitUnit2Half(tmp, leftHalf, rightHalf);
    if(ret != MOCRYPT_DES_ERR_OK)
    {
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "splitUnit2Half failed! ret = %d\n", ret);
        return ret;
    }
    moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, "splitUnit2Half succeed.\n");

    //Do cryptRound looply, this is the important point different with encrypt!
    int loopCnt = 0;
    for(; loopCnt < UNIT_LOOP_CNT; loopCnt++)
    {
//        ret = cryptRound(rightHalf, leftHalf, keyEx[UNIT_LOOP_CNT - loopCnt - 1]);
        ret = cryptRound(leftHalf, rightHalf, keyEx[UNIT_LOOP_CNT - loopCnt - 1]);
        if(ret != MOCRYPT_DES_ERR_OK)
        {
            moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "cryptRound failed! loopCnt = %d, ret = %d\n",
                loopCnt, ret);
            return ret;
        }
    }
    moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, "cryptRound over.\n");

    //Should convert left to right, right to left!
    unsigned char tmpHalf[UNIT_HALF_LEN_BYTES] = {0x00};
    memcpy(tmpHalf, leftHalf, UNIT_HALF_LEN_BYTES);
    memcpy(leftHalf, rightHalf, UNIT_HALF_LEN_BYTES);
    memcpy(rightHalf, tmpHalf, UNIT_HALF_LEN_BYTES);

    //join this two parts to a unit
    ret = joinHalf2Unit(leftHalf, rightHalf, tmp);
    if(ret != MOCRYPT_DES_ERR_OK)
    {
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "joinHalf2Unit failed! ret = %d\n", ret);
        return ret;
    }
    moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, "joinHalf2Unit succeed.\n");

    //Do IP-inverse-table converse
    ret = ipInvConv(tmp, pDstUnit);
    if(ret != MOCRYPT_DES_ERR_OK)
    {
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "ipInvConv failed! ret = %d\n", ret);
        return ret;
    }
    moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, "ipInvConv succeed.\n");
    
    return MOCRYPT_DES_ERR_OK;
}


/*
    Do decrypt to @pSrc;
    @pKey has been checked, length is 8bytes;
*/
static int deCrypt(const unsigned char *pSrc, const unsigned int srcLen, 
    const unsigned char *pKey, unsigned char *pDst)
{
    //Do key-expanding firstly.
    unsigned char keyEx[KEYEX_ARRAY_LEN][KEYEX_ELE_LEN];
    memset(keyEx, 0x00, KEYEX_ARRAY_LEN * KEYEX_ELE_LEN);
    int ret = keyExpand(pKey, keyEx);
    if(ret != MOCRYPT_DES_ERR_OK)
    {
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "keyExpand failed! ret = %d\n", ret);
        return ret;
    }
    moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, "keyExpand succeed.\n");
    
    //Do crypt in a unit looply, a unit has length 8bytes
    unsigned int cnt = 0;
    while(cnt < srcLen / UNIT_LEN_BYTES)
    {
        //do crypt to this unit(length is 8bytes)
        ret = unitDecrypt(pSrc + cnt * UNIT_LEN_BYTES, keyEx, pDst + cnt * UNIT_LEN_BYTES);
        if(ret != MOCRYPT_DES_ERR_OK)
        {
            moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "unitDecrypt failed! cnt = %d, ret = %d\n",
                cnt, ret);
            break;
        }

        //to next unit
        cnt++;
    }
    moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, "deCrypt being done, ret = %d\n", ret);

    return ret;

}

/*
    Do crypt to @pSrc, cipherTxt/plainTxt set to @pDst;
    If @srcLen%8 != 0, error, cannot do DES;
    If @keyLen != 8, error, cannot do DES;

    rules:
        1.do IP-table converse;
        2.Looply do crypt to units which has length with 8bytes;
        3.do IP-inverse-table converse;
*/
int moCrypt_DES_ECB(const MOCRYPT_METHOD method, const unsigned char * pSrc, const unsigned int srcLen, 
     const unsigned char *pKey, const unsigned int keyLen, unsigned char * pDst, unsigned int *pDstLen)
{
    if(NULL == pSrc || NULL == pKey || NULL == pDst || NULL == pDstLen)
    {
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "Input param is NULL.\n");
        return MOCRYPT_DES_ERR_INPUTNULL;
    }

    //Check param validition
    if(0 == srcLen)
    {
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "Input srcLen is 0, cannot do crypt to it!\n");
        return MOCRYPT_DES_ERR_INPUTERROR;
    }
    if(srcLen % UNIT_LEN_BYTES != 0)
    {
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "Input srcLen = %d, cannot divide 8bytes, in DES," \
            "We need srcLen %% 8 == 0!\n", srcLen);
    }

    //in DES, allow @pKey donot have length 8bytes, if less, add 0 at the end; if more, just use 8bytes;
    unsigned char key[MOCRYPT_DES_KEYLEN] = {0x00};
    if(MOCRYPT_DES_KEYLEN < keyLen)
    {
        moLoggerWarn(MOCRYPT_LOGGER_MODULE_NAME, "KeyLen=%d, MOCRYPT_DES_KEYLEN=%d, should use the first 8bytes as a valid key.\n",
            keyLen, MOCRYPT_DES_KEYLEN);
        memcpy(key, pKey, MOCRYPT_DES_KEYLEN);
    }
    else if(MOCRYPT_DES_KEYLEN > keyLen)
    {
        moLoggerWarn(MOCRYPT_LOGGER_MODULE_NAME, "KeyLen=%d, MOCRYPT_DES_KEYLEN=%d, will add 0 at the end to be a valid key.\n",
            keyLen, MOCRYPT_DES_KEYLEN);
        memset(key, 0x00, MOCRYPT_DES_KEYLEN);
        memcpy(key, pKey, keyLen);
    }
    else
    {
        memcpy(key, pKey, MOCRYPT_DES_KEYLEN);
    }

    //Do crypt append on crypt/decrypt method
    int ret = 0;
    switch(method)
    {
        case MOCRYPT_METHOD_ENCRYPT:
            ret = enCrypt(pSrc, srcLen, pKey, pDst);
            break;
        case MOCRYPT_METHOD_DECRYPT:
            ret = deCrypt(pSrc, srcLen, pKey, pDst);
            break;
        default:
            moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "Input method = %d, invalid value!\n",
                method);
            ret = MOCRYPT_DES_ERR_INPUTERROR;
            break;
    }

    if(ret != MOCRYPT_DES_ERR_OK)
    {
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "Do cyrpt in DES failed! method = %d, ret = %d\n",
            method, ret);
    }
    else
    {
        //@pDstLen is the same with srcLen all the time in DES
        *pDstLen = srcLen;
    }

    return ret;
}



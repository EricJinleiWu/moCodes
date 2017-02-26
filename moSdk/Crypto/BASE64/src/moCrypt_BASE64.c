#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "moCrypt.h"

#define ENCRYPT_BLOCK_SIZE      3               //In base64, 3bytes is a block when encrypt, 4bytes is a block when decrypt
#define DECRYPT_BLOCK_SIZE      4               //In base64, 3bytes is a block when encrypt, 4bytes is a block when decrypt
#define TEMP_FILE_PREFIX		"moBase64Temp_"
#define PADDING_SYMB            '='

#if 1
#define error(format, ...) printf("MO_CRYPT_BASE64 : [%s, %s, %d ERR] : "format, __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define debug(format, ...) printf("MO_CRYPT_BASE64 : [%s, %s, %d DBG] : "format, __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define error(format, ...)
#define debug(format, ...)
#endif

static const unsigned char gMap[64] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 
    'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 
    'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 
    'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 
    'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 
    'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 
    'w', 'x', 'y', 'z', '0', '1', '2', '3', 
    '4', '5', '6', '7', '8', '9', '+', '/' 
};

/*
    convert from number to base64 symbol using gMap;
*/
static unsigned char convertFromNumToBase64Symbol(unsigned char num)
{
    return gMap[num];
}

static unsigned char convertFromBase64SymbolTonum(unsigned char num)
{
    if(num >= 'A' && num <= 'Z')
    {
        return (num - 'A');
    }
    else if(num >= 'a' && num <= 'z')
    {
        return (num - 'a' + 26);
    }
    else if(num >= '0' && num <= '9')
    {
        return (num - '0' + 52);
    }
    else if(num == '+')
    {
        return 62;
    }
    else
    {
        return 63;
    }
}

/*
    Do encrypt to @src
*/
static unsigned char * encryptChars(const unsigned char *src, const unsigned int srcLen, unsigned int *pDstLen)
{
    if(NULL == src || NULL == pDstLen)
    {
        error("Input param is NULL.\n");
        return NULL;
    }
    
    //complete block num, each block has 3 bytes
    unsigned int completeBlockNum = srcLen / ENCRYPT_BLOCK_SIZE;
    unsigned int lastBlockBytes = srcLen % ENCRYPT_BLOCK_SIZE;
    unsigned int allBlockNum = completeBlockNum;
    if(lastBlockBytes != 0)
        allBlockNum++;
    
    unsigned char *pDst = NULL;
    pDst = (unsigned char *)malloc(sizeof(unsigned char) * allBlockNum * DECRYPT_BLOCK_SIZE);
    if(NULL == pDst)
    {
        error("Malloc for cipher text failed! dst length = %d\n", allBlockNum * DECRYPT_BLOCK_SIZE);
        return NULL;
    }

    //Deal with all complete blocks firstly, they all in same rule.
    unsigned int curBlk = 0;
    for(curBlk = 0; curBlk < completeBlockNum; curBlk++)
    {
        *(pDst + curBlk * DECRYPT_BLOCK_SIZE + 0) = 
            ((*(src + curBlk * ENCRYPT_BLOCK_SIZE + 0)) & 0xfc) >> 2;
        *(pDst + curBlk * DECRYPT_BLOCK_SIZE + 0) = 
            convertFromNumToBase64Symbol(*(pDst + curBlk * DECRYPT_BLOCK_SIZE + 0));
        
        *(pDst + curBlk * DECRYPT_BLOCK_SIZE + 1) = 
            (((*(src + curBlk * ENCRYPT_BLOCK_SIZE + 0)) & 0x03) << 4) | 
            (((*(src + curBlk * ENCRYPT_BLOCK_SIZE + 1)) & 0xf0) >> 4);
        *(pDst + curBlk * DECRYPT_BLOCK_SIZE + 1) = 
            convertFromNumToBase64Symbol(*(pDst + curBlk * DECRYPT_BLOCK_SIZE + 1));
        
        *(pDst + curBlk * DECRYPT_BLOCK_SIZE + 2) = 
            (((*(src + curBlk * ENCRYPT_BLOCK_SIZE + 1)) & 0x0f) << 2) | 
            (((*(src + curBlk * ENCRYPT_BLOCK_SIZE + 2)) & 0xc0) >> 6);
        *(pDst + curBlk * DECRYPT_BLOCK_SIZE + 2) = 
            convertFromNumToBase64Symbol(*(pDst + curBlk * DECRYPT_BLOCK_SIZE + 2));
        
        *(pDst + curBlk * DECRYPT_BLOCK_SIZE + 3) = 
            ((*(src + curBlk * ENCRYPT_BLOCK_SIZE + 2)) & 0x3f);
        *(pDst + curBlk * DECRYPT_BLOCK_SIZE + 3) = 
            convertFromNumToBase64Symbol(*(pDst + curBlk * DECRYPT_BLOCK_SIZE + 3));
    }

    //The last block will be done now.
    switch(lastBlockBytes)
    {
        case 1:
            *(pDst + completeBlockNum * DECRYPT_BLOCK_SIZE + 0) = 
                ((*(src + completeBlockNum * ENCRYPT_BLOCK_SIZE + 0)) & 0xfc) >> 2;
            *(pDst + curBlk * DECRYPT_BLOCK_SIZE + 0) = 
                convertFromNumToBase64Symbol(*(pDst + curBlk * DECRYPT_BLOCK_SIZE + 0));
        
            *(pDst + completeBlockNum * DECRYPT_BLOCK_SIZE + 1) = 
                ((*(src + completeBlockNum * ENCRYPT_BLOCK_SIZE + 0)) & 0x03) << 4;
            *(pDst + curBlk * DECRYPT_BLOCK_SIZE + 1) = 
                convertFromNumToBase64Symbol(*(pDst + curBlk * DECRYPT_BLOCK_SIZE + 1));
            
            *(pDst + completeBlockNum * DECRYPT_BLOCK_SIZE + 2) = PADDING_SYMB;
            *(pDst + completeBlockNum * DECRYPT_BLOCK_SIZE + 3) = PADDING_SYMB;
            break;
        case 2:
            *(pDst + completeBlockNum * DECRYPT_BLOCK_SIZE + 0) = 
                ((*(src + completeBlockNum * ENCRYPT_BLOCK_SIZE + 0)) & 0xfc) >> 2;
            *(pDst + curBlk * DECRYPT_BLOCK_SIZE + 0) = 
                convertFromNumToBase64Symbol(*(pDst + curBlk * DECRYPT_BLOCK_SIZE + 0));
            
            *(pDst + completeBlockNum * DECRYPT_BLOCK_SIZE + 1) = 
                (((*(src + completeBlockNum * ENCRYPT_BLOCK_SIZE + 0)) & 0x03) << 4) | 
                (((*(src + completeBlockNum * ENCRYPT_BLOCK_SIZE + 1)) & 0xf0) >> 4);
            *(pDst + curBlk * DECRYPT_BLOCK_SIZE + 1) = 
                convertFromNumToBase64Symbol(*(pDst + curBlk * DECRYPT_BLOCK_SIZE + 1));
            
            *(pDst + completeBlockNum * DECRYPT_BLOCK_SIZE + 2) = 
                (((*(src + completeBlockNum * ENCRYPT_BLOCK_SIZE + 1)) & 0x0f) << 2);
            *(pDst + curBlk * DECRYPT_BLOCK_SIZE + 2) = 
                convertFromNumToBase64Symbol(*(pDst + curBlk * DECRYPT_BLOCK_SIZE + 2));
            
            *(pDst + completeBlockNum * DECRYPT_BLOCK_SIZE + 3) = PADDING_SYMB;
            break;
        default:
            break;
    }

    *pDstLen = allBlockNum * DECRYPT_BLOCK_SIZE;
    return pDst;
}

/*
    Do decrypt to @src
*/
static unsigned char * decryptChars(const unsigned char *src, const unsigned int srcLen, unsigned int *pDstLen)
{
    if(NULL == src || NULL == pDstLen)
    {
        error("Input param is NULL.\n");
        return NULL;
    }

    //Get the number of '=' at the last, this can help us to assure how many bytes in the last block in src.
    unsigned int padSymbNum = 0;
    int i = 0;
    for(i = 0; i < 2; i++)
    {
        if(*(src + srcLen - 1 - i) == PADDING_SYMB)
            padSymbNum++;
        else
            break;
    }

    //calc the pDstLen append on padSymbNum
    int blockNum = srcLen / DECRYPT_BLOCK_SIZE;
    int completeBlockNum = blockNum;
    int lastBlockBytes = 0;
    switch(padSymbNum)
    {
        case 1:
            completeBlockNum--;
            lastBlockBytes += 2;
            break;
        case 2:
            completeBlockNum--;
            lastBlockBytes++;
            break;
        default:
            break;
    }

    //get memory for pDst
    unsigned char *pDst = NULL;
    pDst = (unsigned char *)malloc(sizeof(unsigned char ) * (completeBlockNum * ENCRYPT_BLOCK_SIZE + lastBlockBytes));
    if(NULL == pDst)
    {
        error("malloc failed! errno = %d, desc = [%s], size = [%d]\n", errno, strerror(errno), 
            completeBlockNum * ENCRYPT_BLOCK_SIZE + lastBlockBytes);
        return NULL;
    }

    //complete block being done firstly
    int blkCnt = 0;
    for(blkCnt = 0; blkCnt < completeBlockNum; blkCnt++)
    {
        *(pDst + blkCnt * ENCRYPT_BLOCK_SIZE + 0) = 
            (((*(src + blkCnt * DECRYPT_BLOCK_SIZE + 0)) & 0x3f) << 2) | 
            (((*(src + blkCnt * DECRYPT_BLOCK_SIZE + 1)) & 0x30) >> 4);
        *(pDst + blkCnt * ENCRYPT_BLOCK_SIZE + 0) = 
            convertFromBase64SymbolTonum(*(pDst + blkCnt * ENCRYPT_BLOCK_SIZE + 0));

        *(pDst + blkCnt * ENCRYPT_BLOCK_SIZE + 1) = 
            (((*(src + blkCnt * DECRYPT_BLOCK_SIZE + 1)) & 0x0f) << 4) | 
            (((*(src + blkCnt * DECRYPT_BLOCK_SIZE + 2)) & 0x3c) >> 2);
        *(pDst + blkCnt * ENCRYPT_BLOCK_SIZE + 1) = 
            convertFromBase64SymbolTonum(*(pDst + blkCnt * ENCRYPT_BLOCK_SIZE + 1));
        
        *(pDst + blkCnt * ENCRYPT_BLOCK_SIZE + 2) = 
            (((*(src + blkCnt * DECRYPT_BLOCK_SIZE + 2)) & 0x03) << 6) | 
            (((*(src + blkCnt * DECRYPT_BLOCK_SIZE + 3)) & 0x3f) >> 0);
        *(pDst + blkCnt * ENCRYPT_BLOCK_SIZE + 2) = 
            convertFromBase64SymbolTonum(*(pDst + blkCnt * ENCRYPT_BLOCK_SIZE + 2));
    }

    //the last block being done, '='is padding symbol, number is 1 or 2;
    switch(lastBlockBytes)
    {
        case 1:
            *(pDst + completeBlockNum * ENCRYPT_BLOCK_SIZE) = 
                (((*(src + completeBlockNum * DECRYPT_BLOCK_SIZE + 0)) & 0x3f) << 2) | 
                (((*(src + completeBlockNum * DECRYPT_BLOCK_SIZE + 1)) & 0x30) >> 4);
            *(pDst + completeBlockNum * ENCRYPT_BLOCK_SIZE) = 
                convertFromBase64SymbolTonum(*(pDst + completeBlockNum * ENCRYPT_BLOCK_SIZE));
            break;
        case 2:
            *(pDst + completeBlockNum * ENCRYPT_BLOCK_SIZE) = 
                (((*(src + completeBlockNum * DECRYPT_BLOCK_SIZE + 0)) & 0x3f) << 2) | 
                (((*(src + completeBlockNum * DECRYPT_BLOCK_SIZE + 1)) & 0x30) >> 4);
            *(pDst + completeBlockNum * ENCRYPT_BLOCK_SIZE) = 
                convertFromBase64SymbolTonum(*(pDst + completeBlockNum * ENCRYPT_BLOCK_SIZE));
            
            *(pDst + completeBlockNum * ENCRYPT_BLOCK_SIZE + 1) = 
                (((*(src + completeBlockNum * DECRYPT_BLOCK_SIZE + 1)) & 0x0f) << 4) | 
                (((*(src + completeBlockNum * DECRYPT_BLOCK_SIZE + 2)) & 0x3c) >> 2);
            *(pDst + completeBlockNum * ENCRYPT_BLOCK_SIZE + 1) = 
                convertFromBase64SymbolTonum(*(pDst + completeBlockNum * ENCRYPT_BLOCK_SIZE + 1));
            break;
        default:
            break;
    }

    *pDstLen = completeBlockNum * ENCRYPT_BLOCK_SIZE + lastBlockBytes;
    return pDst;
}

/*
    Do crypt to @src;
*/
unsigned char * moCrypt_BASE64_Chars(const BASE64_CRYPT_METHOD method, 
    const unsigned char *src, const unsigned int srcLen, unsigned int *pDstLen)
{
    if(NULL == src || NULL == pDstLen)
    {
        error("Input param is NULL.\n");
        return NULL;
    }

    if(0 == srcLen)
    {
        error("Input length is 0, will do nothing.\n");
        return NULL;
    }

    unsigned char * pDst = NULL;

    switch(method)
    {
        case BASE64_CRYPT_METHOD_ENCRYPT:
            pDst = encryptChars(src, srcLen, pDstLen);
            break;
        case BASE64_CRYPT_METHOD_DECRYPT:
            pDst = decryptChars(src, srcLen, pDstLen);
            break;
        default:
            error("Input method is %d, invalid!\n", method);
            break;
    }
    
    return pDst;
}

void moCrypt_BASE64_dumpChars(const unsigned char *pTxt, const unsigned int len)
{
    printf("Dump charactors here : \n\t");
    unsigned int i = 0;
    for(i = 0; i < len; i++)
    {
        printf("%c", *(pTxt + i));
    }
    printf("\n");
}

int moCrypt_BASE64_File(const BASE64_CRYPT_METHOD method,const char * pSrcFilepath,const char * pDstFilepath)
{
    //TODO
    return 0;
}

void moCrypt_BASE64_free(unsigned char * pChars)
{
    if(pChars != NULL)
    {
        free(pChars);
        pChars = NULL;
    }
}




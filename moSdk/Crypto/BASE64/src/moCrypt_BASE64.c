#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#include "moCrypt.h"
#include "moUtils.h"

#define ENCRYPT_BLOCK_SIZE      3               //In base64, 3bytes is a block when encrypt, 4bytes is a block when decrypt
#define DECRYPT_BLOCK_SIZE      4               //In base64, 3bytes is a block when encrypt, 4bytes is a block when decrypt
#define TEMP_FILE_PREFIX		"moBase64Temp_"
#define PADDING_SYMB            '='
//Each time, read CRYPT_FILE_BLOCKSIZE bytes data from src file, do encrypt, and set result to dst file
//768 = 256 * 3 = 192 * 4
#define CRYPT_FILE_BLOCKSIZE  768

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
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "Input param is NULL.\n");
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
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, 
            "Malloc for cipher text failed! dst length = %d\n", 
            allBlockNum * DECRYPT_BLOCK_SIZE);
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
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "Input param is NULL.\n");
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
    moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, "padSymbNum = %d\n", padSymbNum);

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
    moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, 
        "completeBlockNum = %d, lastBlockBytes = %d\n", 
        completeBlockNum, lastBlockBytes);

    //get memory for pDst
    unsigned char *pDst = NULL;
    pDst = (unsigned char *)malloc(sizeof(unsigned char ) * (completeBlockNum * ENCRYPT_BLOCK_SIZE + lastBlockBytes));
    if(NULL == pDst)
    {
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, 
            "malloc failed! errno = %d, desc = [%s], size = [%d]\n", 
            errno, strerror(errno), 
            completeBlockNum * ENCRYPT_BLOCK_SIZE + lastBlockBytes);
        return NULL;
    }
    moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, "The size of dst txt is %d\n", 
        completeBlockNum * ENCRYPT_BLOCK_SIZE + lastBlockBytes);

    //complete block being done firstly
    int blkCnt = 0;
    for(blkCnt = 0; blkCnt < completeBlockNum; blkCnt++)
    {
    	moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, "blkCnt = %d\n", blkCnt);

    	//Convert all cipher char to pos in MAP
    	unsigned char var0 =
    			convertFromBase64SymbolTonum(*(src + blkCnt * DECRYPT_BLOCK_SIZE + 0));
    	unsigned char var1 =
    			convertFromBase64SymbolTonum(*(src + blkCnt * DECRYPT_BLOCK_SIZE + 1));
    	unsigned char var2 =
    			convertFromBase64SymbolTonum(*(src + blkCnt * DECRYPT_BLOCK_SIZE + 2));
    	unsigned char var3 =
    			convertFromBase64SymbolTonum(*(src + blkCnt * DECRYPT_BLOCK_SIZE + 3));

        *(pDst + blkCnt * ENCRYPT_BLOCK_SIZE + 0) = 
            (((var0) & 0x3f) << 2) | (((var1) & 0x30) >> 4);

        *(pDst + blkCnt * ENCRYPT_BLOCK_SIZE + 1) = 
            (((var1) & 0x0f) << 4) | (((var2) & 0x3c) >> 2);
        
        *(pDst + blkCnt * ENCRYPT_BLOCK_SIZE + 2) = 
            (((var2) & 0x03) << 6) | (((var3) & 0x3f) >> 0);
    }

    //the last block being done, '='is padding symbol, number is 1 or 2;
    switch(lastBlockBytes)
    {
        case 1:
        {
        	unsigned char var0 =
        			convertFromBase64SymbolTonum(*(src + blkCnt * DECRYPT_BLOCK_SIZE + 0));
        	unsigned char var1 =
        			convertFromBase64SymbolTonum(*(src + blkCnt * DECRYPT_BLOCK_SIZE + 1));

            *(pDst + completeBlockNum * ENCRYPT_BLOCK_SIZE) = 
                (((var0) & 0x3f) << 2) | (((var1) & 0x30) >> 4);
        }
            break;
        case 2:
        {
        	unsigned char var0 =
        			convertFromBase64SymbolTonum(*(src + blkCnt * DECRYPT_BLOCK_SIZE + 0));
        	unsigned char var1 =
        			convertFromBase64SymbolTonum(*(src + blkCnt * DECRYPT_BLOCK_SIZE + 1));
        	unsigned char var2 =
        			convertFromBase64SymbolTonum(*(src + blkCnt * DECRYPT_BLOCK_SIZE + 2));

            *(pDst + completeBlockNum * ENCRYPT_BLOCK_SIZE) = 
                (((var0) & 0x3f) << 2) | (((var1) & 0x30) >> 4);
            *(pDst + completeBlockNum * ENCRYPT_BLOCK_SIZE + 1) = 
                (((var1) & 0x0f) << 4) | (((var2) & 0x3c) >> 2);
        }
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
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "Input param is NULL.\n");
        return NULL;
    }

    if(0 == srcLen)
    {
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, 
            "Input length is 0, will do nothing.\n");
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
            moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "Input method is %d, invalid!\n", method);
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

/*
    Get the tmp file path;
*/
static char * getTmpFilepath(const char * pSrcFilepath)
{
    if(NULL == pSrcFilepath)
    {
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "Input param is NULL.\n");
        return NULL;
    }

    MOUTILS_FILE_DIR_FILENAME info;
    memset(&info, 0x00, sizeof(MOUTILS_FILE_DIR_FILENAME));
    int ret = moUtils_File_getDirAndFilename(pSrcFilepath, &info);
    if(ret != MOUTILS_FILE_ERR_OK)
    {
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, 
            "moUtils_File_getDirAndFilename failed! ret = %x, srcFilepath = [%s]\n",
            ret, pSrcFilepath);
        return NULL;
    }

    char * pTmpFilepath = NULL;
    unsigned int len = strlen(info.pDirpath) + 1 + strlen(TEMP_FILE_PREFIX) + strlen(info.pFilename) + 1;
    pTmpFilepath = (char *)malloc(sizeof(char) * len);
    if(NULL == pTmpFilepath)
    {
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, 
            "Malloc failed! length = %d, errno = %d, desc = [%s]\n", 
            len, errno, strerror(errno));
        goto OUT;
    }
    memset(pTmpFilepath, 0x00, len);
    sprintf(pTmpFilepath, "%s/%s%s", info.pDirpath, TEMP_FILE_PREFIX, info.pFilename);
    moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, "pTmpFilepath is [%s]\n", pTmpFilepath);
    
OUT:
    free(info.pDirpath);
    info.pDirpath = NULL;
    free(info.pFilename);
    info.pFilename = NULL;
    return pTmpFilepath;
}

/*
    1.Get tmp filepath;
    2.open src as reading mode, open tmp as writing mode;
    3.do crypt from src to tmp;
    4.close src and tmp;
    5.open src as writing mode, open tmp as reading mode;
    6.copy from tmp to src;
    7.close src and tmp;
    8.delete tmp;
*/
static int cryptFileToSame(const BASE64_CRYPT_METHOD method, const char * pSrcFilepath)
{
    if(NULL == pSrcFilepath)
    {
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "Input param is NULL.\n");
        return MOCRYPTBASE64_ERR_INPUTNULL;
    }

    //1.Get the tmp file path
    char * pTmpFilepath = getTmpFilepath(pSrcFilepath);
    if(NULL == pTmpFilepath)
    {
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "getTmpFilepath failed!\n");
        return MOCRYPTBASE64_ERR_MALLOCFAILED;
    }
    moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, 
        "Step 1 : get tmp file path has been over, it is [%s]\n", 
        pTmpFilepath);
    
    //2.Open src file for reading, open tmp for writing
    FILE * fpSrc = fopen(pSrcFilepath, "rb");
    if(NULL == fpSrc)
    {
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME,  
            "Open src file [%s] for reading failed! errno = %d, desc = [%s]\n", 
            pSrcFilepath, errno, strerror(errno));
        free(pTmpFilepath);
        pTmpFilepath = NULL;
        return MOCRYPTBASE64_ERR_FILEOPENFAIL;
    }
    FILE * fpTmp = fopen(pTmpFilepath, "wb");
    if(NULL == fpTmp)
    {
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, 
            "Open tmp file [%s] for writing failed! errno = %d, desc = [%s]\n", 
            pTmpFilepath, errno, strerror(errno));
        free(pTmpFilepath);
        pTmpFilepath = NULL;
        fclose(fpSrc);
        fpSrc = NULL;
        return MOCRYPTBASE64_ERR_FILEOPENFAIL;
    }
    moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, "Step 2 : open src for read, open tmp for write has been over.\n");
    
    //3.Do crypt 
    int readNum = 0;
    unsigned char buf[CRYPT_FILE_BLOCKSIZE] = {0x00};
    while((readNum = fread(buf, 1, CRYPT_FILE_BLOCKSIZE, fpSrc)) > 0)
    {
        moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, 
            "In this loop, readNum = %d, CRYPT_FILE_BLOCKSIZE = %d\n", 
            readNum, CRYPT_FILE_BLOCKSIZE);
        
        if(method == BASE64_CRYPT_METHOD_ENCRYPT)
        {
            unsigned int cipherLen = 0;
            unsigned char * pCipherTxt = encryptChars(buf, readNum, &cipherLen);
            if(NULL == pCipherTxt)
            {
                moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "encryptChars failed!\n");
                free(pTmpFilepath);
                pTmpFilepath = NULL;
                fclose(fpSrc);
                fpSrc = NULL;
                fclose(fpTmp);
                fpTmp = NULL;
                return MOCRYPTBASE64_ERR_MALLOCFAILED;
            }
            //Write to dst file
            fwrite(pCipherTxt, 1, cipherLen, fpTmp);
            fflush(fpTmp);
            free(pCipherTxt);
            pCipherTxt = NULL;
        }
        else
        {
            unsigned int plainLen = 0;
            unsigned char * pPlainTxt = decryptChars(buf, readNum, &plainLen);
            if(NULL == pPlainTxt)
            {
                moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "decryptChars failed!\n");
                free(pTmpFilepath);
                pTmpFilepath = NULL;
                fclose(fpSrc);
                fpSrc = NULL;
                fclose(fpTmp);
                fpTmp = NULL;
                return MOCRYPTBASE64_ERR_MALLOCFAILED;
            }
            //Write to dst file
            fwrite(pPlainTxt, 1, plainLen, fpTmp);
            fflush(fpTmp);
            free(pPlainTxt);
            pPlainTxt = NULL;
        }

        memset(buf, 0x00, CRYPT_FILE_BLOCKSIZE);
    }
    moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, 
        "Step 3 : do crypt from src to tmp has been over.\n");

    //4.close files
    fclose(fpSrc);
    fpSrc = NULL;
    fclose(fpTmp);
    fpTmp = NULL;
    moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, 
        "Step 4 : Close src file and tmp file temply has been over.\n");

    //5.open src file for writing, open tmp for reading
    fpSrc = fopen(pSrcFilepath, "wb");
    if(fpSrc == NULL)
    {
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, 
            "Open src file [%s] for writing failed! errno = %d, desc = [%s]\n",
            pSrcFilepath, errno, strerror(errno));
        free(pTmpFilepath);
        pTmpFilepath = NULL;
        return MOCRYPTBASE64_ERR_FILEOPENFAIL;
    }
    fpTmp = fopen(pTmpFilepath, "rb");
    if(fpTmp == NULL)
    {
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, 
            "Open tmp file [%s] for reading failed! errno = %d, desc = [%s]\n",
            pTmpFilepath, errno, strerror(errno));
        fclose(fpSrc);
        fpSrc = NULL;
        free(pTmpFilepath);
        pTmpFilepath = NULL;
        return MOCRYPTBASE64_ERR_FILEOPENFAIL;
    }
    moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, 
        "Step 5 : open src for write, open tmp for read has been over.\n");

    //6.Copy contents from tmp to src
    while((readNum = fread(buf, 1, CRYPT_FILE_BLOCKSIZE, fpTmp)) > 0)
    {
        fwrite(buf, 1, readNum, fpSrc);
        fflush(fpSrc);
        memset(buf, 0x00, CRYPT_FILE_BLOCKSIZE);
    }
    moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, 
        "Step 6 : copy from tmp to src has been over.\n");

    //7.close them
    fclose(fpSrc);
    fpSrc = NULL;
    fclose(fpTmp);
    fpTmp = NULL;
    moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, 
        "Step 7 : Close src file and tmp file finally has been over.\n");

    //8.delete tmp file, free memory
    unlink(pTmpFilepath);
    free(pTmpFilepath);
    pTmpFilepath = NULL;
    moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, "Step 8 : crypt being done!\n");
    
    return MOCRYPTBASE64_ERR_OK;
}


/*
    1.Open src in reading mode, open dst in writing mode;
    2.do crypt from src to dst;
    3.close src and dst;
*/
static int cryptFileToDiff(const BASE64_CRYPT_METHOD method,const char * pSrcFilepath,const char * pDstFilepath)
{
    if(NULL == pSrcFilepath || NULL == pDstFilepath)
    {
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "Input param is NULL.\n");
        return MOCRYPTBASE64_ERR_INPUTNULL;
    }

    //1.Open them
    FILE * fpSrc = NULL;
    fpSrc = fopen(pSrcFilepath, "rb");
    if(NULL == fpSrc)
    {
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, 
            "Open src file [%s] for reading failed! errno = %d, desc = [%s]\n",
            pSrcFilepath, errno, strerror(errno));
        return MOCRYPTBASE64_ERR_FILEOPENFAIL;
    }
    FILE * fpDst = NULL;
    fpDst = fopen(pDstFilepath, "wb");
    if(NULL == fpDst)
    {
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, 
            "Open dst file [%s] for writing failed! errno = %d, desc = [%s]\n",
            pDstFilepath, errno, strerror(errno));

        fclose(fpSrc);
        fpSrc = NULL;
        
        return MOCRYPTBASE64_ERR_FILEOPENFAIL;
    }

    //2.Do crypt 
    int ret = MOCRYPTBASE64_ERR_OK;
    int readNum = 0;
    unsigned char buf[CRYPT_FILE_BLOCKSIZE] = {0x00};
    while((readNum = fread(buf, 1, CRYPT_FILE_BLOCKSIZE, fpSrc)) > 0)
    {
        moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, 
            "In this loop, readNum = %d, CRYPT_FILE_BLOCKSIZE = %d\n", 
            readNum, CRYPT_FILE_BLOCKSIZE);
        
        if(method == BASE64_CRYPT_METHOD_ENCRYPT)
        {
            unsigned int cipherLen = 0;
            unsigned char * pCipherTxt = encryptChars(buf, readNum, &cipherLen);
            if(NULL == pCipherTxt)
            {
                moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "encryptChars failed!\n");
                ret = MOCRYPTBASE64_ERR_MALLOCFAILED;
                goto OUT;
            }
            //Write to dst file
            fwrite(pCipherTxt, 1, cipherLen, fpDst);
            fflush(fpDst);
            free(pCipherTxt);
            pCipherTxt = NULL;
        }
        else
        {
            unsigned int plainLen = 0;
            unsigned char * pPlainTxt = decryptChars(buf, readNum, &plainLen);
            if(NULL == pPlainTxt)
            {
                moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "decryptChars failed!\n");
                ret = MOCRYPTBASE64_ERR_MALLOCFAILED;
                goto OUT;
            }
            //Write to dst file
            fwrite(pPlainTxt, 1, plainLen, fpDst);
            fflush(fpDst);
            free(pPlainTxt);
            pPlainTxt = NULL;
        }
    }
    
OUT:
    fclose(fpSrc);
    fpSrc = NULL;
    fclose(fpDst);
    fpDst = NULL;
    return ret;
}

/*
    1.srcFilepath is the same with dstFilepath or not;
    2.read src file with block, do crypt looply, write to dst file;
*/
int moCrypt_BASE64_File(const BASE64_CRYPT_METHOD method,const char * pSrcFilepath,const char * pDstFilepath)
{
    if(NULL == pSrcFilepath || NULL == pDstFilepath)
    {
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "Input param is NULL.\n");
        return MOCRYPTBASE64_ERR_INPUTNULL;
    }
    
    MOUTILS_FILE_ABSPATH_STATE sameState;
    int ret = moUtils_File_getFilepathSameState(pSrcFilepath, pDstFilepath, &sameState);
    if(ret != 0)
    {
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, 
            "moUtils_File_getFilepathSameState failed! ret = %d(0x%x)\n", ret, ret);
        return MOCRYPTBASE64_ERR_OTHER;
    }

    switch(sameState)
    {
        case MOUTILS_FILE_ABSPATH_STATE_SAME:
            ret = cryptFileToSame(method, pSrcFilepath);
            break;
        case MOUTILS_FILE_ABSPATH_STATE_DIFF:
            ret = cryptFileToDiff(method, pSrcFilepath, pDstFilepath);
            break;
        default:
            ret = MOCRYPTBASE64_ERR_OTHER;
            break;
    }
    
    return ret;
}

void moCrypt_BASE64_free(unsigned char * pChars)
{
    if(pChars != NULL)
    {
        free(pChars);
        pChars = NULL;
    }
}




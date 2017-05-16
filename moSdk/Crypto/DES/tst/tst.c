/*
    wujl, create at 20170505, Just unit test for algorithm DES;
*/
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

#include "moCrypt.h"
#include "moLogger.h"
#include "moCrypt_DES.h"

static void dumpArrayInfo(const char * pArrayName,const unsigned char * pArray,const unsigned int len)
{
    printf("Dump array [%s] info start:\n\t", pArrayName);
    unsigned int i = 0;
    for(; i < len; i++)
    {
        printf("0x%04x ", *(pArray + i));
    }
    printf("\nDump array [%s] over.\n", pArrayName);
}

static void tst_keyExReplace(void)
{
#if 0
    unsigned char desKey[MOCRYPT_DES_KEYLEN] = {0x00};
//    unsigned char desKey[MOCRYPT_DES_KEYLEN] = 
//        {0x0e, 0xe9, 0xe3, 0xa6, 0x94, 0xe1, 0x88, 0x26};    
    memset(desKey, 0x00, MOCRYPT_DES_KEYLEN);
    //generate key random
    int i = 0;
    for(i = 0; i < MOCRYPT_DES_KEYLEN; i++)
    {
        desKey[i] = rand() % 255;
    }
    dumpArrayInfo("Org DES key", desKey, MOCRYPT_DES_KEYLEN);
    
    unsigned char reKey[KEYEX_RE_KEY_LEN] = {0x00};
    memset(reKey, 0x00, KEYEX_RE_KEY_LEN);

    //Start replace
    keyExReplace(desKey, reKey);
    dumpArrayInfo("replace key", reKey, KEYEX_RE_KEY_LEN);
#endif
}

static void tst_splitReKey(void)
{
#if 0
    unsigned char reKey[KEYEX_RE_KEY_LEN] = {0x00};
    memset(reKey, 0x00, KEYEX_RE_KEY_LEN);
    //generate key radom
    int i = 0;
    for(i = 0; i < KEYEX_RE_KEY_LEN; i++)
    {
        reKey[i] = rand() % 255;
    }
    dumpArrayInfo("OrgReKey", reKey, KEYEX_RE_KEY_LEN);

    unsigned char leftPart[KEYEX_RE_KEY_HALF_BITSLEN] = {0x00};
    unsigned char rightPart[KEYEX_RE_KEY_HALF_BITSLEN] = {0x00};
    splitReKey(reKey, leftPart, rightPart);
    dumpArrayInfo("LeftPart", leftPart, KEYEX_RE_KEY_HALF_BITSLEN);
    dumpArrayInfo("RightPart", rightPart, KEYEX_RE_KEY_HALF_BITSLEN);
#endif
}

static void tst_loopLeftShiftArray(void)
{
#if 0
    unsigned char arr[KEYEX_RE_KEY_HALF_BITSLEN] = {0x00};
    int i = 0;
    for(; i < KEYEX_RE_KEY_HALF_BITSLEN; i++)
    {
        arr[i] = rand() % 255;
    }
    dumpArrayInfo("OrgArray", arr, KEYEX_RE_KEY_HALF_BITSLEN);

    int shiftNum = rand() % KEYEX_RE_KEY_HALF_BITSLEN;
    printf("shiftNum = %d\n", shiftNum);

    loopLeftShiftArray(arr, KEYEX_RE_KEY_HALF_BITSLEN, shiftNum);
    dumpArrayInfo("LoopedArray", arr, KEYEX_RE_KEY_HALF_BITSLEN);
    
    loopLeftShiftArray(arr, KEYEX_RE_KEY_HALF_BITSLEN, KEYEX_RE_KEY_HALF_BITSLEN);
    dumpArrayInfo("LoopedArray", arr, KEYEX_RE_KEY_HALF_BITSLEN);
#endif
}

static void tst_joinHalf2ReKey(void)
{
#if 0
    unsigned char reKey[KEYEX_RE_KEY_LEN] = {0x00};
    memset(reKey, 0x00, KEYEX_RE_KEY_LEN);
    //generate key radom
    int i = 0;
    for(i = 0; i < KEYEX_RE_KEY_LEN; i++)
    {
        reKey[i] = rand() % 255;
    }
    dumpArrayInfo("OrgReKey", reKey, KEYEX_RE_KEY_LEN);

    unsigned char leftPart[KEYEX_RE_KEY_HALF_BITSLEN] = {0x00};
    unsigned char rightPart[KEYEX_RE_KEY_HALF_BITSLEN] = {0x00};
    splitReKey(reKey, leftPart, rightPart);
    dumpArrayInfo("LeftPart", leftPart, KEYEX_RE_KEY_HALF_BITSLEN);
    dumpArrayInfo("RightPart", rightPart, KEYEX_RE_KEY_HALF_BITSLEN);

    unsigned char joinKey[KEYEX_RE_KEY_LEN] = {0x00};
    memset(joinKey, 0x00, KEYEX_RE_KEY_LEN);
    joinHalf2ReKey(leftPart, rightPart, joinKey);
    dumpArrayInfo("JoinKey", joinKey, KEYEX_RE_KEY_LEN);
#endif
}

static void tst_compReKey(void)
{
#if 0
    unsigned char reKey[KEYEX_RE_KEY_LEN] = {0x00};
    int i = 0;
    for(; i < KEYEX_RE_KEY_LEN; i++)
    {
        reKey[i] = rand() % 255;
    }
    dumpArrayInfo("ReKey", reKey, KEYEX_RE_KEY_LEN);

    unsigned char keyEx[KEYEX_ELE_LEN] = {0x00};
    compReKey(reKey, keyEx);
    dumpArrayInfo("keyEx", keyEx, KEYEX_ELE_LEN);
#endif
}

static void tst_keyExpand(void)
{
#if 0
//    unsigned char desKey[MOCRYPT_DES_KEYLEN] = {0x00};  
//    memset(desKey, 0x00, MOCRYPT_DES_KEYLEN);
    
    unsigned char desKey[MOCRYPT_DES_KEYLEN] = 
    {
        0x77, 0x5a, 0xe7, 0x52, 0xca, 0x32, 0x11, 0x8a    
    };

    //generate key random
    int i = 0;
//    for(i = 0; i < MOCRYPT_DES_KEYLEN; i++)
//    {
//        desKey[i] = rand() % 255;
//    }
    dumpArrayInfo("Org DES key", desKey, MOCRYPT_DES_KEYLEN);

    unsigned char keyEx[KEYEX_ARRAY_LEN][KEYEX_ELE_LEN];
    memset(keyEx, 0x00, KEYEX_ARRAY_LEN * KEYEX_ELE_LEN);
    keyExpand(desKey, keyEx);
    printf("Dump the array now.\n");
//    for(i = 0; i < KEYEX_ARRAY_LEN; i++)
    for(i = 0; i < KEYEX_ARRAY_LEN * KEYEX_ELE_LEN; i++)
    {
        if(i % KEYEX_ELE_LEN == 0)
            printf("\n");
        int idx1 = i / KEYEX_ELE_LEN;
        int idx2 = i % KEYEX_ELE_LEN;
        printf("0x%02x, ", keyEx[idx1][idx2]);
//        dumpArrayInfo("CurKeyEx", &keyEx[i], KEYEX_ELE_LEN);
    }
    printf("\n");
#endif
}

static void tst_roundExReplace(void)
{
#if 0
    unsigned char org[UNIT_HALF_LEN_BYTES] = {0x00};
    int i = 0;
    for(i = 0; i < UNIT_HALF_LEN_BYTES; i++)
    {
        org[i] = rand() % 255;
    }
    dumpArrayInfo("Org", org, UNIT_HALF_LEN_BYTES);
    
    unsigned char ex[KEYEX_ELE_LEN] = {0x00};
    roundExReplace(org, ex);
    dumpArrayInfo("Ex", ex, KEYEX_ELE_LEN);
#endif
}

static void tst_roundXor(void)
{
#if 0
    unsigned char length = rand() % 64;

    unsigned char part1[64] = {0x00};
    int i = 0;
    for(i = 0; i < 64; i++)
    {
        part1[i] = rand() % 255;
    }
    dumpArrayInfo("part1", part1, length);
    
    unsigned char part2[64] = {0x00};
    for(i = 0; i < 64; i++)
    {
        part2[i] = rand() % 255;
    }
    dumpArrayInfo("part2", part2, length);
    
    unsigned char xorRet[64] = {0x00};
    memset(xorRet, 0x00, 64);
    roundXor(xorRet, part1, part2, length);
    dumpArrayInfo("xorRet", xorRet, length);
#endif
}

void tst_roundSboxSplitKey(void)
{
#if 0
    unsigned char bytesArr[KEYEX_ELE_LEN] = {0x00};
    int i = 0;
    for(; i < KEYEX_ELE_LEN; i++)
    {
        bytesArr[i] = rand() % 255;
    }
    dumpArrayInfo("bytesArray", bytesArr, KEYEX_ELE_LEN);

    unsigned char bitsArr[KEYEX_ELE_LEN * 8] = {0x00};
    roundSboxSplitKey(bytesArr, bitsArr);
    dumpArrayInfo("bitsArray", bitsArr, KEYEX_ELE_LEN * 8);
#endif
}

void tst_roundSbox(void)
{
#if 0
    unsigned char org[KEYEX_ELE_LEN] = {0x00};
    int i = 0;
    for(; i < KEYEX_ELE_LEN; i++)
    {
        org[i] = rand() % 255;
    }
    dumpArrayInfo("org", org, KEYEX_ELE_LEN);
    
    unsigned char dst[UNIT_HALF_LEN_BYTES] = {0x00};
    roundSbox(org, dst);
    dumpArrayInfo("dst", dst, UNIT_HALF_LEN_BYTES);
#endif
}

void tst_roundPbox(void)
{
#if 0
    unsigned char value[UNIT_HALF_LEN_BYTES] = {0x00};
    int i = 0;
    for(i = 0; i < UNIT_HALF_LEN_BYTES; i++)
    {
        value[i] = rand() % 255;
    }
    dumpArrayInfo("orgValue", value, UNIT_HALF_LEN_BYTES);

    roundPbox(value);
    dumpArrayInfo("pBoxValue", value, UNIT_HALF_LEN_BYTES);
#endif
}

void tst_ipConv(void)
{
#if 0
    unsigned char src[UNIT_LEN_BYTES] = {0x00};
    int i = 0;
    for(; i < UNIT_LEN_BYTES; i++)
    {
        src[i] = rand() % 255;
    }
    dumpArrayInfo("src", src, UNIT_LEN_BYTES);

    unsigned char dst[UNIT_LEN_BYTES] = {0x00};
    ipConv(src, dst);
    dumpArrayInfo("dst", dst, UNIT_LEN_BYTES);
#endif
}

static void tst_splitUnit2Half(void)
{
#if 0
    unsigned char src[UNIT_LEN_BYTES] = {0x00};
    int i = 0;
    for(; i < UNIT_LEN_BYTES; i++)
    {
        src[i] = rand() % 255;
    }
    dumpArrayInfo("src", src, UNIT_LEN_BYTES);

    unsigned char left[UNIT_HALF_LEN_BYTES] = {0x00};
    unsigned char right[UNIT_HALF_LEN_BYTES] = {0x00};
    splitUnit2Half(src, left, right);
    dumpArrayInfo("left", left, UNIT_HALF_LEN_BYTES);
    dumpArrayInfo("right", right, UNIT_HALF_LEN_BYTES);
#endif
}

static void tst_joinHalf2Unit(void)
{
#if 0
    unsigned char left[UNIT_HALF_LEN_BYTES] = {0x00};
    int i = 0;
    for(i = 0; i < UNIT_HALF_LEN_BYTES; i++)
    {
        left[i] = rand() % 255;
    }
    dumpArrayInfo("left", left, UNIT_HALF_LEN_BYTES);
    
    unsigned char right[UNIT_HALF_LEN_BYTES] = {0x00};
    for(i = 0; i < UNIT_HALF_LEN_BYTES; i++)
    {
        right[i] = rand() % 255;
    }
    dumpArrayInfo("right", right, UNIT_HALF_LEN_BYTES);

    unsigned char dst[UNIT_LEN_BYTES] = {0x00};
    joinHalf2Unit(left, right, dst);
    dumpArrayInfo("dst", dst, UNIT_LEN_BYTES);
#endif
}

static void tst_enCrypt_deCrypt(void)
{
#if 0
    unsigned char orgSrc[UNIT_LEN_BYTES] = {0x00};
    int i = 0;
    for(; i < UNIT_LEN_BYTES; i++)
    {
        orgSrc[i] = rand() % 255;
//        orgSrc[i] = 11;
    }
//    orgSrc[0] = 0x12;
//    orgSrc[1] = 0x34;
//    orgSrc[2] = 0x12;
//    orgSrc[3] = 0x34;
//    orgSrc[4] = 0x12;
//    orgSrc[5] = 0x34;
//    orgSrc[6] = 0x12;
//    orgSrc[7] = 0x34;
    dumpArrayInfo("orgSrc", orgSrc, UNIT_LEN_BYTES);

    unsigned char key[UNIT_LEN_BYTES] = {0x00};
    key[0] = 0x12;
    key[1] = 0x34;
    key[2] = 0x12;
    key[3] = 0x34;
    key[4] = 0x12;
    key[5] = 0x34;
    key[6] = 0x12;
    key[7] = 0x34;
    for(i = 0; i < UNIT_LEN_BYTES; i++)
    {
        key[i] = i + 32;
//        key[i] = 11;
    }
    dumpArrayInfo("key", key, UNIT_LEN_BYTES);

    unsigned char cipher[UNIT_LEN_BYTES] = {0x00};
    enCrypt(orgSrc, UNIT_LEN_BYTES, key, cipher);
    dumpArrayInfo("cipher", cipher, UNIT_LEN_BYTES);

    unsigned char plain[UNIT_LEN_BYTES] = {0x00};
    deCrypt(cipher, UNIT_LEN_BYTES, key, plain);
    dumpArrayInfo("plain", plain, UNIT_LEN_BYTES);
#endif
}

static void tst_moCrypt_DES_ECB(void)
{
#define TST_STR_LEN 32
#if 0
    unsigned char src[TST_STR_LEN] = {0x00};
    int i = 0;
    for(i = 0; i < TST_STR_LEN; i++)
    {
        src[i] = rand() % 255;
    }
    dumpArrayInfo("src", src, TST_STR_LEN);

    unsigned int keyLen = rand() % 16;
    unsigned char *key = NULL;
    key = (unsigned char *)malloc(keyLen * (sizeof(unsigned char)));
    if(key == NULL)
    {
        printf("Malloc memory for key failed! errno = %d, desc = [%s]\n",
            errno, strerror(errno));
        return ;
    }
    for(i = 0; i < keyLen; i++)
    {
        key[i] = rand() % 255;
    }
    dumpArrayInfo("key", key, keyLen);

    unsigned char cipher[TST_STR_LEN] = {0x00};
    unsigned int cipherLen = 0;
    int ret = moCrypt_DES_ECB(MOCRYPT_METHOD_ENCRYPT, src, TST_STR_LEN, 
        key, keyLen, cipher, &cipherLen);
    if(ret != 0)
    {
        printf("moCrypt_DES_ECB failed! encrypt, ret = %d\n", ret);
        free(key);
        key = NULL;
        return ;
    }
    dumpArrayInfo("cipher", cipher, cipherLen);

    unsigned char plain[TST_STR_LEN] = {0x00};
    unsigned int plainLen = 0;
    ret = moCrypt_DES_ECB(MOCRYPT_METHOD_DECRYPT, cipher, TST_STR_LEN, 
        key, keyLen, plain, &plainLen);
    if(ret != 0)
    {
        printf("moCrypt_DES_ECB failed! decrypt, ret = %d\n", ret);
        free(key);
        key = NULL;
        return ;
    }
    
    free(key);
    key = NULL;
    dumpArrayInfo("plain", plain, plainLen);
#endif
}

static void tst_moCrypt_DES_CBC(void)
{
#if 0
    unsigned char src[TST_STR_LEN] = {0x00};
    int i = 0;
    for(i = 0; i < TST_STR_LEN; i++)
    {
        src[i] = rand() % 255;
    }
    dumpArrayInfo("src", src, TST_STR_LEN);

    unsigned int keyLen = rand() % 16;
    unsigned char *key = NULL;
    key = (unsigned char *)malloc(keyLen * (sizeof(unsigned char)));
    if(key == NULL)
    {
        printf("Malloc memory for key failed! errno = %d, desc = [%s]\n",
            errno, strerror(errno));
        return ;
    }
    for(i = 0; i < keyLen; i++)
    {
        key[i] = rand() % 255;
    }
    dumpArrayInfo("key", key, keyLen);

    unsigned char iv[UNIT_LEN_BYTES] = {0x00};
    moCrypt_DES_CBC_getIv(iv);
    dumpArrayInfo("Iv", iv, UNIT_LEN_BYTES);

    unsigned char cipher[TST_STR_LEN] = {0x00};
    unsigned int cipherLen = 0;
    int ret = moCrypt_DES_CBC(MOCRYPT_METHOD_ENCRYPT, src, TST_STR_LEN, 
        key, keyLen, iv, cipher, &cipherLen);
    if(ret != 0)
    {
        printf("moCrypt_DES_ECB failed! encrypt, ret = %d\n", ret);
        free(key);
        key = NULL;
        return ;
    }
    dumpArrayInfo("cipher", cipher, cipherLen);

    unsigned char plain[TST_STR_LEN] = {0x00};
    unsigned int plainLen = 0;
    ret = moCrypt_DES_CBC(MOCRYPT_METHOD_DECRYPT, cipher, TST_STR_LEN, 
        key, keyLen, iv, plain, &plainLen);
    if(ret != 0)
    {
        printf("moCrypt_DES_ECB failed! decrypt, ret = %d\n", ret);
        free(key);
        key = NULL;
        return ;
    }
    
    free(key);
    key = NULL;
    dumpArrayInfo("plain", plain, plainLen);
#endif
}

static void tst_moCrypt_DES3_ECB(void)
{
#if 0
    unsigned char src[TST_STR_LEN] = {0x00};
    int i = 0;
    for(i = 0; i < TST_STR_LEN; i++)
    {
        src[i] = rand() % 255;
    }
    dumpArrayInfo("src", src, TST_STR_LEN);

    unsigned int keyLen = rand() % 64;
    unsigned char *key = NULL;
    key = (unsigned char *)malloc(keyLen * (sizeof(unsigned char)));
    if(key == NULL)
    {
        printf("Malloc memory for key failed! errno = %d, desc = [%s]\n",
            errno, strerror(errno));
        return ;
    }
    for(i = 0; i < keyLen; i++)
    {
        key[i] = rand() % 255;
    }
    dumpArrayInfo("key", key, keyLen);

    unsigned char cipher[TST_STR_LEN] = {0x00};
    unsigned int cipherLen = 0;
    int ret = moCrypt_DES3_ECB(MOCRYPT_METHOD_ENCRYPT, src, TST_STR_LEN, 
        key, keyLen, cipher, &cipherLen);
    if(ret != 0)
    {
        printf("moCrypt_DES_ECB failed! encrypt, ret = %d\n", ret);
        free(key);
        key = NULL;
        return ;
    }
    dumpArrayInfo("cipher", cipher, cipherLen);

    unsigned char plain[TST_STR_LEN] = {0x00};
    unsigned int plainLen = 0;
    ret = moCrypt_DES3_ECB(MOCRYPT_METHOD_DECRYPT, cipher, TST_STR_LEN, 
        key, keyLen, plain, &plainLen);
    if(ret != 0)
    {
        printf("moCrypt_DES_ECB failed! decrypt, ret = %d\n", ret);
        free(key);
        key = NULL;
        return ;
    }
    
    free(key);
    key = NULL;
    dumpArrayInfo("plain", plain, plainLen);
#endif
}

static void tst_moCrypt_DES3_CBC(void)
{
    unsigned char src[TST_STR_LEN] = {0x00};
    int i = 0;
    for(i = 0; i < TST_STR_LEN; i++)
    {
        src[i] = rand() % 255;
    }
    dumpArrayInfo("src", src, TST_STR_LEN);

    unsigned int keyLen = rand() % 16;
    unsigned char *key = NULL;
    key = (unsigned char *)malloc(keyLen * (sizeof(unsigned char)));
    if(key == NULL)
    {
        printf("Malloc memory for key failed! errno = %d, desc = [%s]\n",
            errno, strerror(errno));
        return ;
    }
    for(i = 0; i < keyLen; i++)
    {
        key[i] = rand() % 255;
    }
    dumpArrayInfo("key", key, keyLen);

    unsigned char iv[UNIT_LEN_BYTES] = {0x00};
    moCrypt_DES_CBC_getIv(iv);
    dumpArrayInfo("Iv", iv, UNIT_LEN_BYTES);

    unsigned char cipher[TST_STR_LEN] = {0x00};
    unsigned int cipherLen = 0;
    int ret = moCrypt_DES3_CBC(MOCRYPT_METHOD_ENCRYPT, src, TST_STR_LEN, 
        key, keyLen, iv, cipher, &cipherLen);
    if(ret != 0)
    {
        printf("moCrypt_DES_ECB failed! encrypt, ret = %d\n", ret);
        free(key);
        key = NULL;
        return ;
    }
    dumpArrayInfo("cipher", cipher, cipherLen);

    unsigned char plain[TST_STR_LEN] = {0x00};
    unsigned int plainLen = 0;
    ret = moCrypt_DES3_CBC(MOCRYPT_METHOD_DECRYPT, cipher, TST_STR_LEN, 
        key, keyLen, iv, plain, &plainLen);
    if(ret != 0)
    {
        printf("moCrypt_DES_ECB failed! decrypt, ret = %d\n", ret);
        free(key);
        key = NULL;
        return ;
    }
    
    free(key);
    key = NULL;
    dumpArrayInfo("plain", plain, plainLen);
}


int main(int argc, char **argv)
{
    srand((unsigned int )time(NULL));
    
    moLoggerInit("./");

    tst_keyExReplace();
    tst_splitReKey();
    tst_loopLeftShiftArray();
    tst_joinHalf2ReKey();
    tst_compReKey();
    tst_keyExpand();
    tst_roundExReplace();
    tst_roundXor();
    tst_roundSboxSplitKey();
    tst_roundSbox();
    tst_roundPbox();
    tst_ipConv();
    tst_splitUnit2Half();
    tst_joinHalf2Unit();
    tst_enCrypt_deCrypt();
    tst_moCrypt_DES_ECB();
    tst_moCrypt_DES_CBC();
    tst_moCrypt_DES3_ECB();
    tst_moCrypt_DES3_CBC();

    moLoggerUnInit();

	return 0;
}

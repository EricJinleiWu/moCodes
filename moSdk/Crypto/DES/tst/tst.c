/*
    wujl, create at 20170505, Just unit test for algorithm DES;
*/
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "moCrypt.h"
#include "moLogger.h"
#include "moCrypt_DES.h"

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

    moLoggerUnInit();

	return 0;
}

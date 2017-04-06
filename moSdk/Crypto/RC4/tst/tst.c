/*
wujl, create at 20160614, Just unit test for algorithm RC4;
*/
#include <stdio.h>
#include <string.h>

#include "moCrypt.h"

static void dumpTxt(const char *txt, const int txtLen)
{
	int i = 0;
	for(i = 0; i < txtLen; i++)
	{
		printf("0x%04x ", txt[i]);
	}
	printf("\n");
}

static void tstBasicCryptString(void)
{
#if 0
		char txt[8] = "abc";
		char key[8] = "key";
	
		printf("Org txt info :");
		dumpTxt(txt, 8);
	
		int ret = moCrypt_RC4_cryptString(key, strlen(key), txt, 8);
		printf("encrypt ret = %d, txt is :", ret);
		dumpTxt(txt, 8);
	
		ret = moCrypt_RC4_cryptString(key, strlen(key), txt, 8);
		printf("decrypt ret = %d, txt is :", ret);
		dumpTxt(txt, 8);
#else
		unsigned char txt[2048] = {0x00};
		unsigned char key[16] = "abcdefghiwjlsdc";
		int i = 0;
		unsigned char temp[64] = "0123456789adcdefghijklmnopqrstuvwxyz=-//.,~!@#$%&(){}[]stuvwxyz";
		for(i = 0; i < 2048 / 64; i++)
		{
	//		printf("i = %d\n", i);
			memcpy(txt + i * 64, temp, 64);
			*(txt + i * 64 + 63) = 'o';
		}
		txt[2047] = 0x00;
		printf("Org txt is [%s]\n", txt);
	
		int ret = moCrypt_RC4_cryptString(key, 16, txt, 2048);
		printf("encrypt ret = %d", ret);
	
		ret = moCrypt_RC4_cryptString(key, 16, txt, 2048);
		printf("decrypt ret = %d", ret);
		printf("Crypt txt is [%s]\n", txt);
	
#endif
}

static int progressOk = 0;

static void progressFunc(int prog)
{
    printf("wjl_test : prog is %d\n", prog);
    if(prog >= 100)
        progressOk = 1;
}

static void tst_CryptoAlgoRc4_cryptFile(void)
{
    MOCRYPT_RC4_FILEINFO info;
    memset(&info, 0x00, sizeof(MOCRYPT_RC4_FILEINFO));
    strcpy(info.pSrcFilepath, "./srcFile");
    strcpy(info.pDstFilepath, "./cipherFile");
    strcpy(info.pKey, "12345678912345");
    info.keyLen = 16;
    info.pCallback = NULL;
    
	int ret = moCrypt_RC4_cryptFile(&info);
	printf("Do encrypt from [%s] to [%s] return [%d]\n", info.pSrcFilepath, info.pDstFilepath, ret);

    memset(info.pSrcFilepath, 0x00, sizeof(info.pSrcFilepath));
    strcpy(info.pSrcFilepath, "./cipherFile");
    memset(info.pDstFilepath, 0x00, sizeof(info.pDstFilepath));
    strcpy(info.pDstFilepath, "./plainFile");
    info.pCallback = progressFunc;
    ret = moCrypt_RC4_cryptFile(&info);
    printf("Do decrypt from [%s] to [%s] return [%d]\n", info.pSrcFilepath, info.pDstFilepath, ret);
}

int main(int argc, char **argv)
{
    moLoggerInit("./");
	
//	tstBasicCryptString();

	tst_CryptoAlgoRc4_cryptFile();

    while(1)
    {
        if(progressOk)
        {
            break;
        }
        usleep(5000);
    }

    moLoggerUnInit();

	return 0;
}

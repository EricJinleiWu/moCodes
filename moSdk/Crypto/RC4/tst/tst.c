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

/* getFileNameDirFromPath is a static function, if do test, should modify src files; */
//static void tst_getFileNameDirFromPath(void)
//{
//	char filepath[128] = "/home/wujinlei/test/test.file";
//	char filename[128] = {0x00};
//	char filedir[128] = {0x00};

//	int ret = getFileNameDirFromPath(filepath, filename, filedir);
//	printf("filepath=[%s], filename = [%s], filedir = [%s], ret=[%d]\n", 
//		filepath, filename, filedir, ret);

//	strcpy(filepath, "justFileTest");
//	memset(filename, 0x00, 128);
//	memset(filedir, 0x00, 128);
//	ret = getFileNameDirFromPath(filepath, filename, filedir);
//	printf("filepath=[%s], filename = [%s], filedir = [%s], ret=[%d]\n", 
//		filepath, filename, filedir, ret);

//	strcpy(filepath, "");
//	memset(filename, 0x00, 128);
//	memset(filedir, 0x00, 128);
//	ret = getFileNameDirFromPath(filepath, filename, filedir);
//	printf("filepath=[%s], filename = [%s], filedir = [%s], ret=[%d]\n", 
//		filepath, filename, filedir, ret);
//	
//}

//static void tst_genTempDstFilepath(void)
//{
//	char *tempPath = NULL;
//	char srcFilepath[] = "/home/wujinlei/test/test.file";
//	tempPath = genTempDstFilepath(srcFilepath);
//	printf("Srcfilepath=[%s], tempFilepath=[%s]\n", srcFilepath, tempPath);
//	freeTempDstFilepath(tempPath);

//	strcpy(srcFilepath, "justForTest");
//	tempPath = genTempDstFilepath(srcFilepath);
//	printf("Srcfilepath=[%s], tempFilepath=[%s]\n", srcFilepath, tempPath);
//	freeTempDstFilepath(tempPath);
//}

static void tst_CryptoAlgoRc4_cryptFile(void)
{
	char srcFilepath[128] = "./srcFile";
	char cipherFilepath[128] = "./cipherFile";
	char key[16] = "12345678912345";
	int keylen = 16;
	int ret = moCrypt_RC4_cryptFile(srcFilepath, cipherFilepath, key, keylen);
	printf("Do encrypt from [%s] to [%s] return [%d]\n", srcFilepath, cipherFilepath, ret);

	char plainFilepath[128] = "./plainFile";
    ret = moCrypt_RC4_cryptFile(cipherFilepath, plainFilepath, key, keylen);
    printf("Do decrypt from [%s] to [%s] return [%d]\n", cipherFilepath, plainFilepath, ret);
}

static void tst_isSrcSameWithDstFilepath(void)
{
    char src0[] = "abc.txt";
    char dst0[] = "abc.txt";
    printf("src0 = [%s], dst0 = [%s], ret = %d\n", src0, dst0, isSrcSameWithDstFilepath(src0, dst0));
    
    char src1[] = "/home/wujl/abc.txt";
    char dst1[] = "/home/wujl/abc.txt";
    printf("src1 = [%s], dst1 = [%s], ret = %d\n", src1, dst1, isSrcSameWithDstFilepath(src1, dst1));
    
    char src2[] = "/home/wujl/abc.txt";
    char dst2[] = "/home/wujl/test/abc.txt";
    printf("src2 = [%s], dst2 = [%s], ret = %d\n", src2, dst2, isSrcSameWithDstFilepath(src2, dst2));
    
    char src3[] = "/home/wujl/abc.txt";
    char dst3[] = "abc.txt";
    printf("src3 = [%s], dst3 = [%s], ret = %d\n", src3, dst3, isSrcSameWithDstFilepath(src3, dst3));
    
    char src4[] = "abc.txt";
    char dst4[] = "/home/wujl/abc.txt";
    printf("src4 = [%s], dst4 = [%s], ret = %d\n", src4, dst4, isSrcSameWithDstFilepath(src4, dst4));
    
    char src5[] = "abc.txtx";
    char dst5[] = "/home/wujl/abc.txt";
    printf("src5 = [%s], dst5 = [%s], ret = %d\n", src5, dst5, isSrcSameWithDstFilepath(src5, dst5));
}

int main(int argc, char **argv)
{
	
//	tstBasicCryptString();

//	tst_getFileNameDirFromPath();

//	tst_genTempDstFilepath();

//	tst_CryptoAlgoRc4_cryptFile();

    tst_isSrcSameWithDstFilepath();


	return 0;
}

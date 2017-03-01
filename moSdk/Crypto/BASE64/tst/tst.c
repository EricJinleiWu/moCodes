/*
    wujl, create at 20170223, Just unit test for algorithm BASE64;
*/
#include <stdio.h>
#include <string.h>

#include "moCrypt.h"

static void tst_moCrypt_BASE64_Chars(void)
{
	unsigned char src[] = "abcdABCD1234!@#$%^&*()~`zy";
	unsigned char * pCipherTxt = NULL;
	unsigned int cipherLen = 0;
	pCipherTxt = moCrypt_BASE64_Chars(BASE64_CRYPT_METHOD_ENCRYPT, src, strlen(src), &cipherLen);
	if(pCipherTxt == NULL)
	{
		printf("moCrypt_BASE64_Chars failed!\n");
		return ;
	}
	printf("moCrypt_BASE64_Chars succeed. plain txt is : \n");
	moCrypt_BASE64_dumpChars(src, strlen(src));
	printf("cipher txt is : \n");
	moCrypt_BASE64_dumpChars(pCipherTxt, cipherLen);

	//decrypt
	char *pPlainTxt = NULL;
	unsigned int plainLen = 0;
	pPlainTxt = moCrypt_BASE64_Chars(BASE64_CRYPT_METHOD_DECRYPT, pCipherTxt, cipherLen, &plainLen);
	if(NULL == pPlainTxt)
	{
		printf("moCrypt_BASE64_Chars failed!");
		moCrypt_BASE64_free(pCipherTxt);
		return ;
	}

	printf("plain txt is : \n");
	moCrypt_BASE64_dumpChars(pPlainTxt, plainLen);

	moCrypt_BASE64_free(pPlainTxt);
	moCrypt_BASE64_free(pCipherTxt);
}

void tst_moCrypt_BASE64_File(void)
{
    //1.src and dst file is not same
    char *srcfile = "a.txt";  //a.txt, abspath format
//    char *dstfile = "./b.txt";
//    int ret = moCrypt_BASE64_File(BASE64_CRYPT_METHOD_ENCRYPT, srcfile, dstfile);
//    if(ret != 0)
//    {
//        printf("moCrypt_BASE64_File to encrypt from src to dst failed! ret = %d\n", ret);
//        return ;
//    }
//    printf("moCrypt_BASE64_File to encrypt from src to dst succeed.\n");

//    char *plainfile = "c.txt";
//    ret = moCrypt_BASE64_File(BASE64_CRYPT_METHOD_DECRYPT, dstfile, plainfile);
//    if(ret != 0)
//    {
//        printf("moCrypt_BASE64_File to decrypt from dst to src failed! ret = %d\n", ret);
//        return ;
//    }
//    printf("moCrypt_BASE64_File to decrypt from dst to src succeed.\n");


    //2.the same file
    int ret = moCrypt_BASE64_File(BASE64_CRYPT_METHOD_ENCRYPT, srcfile, srcfile);
    if(ret != 0)
    {
        printf("moCrypt_BASE64_File to encrypt from src to dst failed! ret = %d\n", ret);
        return ;
    }
    printf("moCrypt_BASE64_File to encrypt from src to dst succeed.\n");

    char *plainfile = "c.txt";
    ret = moCrypt_BASE64_File(BASE64_CRYPT_METHOD_DECRYPT, srcfile, srcfile);
    if(ret != 0)
    {
        printf("moCrypt_BASE64_File to decrypt from dst to src failed! ret = %d\n", ret);
        return ;
    }
    printf("moCrypt_BASE64_File to decrypt from dst to src succeed.\n");
}

int main(int argc, char **argv)
{
//	tst_moCrypt_BASE64_Chars();

    tst_moCrypt_BASE64_File();


	return 0;
}

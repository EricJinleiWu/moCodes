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

int main(int argc, char **argv)
{
	tst_moCrypt_BASE64_Chars();

	return 0;
}

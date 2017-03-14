/*
wujl, create at 20160614, exec RC4 algorithm;

	V1.0.0, init, exec this algorithm;

	V1.0.1, encrypt and decrypt with blocks which length is 1024, this can deal with long array;
	        support crypt to file;

*/

/*
Encrypt method:
	1.Init state vector S and temp vector T;
		for i=0 to 255 do 
			S[i] =i;  
			T[i]=Key[ i mod keylen ]; 
	2.Set S with swup algo;
		for i=0 to 255 do  
			j= ( j+S[i]+T[i])mod256;  
			swap(S[i],S[j]); 
	3.Gen cipher stream;
		for r=0 to txtlen do  //r为明文长度，r字节  
			i=(i+1) mod 256;  
			j=(j+S[i])mod 256;
			swap(S[i],S[j]);  
			t=(S[i]+S[j])mod 256; 
			cipherStream[r]=S[t]; 
	4.Use cipher stream and plain txt, gen cipher txt;
		for i=0 to txtLen do
			ciphertxt[i] = plaintxt[i] XOR cipherStream[i];
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "moCrypt.h"

#define VECTOR_LEN		256		//The length of state vector and temp vector

#define KEY_MAX_LEN		256		//The max key length, cannot larger than this value
#define KEY_MIN_LEN		1		//The min key length, cannot less than this value

#define BLOCK_SIZE		1024	//split the input txt to blocks with size 1024
#define RDWR_BLOCKNUM_ONCE	1	//Once, read or write just one block

#define TEMP_FILE_PREFIX		"temp_"		

/*
check the following :
	1.key is NULL or not;
	2.length of key is more than KEY_MAX_LEN or not;
*/
static int isRightFormatKey(const unsigned char *key, const int keylen)
{
	if(NULL == key)
	{
		return -1;
	}
	if(keylen < KEY_MIN_LEN || keylen > KEY_MAX_LEN)
	{
		return -2;
	}
	return 0;
}

/*
	Do init operation to vector S and T;
	As default, input param is an array with size 256;
*/
static void initVectorSAndT(unsigned char *vectorS, unsigned char *vectorT, const unsigned char *key, const int keylen)
{
	unsigned int i = 0;
	for(i = 0; i < VECTOR_LEN; i++)
	{
		vectorS[i] = i;
		vectorT[i] = *(key + (i % keylen));
	}
}

/*
As default, input vectorS and vectorT has length 256;
*/
static void rangeVectorS(unsigned char *vectorS, const unsigned char *vectorT)
{
	unsigned int i = 0, j = 0;
	for(i = 0; i < VECTOR_LEN; i++)
	{
		j = (j + vectorS[i] + vectorT[i]) % VECTOR_LEN;
		unsigned char temp = vectorS[i];
		vectorS[i] = vectorS[j];
		vectorS[j] = temp;		
	}
}

static void genCipherStream(unsigned char *stream, const unsigned int txtLen, unsigned char *vectorS)
{
	unsigned int i = 0, j = 0, cnt = 0;
	for(cnt = 0; cnt < txtLen; cnt++)
	{
		i = (i + 1) % VECTOR_LEN;
		j = (j + vectorS[i]) % VECTOR_LEN;

		unsigned char temp = vectorS[i];
		vectorS[i] = vectorS[j];
		vectorS[j] = temp;

		unsigned char idx = (vectorS[i] + vectorS[j]) % 256;
		stream[cnt] = vectorS[idx];
	}
}

static void genCipherOrPlainTxt(unsigned char *txt, const unsigned char *stream, const unsigned int txtLen)
{
	unsigned int i = 0;
	for(i = 0; i < txtLen; i++)
	{
		txt[i] ^= stream[i];
	}
}

static int blockEncryptDecrypt(const unsigned char *key, const unsigned int keylen, unsigned char *txt, const unsigned int txtLen)
{
	//Init S and T
	unsigned char vectorS[VECTOR_LEN] = {0x00};
	memset(vectorS, 0x00, VECTOR_LEN);
	unsigned char vectorT[VECTOR_LEN] = {0x00};
	memset(vectorT, 0x00, VECTOR_LEN);

	initVectorSAndT(vectorS, vectorT, key, keylen);
	
	//Set right values to vector S rangeable
	rangeVectorS(vectorS, vectorT);
	
	//Generate cipher stream
	unsigned char *stream = NULL;
	stream = (unsigned char *)malloc(txtLen * sizeof(unsigned char ));
	if(NULL == stream)
	{
		return -1;
	}
	memset(stream, 0x00, txtLen);
	genCipherStream(stream, txtLen, vectorS);

	//Get cipher txt
	genCipherOrPlainTxt(txt, stream, txtLen);
	
	free(stream);
	stream = NULL;
	return 0;
}

/*
1.Init state vector S and temp vector T;
2.Set S with swup algo;
3.Gen cipher stream;
4.Use cipher stream and plain txt, gen cipher txt;

The encrypt and decrypt will all use this function;
*/
int moCrypt_RC4_cryptString(const unsigned char *key, const unsigned int keylen, unsigned char *txt, int txtLen)
{
	if(NULL == key || NULL == txt)
	{
		return -1;
	}
	if(0 != isRightFormatKey(key, keylen))
	{
		return -2;
	}

	int ret = 0;
	unsigned int curBlockNum = 0, curBlockLen = 0;
	while(txtLen > 0)
	{
		unsigned char *curPtr = txt + curBlockNum * BLOCK_SIZE;
		curBlockLen = (txtLen > BLOCK_SIZE) ? BLOCK_SIZE : txtLen;

		ret = blockEncryptDecrypt(key, keylen, curPtr, curBlockLen);
		if(0 != ret)
		{
			ret = -3;
		}

		curBlockNum++;
		txtLen -= BLOCK_SIZE;
	}
	
	return ret;
}

/*
	Check srcFilepath is the same with dstFilepath or not;
	return 1, means they are the same;
	return 0, different;

	We should deal with some possibilities:
	    srcFilepath is : /home/wujl/a.txt;
	    dstFilepath is : a.txt or ./a.txt, 
	        or our process running in /home/wujl/dirB, dstFilepath is : ../a.txt;
	        or any others;
	    So, I decide, if they have same filename, I will deal with it as same file;
	    this is ugly, but I donot find other good resolution....
*/
//static int isSrcSameWithDstFilepath(const char *srcFilepath, const char *dstFilepath)
int isSrcSameWithDstFilepath(const char *srcFilepath, const char *dstFilepath)
{
	int ret = 0;
    
	if(0 == strcmp(srcFilepath, dstFilepath))
	{
		ret = 1;
        return ret;
	}

    char *srcpathLastSymbPos = strrchr(srcFilepath, '/');
    if(NULL == srcpathLastSymbPos)
    {
        char *dstpathLastSymbPos = strrchr(dstFilepath, '/');
        if(NULL == dstpathLastSymbPos)
        {
            //They all just have filename, so just compare them directly;
            if(0 == strcmp(srcFilepath, dstFilepath))
            {
                ret = 1;
                return ret;
            }
            else
            {
                ret = 0;
                return ret;
            }
        }
        else
        {
            //srcFilepath is filename, dstFilepath is not
            if(0 == strcmp(srcFilepath, dstpathLastSymbPos + 1))
            {
                ret = 1;
                return ret;
            }
            else
            {
                ret = 0;
                return ret;
            }
        }
    }
    else
    {
        char *dstpathLastSymbPos = strrchr(dstFilepath, '/');
        if(NULL == dstpathLastSymbPos)
        {
            //srcFilepath is a filepath, dstFilepath is just a filename
            if(0 == strcmp(srcpathLastSymbPos + 1, dstFilepath))
            {
                ret = 1;
                return ret;
            }
            else
            {
                ret = 0;
                return ret;
            }
        }
        else
        {
            //They all filepath
            if(0 == strcmp(srcpathLastSymbPos + 1, dstpathLastSymbPos + 1))
            {
                ret = 1;
                return ret;
            }
            else
            {
                ret = 0;
                return ret;
            }
        }        
    }
}

/* 
	get filename and directory from filepath:
	1.complete path : like "/home/jinlei/test/test.file", The directory will be "/home/jinlei/test/", filename should be test.file;
	2.just name : like "test.file", this indicates in current directory, so directory will be "", and filename is "test.file" directly;
*/
static int getFileNameDirFromPath(const char *srcFilepath, char *srcFilename, char *srcFileDir)
{
	if(NULL == srcFilepath || NULL == srcFilename)
	{
		return -1;
	}
	if(0 == strlen(srcFilepath))
	{
		return -2;
	}
	char *lastSymbPos = strrchr(srcFilepath, '/');
	if(NULL == lastSymbPos)
	{
		strcpy(srcFilename, srcFilepath);
		strcpy(srcFileDir, "");
	}
	else
	{
		int filenameLen = strlen(srcFilepath) - (lastSymbPos - srcFilepath + 1);
		if(0 == filenameLen)
		{
			return -3;
		}
		strncpy(srcFilename, lastSymbPos + 1, filenameLen);
		srcFilename[filenameLen] = 0x00;

		int dirLen = lastSymbPos - srcFilepath + 1;
		strncpy(srcFileDir, srcFilepath, dirLen);
		srcFileDir[dirLen]= 0x00;
	}
	return 0;
}

/*
	generate temp dst file path when input srcfile and dstfile is the same one;
	we should record info to temp dst file firstly, after all being done, copy contents to dst file directly;

	If srcFilepath is "/home/jinlei/tst.file", then we defined the temp file will be "/home/jinlei/temp_tst.file";
	If srcFilepath is "tst.file", then we defined the temp file will be "temp_tst.file";

	If generate OK, return path; Or return NULL;
	This pointer should be freed manully!
*/
static char *genTempDstFilepath(const char *srcFilepath)
{
	if(NULL == srcFilepath)
	{
		return NULL;
	}

	char *srcFilename = NULL, *srcFileDir = NULL;
	srcFilename = (char *)malloc((strlen(srcFilepath) + 1) * sizeof(char));
	if(NULL == srcFilename)
	{
		return NULL;
	}
	srcFileDir= (char *)malloc((strlen(srcFilepath) + 1) * sizeof(char));
	if(NULL == srcFileDir)
	{
		free(srcFilename);
		return NULL;
	}

	char *res = NULL;
	int ret = getFileNameDirFromPath(srcFilepath, srcFilename, srcFileDir);
	if(0 != ret)
	{
		goto OUT;
	}
	res = (char *)malloc(sizeof(char) * (strlen(srcFilename) + strlen(srcFileDir) + strlen(TEMP_FILE_PREFIX) + 1));
	if(NULL == res)
	{
		goto OUT;
	}
	strncpy(res, srcFileDir, strlen(srcFileDir));
	strncpy(res + strlen(srcFileDir), TEMP_FILE_PREFIX, strlen(TEMP_FILE_PREFIX));
	strncpy(res + strlen(srcFileDir) + strlen(TEMP_FILE_PREFIX), srcFilename, strlen(srcFilename));
	res[strlen(srcFileDir) + strlen(TEMP_FILE_PREFIX) + strlen(srcFilename)] = 0x00;

OUT:
	free(srcFilename);
	free(srcFileDir);
	return res;
}

static void freeTempDstFilepath(char *path)
{
	if(NULL != path)
	{
		free(path);
		path = NULL;
	}
}

/*
	Read a block from srcFileDescriptor, do crypt to it, and set result to dstFileDescriptor
*/
static int cryptFile(FILE *srcFd, FILE *dstFd, const unsigned char *key, const int keylen)
{
	if(NULL == srcFd || NULL == dstFd)
	{
		return -1;
	}
    moLogger(MOCRYPT_LOGGER_MODULE_NAME, moLoggerLevelDebug, "cryptFile enter\n");
	int ret = 0;
	unsigned char readBuf[RDWR_BLOCKNUM_ONCE * BLOCK_SIZE] = {0x00};
	memset(readBuf, 0x00, RDWR_BLOCKNUM_ONCE * BLOCK_SIZE);
	int readLen = -1;
	while((readLen = fread(readBuf, RDWR_BLOCKNUM_ONCE, BLOCK_SIZE, srcFd)) > 0)
	{
		ret = moCrypt_RC4_cryptString(key, keylen, readBuf, readLen);
		if(0 != ret)
		{
			break;
		}
		int writeLen = fwrite(readBuf, 1, readLen, dstFd);
		if(writeLen != readLen)
		{
			ret = -2;
			break;
		}
		memset(readBuf, 0x00, BLOCK_SIZE * RDWR_BLOCKNUM_ONCE);		
        moLogger(MOCRYPT_LOGGER_MODULE_NAME, moLoggerLevelDebug, "readLen = %d, writeLen = %d\n", readLen, writeLen);
	}
	
	return (0 == ret) ? 0 : -2;
}

/*
	Copy the contents from temp dst file to dst file, in this situation, dst file is the same with src file;

	1.Jump to the head of src file;
	2.write to src file block and block;
*/
static int copyContentsToDstfile(FILE *srcFd, FILE *dstFd)
{
    moLogger(MOCRYPT_LOGGER_MODULE_NAME, moLoggerLevelDebug, "copyContentsToDstfile enter \n");
	if(NULL == srcFd || NULL == dstFd)
	{
		return -1;
	}
	if(0 != fseek(srcFd, 0, SEEK_SET))
	{
		return -2;
	}
	if(0 != fseek(dstFd, 0, SEEK_SET))
	{
		return -3;
	}
	int ret = 0;
	int readLen = -1, writeLen = -1;
	char readBuf[BLOCK_SIZE * RDWR_BLOCKNUM_ONCE] = {0x00};
	memset(readBuf, 0x00, BLOCK_SIZE * RDWR_BLOCKNUM_ONCE);
	while((readLen = fread(readBuf, BLOCK_SIZE, RDWR_BLOCKNUM_ONCE, dstFd)) > 0)
	{
		writeLen = fwrite(readBuf, readLen, 1, srcFd);
		if(writeLen != readLen)
		{
			ret = -4;
			break;
		}
		memset(readBuf, 0x00, BLOCK_SIZE * RDWR_BLOCKNUM_ONCE);		
	}
	
	return ret;
}

/*
1.Check the src file is valid or not;
2.check src file and dst file is the same one or not;
3.Read src file as block, and do crypt to it, set it to dst file;
4.looply do step3, just util this file being read to the end;
5.If dst file is the same with src file, we need a temp file to record the result, and after crypt being done, use this temp file cover the src file;
*/
int moCrypt_RC4_cryptFile(const char *srcFilepath, const char *dstFilepath, const unsigned char *key, const unsigned int keylen)
{
	if(NULL == srcFilepath || NULL == dstFilepath || NULL == key)
	{
		return -1;
	}
	if(0 != isRightFormatKey(key, keylen))
	{
		return -2;
	}

	FILE *srcFd = NULL, *dstFd = NULL;
	char *tempDstFilepath = NULL;
	if(isSrcSameWithDstFilepath(srcFilepath, dstFilepath))
	{
		tempDstFilepath = genTempDstFilepath(srcFilepath);
		if(NULL == tempDstFilepath)
		{
			return -3;
		}
		if(NULL == (srcFd = fopen(srcFilepath, "rb+")))
		{
			return -4;
		}
        moLogger(MOCRYPT_LOGGER_MODULE_NAME, moLoggerLevelDebug, "tempDstFilepath = [%s]\n", tempDstFilepath);
		if(NULL == (dstFd = fopen(tempDstFilepath, "wb")))
		{
			fclose(srcFd);
			srcFd = NULL;
			return -5;
		}
	}
	else
	{
		if(NULL == (srcFd = fopen(srcFilepath, "rb")))
		{
			return -4;
		}
		if(NULL == (dstFd = fopen(dstFilepath, "wb")))
		{
			fclose(srcFd);
			srcFd = NULL;
			return -6;			
		}
	}

 	int ret = cryptFile(srcFd, dstFd, key, keylen);
	if(0 != ret)
	{
		ret = -7;
		goto END;
	}
	//Should copy contents from temp dst file to dst file
	if(NULL != tempDstFilepath)
	{
		ret = copyContentsToDstfile(srcFd, dstFd);
		if(0 != ret)
		{
			ret = -8;
			goto END;
		}
	}
	
	END:
		if(NULL != srcFd)
		{
			fclose(srcFd);
			srcFd = NULL;
		}
		if(NULL != dstFd)
		{
			fclose(dstFd);
			dstFd = NULL;
		}	
		if(NULL != tempDstFilepath)
		{
			freeTempDstFilepath(tempDstFilepath);
			tempDstFilepath = NULL;
		}
		return ret;
}





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
#include <errno.h>
#include <pthread.h>

#include "moCrypt.h"
#include "moUtils.h"

#define VECTOR_LEN		256		//The length of state vector and temp vector

#define BLOCK_SIZE		1024	//split the input txt to blocks with size 1024
#define RDWR_BLOCKNUM_ONCE	1	//Once, read or write just one block

#define TEMP_FILE_PREFIX		"temp_"		


#define PROGRESS_SATRT          0   //Start to crypt
#define PROGRESS_PARSEFILEPATH  5   //parse src filepath over
#define PROGRESS_OPENFILE       10  //Open srcfile and dstfile/tmpfile over
#define PROGRESS_CRYPTRANGE     80  //80% crypt blockes to src file, from 10%--90%
//After crypt, progress to 90(PROGRESS_OPENFILE + PROGRESS_CRYPTRANGE), if tmp file being used, should copy contents to dst
//when copy over, set progress to PROGRESS_TMP2DST;
#define PROGRESS_TMP2DST        95
#define PROGRESS_CRYPTOVER      100


/*
    check the following :
    	1.key is NULL or not;
    	2.length of key is more than KEY_MAX_LEN or not;

    return 0 if format invalid, else return 1;
*/
static int isRightFormatKey(const unsigned char *key, const int keylen)
{
	if(NULL == key)
	{
		return 0;
	}
	if(keylen < MOCRYPT_RC4_KEY_MIN_LEN || keylen > MOCRYPT_RC4_KEY_MAX_LEN)
	{
		return 0;
	}
	return 1;
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

/*
    Set progress by callback function;
*/
static void setProgress(const pProgBarCallbackFunc pFunc, const int prog)
{
    if(NULL != pFunc)
    {
        pFunc(prog);
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
		return MOCRYPT_RC4_ERR_MALLOCFAILED;
	}
	memset(stream, 0x00, txtLen);
	genCipherStream(stream, txtLen, vectorS);

	//Get cipher txt
	genCipherOrPlainTxt(txt, stream, txtLen);
    
	free(stream);
	stream = NULL;
	return MOCRYPT_RC4_ERR_OK;
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
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "Input param is NULL.\n");
		return MOCRYPT_RC4_ERR_INPUTPARAMNULL;
	}
	if(!isRightFormatKey(key, keylen))
	{
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "key[%s], keylen=%d, invalid!\n",
            key, keylen);
		return MOCRYPT_RC4_ERR_INVALIDKEY;
	}
    moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, "txtLen = %d\n", txtLen);

	int ret = 0;
	unsigned int curBlockNum = 0, curBlockLen = 0;
	while(txtLen > 0)
	{
		unsigned char *curPtr = txt + curBlockNum * BLOCK_SIZE;
		curBlockLen = (txtLen > BLOCK_SIZE) ? BLOCK_SIZE : txtLen;
        moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, "curBlockNum = %d, curBlockLen = %d\n",
            curBlockNum, curBlockLen);

		ret = blockEncryptDecrypt(key, keylen, curPtr, curBlockLen);
		if(0 != ret)
		{
            moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "blockEncryptDecrypt failed. ret = %d\n", ret);
            break;
		}
        
		curBlockNum++;
		txtLen -= BLOCK_SIZE;
        moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, "txtLen = %d\n", txtLen);
	}
	
	return ret;
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
static int cryptFileBlockly(FILE *srcFd, const int srcFilesize, FILE *dstFd, const unsigned char *key, 
    const int keylen, pProgBarCallbackFunc pProBarFunc)
{
	if(NULL == srcFd || NULL == dstFd || NULL == key || srcFilesize < 0)
	{
		return MOCRYPT_RC4_ERR_INPUTPARAMNULL;
	}

    moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, "srcFilesize = %d\n", srcFilesize);

    int ret = 0;
	unsigned char readBuf[RDWR_BLOCKNUM_ONCE * BLOCK_SIZE] = {0x00};
	memset(readBuf, 0x00, RDWR_BLOCKNUM_ONCE * BLOCK_SIZE);
	int readLen = -1;
    int blockNum = 0;
    int maxBlockNum = (0 == srcFilesize % BLOCK_SIZE) ? (srcFilesize / BLOCK_SIZE) : (srcFilesize / BLOCK_SIZE + 1);
	while((readLen = fread(readBuf, RDWR_BLOCKNUM_ONCE, BLOCK_SIZE, srcFd)) > 0)
	{
		ret = moCrypt_RC4_cryptString(key, keylen, readBuf, readLen);
		if(0 != ret)
		{
            moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "moCrypt_RC4_cryptString failed! ret = %d\n", ret);
			break;
		}
		int writeLen = fwrite(readBuf, 1, readLen, dstFd);
		if(writeLen != readLen)
		{
            moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "fwrite failed! " \
                "length will be write is [%d], writen length is [%d], errno = %d, desc = [%s]\n",
                readLen, writeLen, errno, strerror(errno));
			ret = MOCRYPT_RC4_ERR_WRITEFILEFAIL;
			break;
		}

        blockNum++;
        float percent = (float)blockNum / maxBlockNum;
        percent *= PROGRESS_CRYPTRANGE;
        percent += PROGRESS_OPENFILE;
        moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, "blockNum = %d, maxBlockNum = %d, percent = %f\n",
            blockNum, maxBlockNum, percent);
        setProgress(pProBarFunc, (int)percent);
        
		memset(readBuf, 0x00, BLOCK_SIZE * RDWR_BLOCKNUM_ONCE);
	}

    fflush(dstFd);
	
	return ret;
}

/*
	Copy the contents from temp dst file to dst file, in this situation, dst file is the same with src file;

	1.Jump to the head of src file;
	2.write to src file block and block;
*/
static int copyContentsToDstfile(FILE *srcFd, FILE *dstFd)
{
    if(NULL == srcFd || NULL == dstFd)
	{
		return MOCRYPT_RC4_ERR_INPUTPARAMNULL;
	}

    //Jump to the beginning of two files;
    fseek(srcFd, 0, SEEK_SET);
    fseek(dstFd, 0, SEEK_SET);
    
	int ret = 0;
	int readLen = -1, writeLen = -1;
	char readBuf[BLOCK_SIZE * RDWR_BLOCKNUM_ONCE] = {0x00};
	memset(readBuf, 0x00, BLOCK_SIZE * RDWR_BLOCKNUM_ONCE);
	while((readLen = fread(readBuf, BLOCK_SIZE, RDWR_BLOCKNUM_ONCE, dstFd)) > 0)
	{
		writeLen = fwrite(readBuf, readLen, 1, srcFd);
		if(writeLen != readLen)
		{
            moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "fwrite failed! readLen = %d, writeLen = %d, errno = %d, desc = [%s]\n",
                readLen, writeLen, errno, strerror(errno));
			ret = MOCRYPT_RC4_ERR_WRITEFILEFAIL;
			break;
		}
		memset(readBuf, 0x00, BLOCK_SIZE * RDWR_BLOCKNUM_ONCE);		
	}

    fflush(srcFd);
	
	return ret;
}

/*
    1.check src file and dst file is the same one or not;
    2.Read src file as block, and do crypt to it, set it to dst file;
    3.looply do step3, just util this file being read to the end;
    4.If dst file is the same with src file, we need a temp file to record the result, and after crypt being done, use this temp file cover the src file;
*/
int cryptFile(const char *srcFilepath, const char *dstFilepath, const unsigned char *key, const unsigned int keylen, pProgBarCallbackFunc pFunc)
{
	FILE *srcFd = NULL, *dstFd = NULL;
	char *tempDstFilepath = NULL;

    setProgress(pFunc, PROGRESS_SATRT);

    MOUTILS_FILE_ABSPATH_STATE diffState;
    int ret = moUtils_File_getFilepathSameState(srcFilepath, dstFilepath, &diffState);
    if(ret != MOUTILS_FILE_ERR_OK)
    {
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "moUtils_File_getFilepathSameState failed! " \
            "ret = %d, srcfilepath=[%s], dstfilepath=[%s]\n",
            ret, srcFilepath, dstFilepath);
        setProgress(pFunc, MOCRYPT_RC4_ERR_GETFILEDIFFSTATEFAIL);
        return MOCRYPT_RC4_ERR_GETFILEDIFFSTATEFAIL;
    }
    moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, "moUtils_File_getFilepathSameState OK. " \
        "srcfilepath=[%s], dstfilepath=[%s], diffState=%d\n",
        srcFilepath, dstFilepath, diffState);
    setProgress(pFunc, PROGRESS_PARSEFILEPATH);

    //If @srcFilepath same with @dstFilepath, should create a temp file;
	if(MOUTILS_FILE_ABSPATH_STATE_SAME == diffState)
	{
		tempDstFilepath = genTempDstFilepath(srcFilepath);
		if(NULL == tempDstFilepath)
		{
            moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "genTempDstFilepath failed! srcFilepath = [%s]\n", 
                srcFilepath);
            setProgress(pFunc, MOCRYPT_RC4_ERR_GENTMPFILE);
			return MOCRYPT_RC4_ERR_GENTMPFILE;
		}
		if(NULL == (srcFd = fopen(srcFilepath, "rb+")))
		{
            moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "fopen src file [%s] failed, errno = %d, desc = [%s]\n",
                srcFilepath, errno, strerror(errno));
            setProgress(pFunc, MOCRYPT_RC4_ERR_OPENFILEFAILED);
			return MOCRYPT_RC4_ERR_OPENFILEFAILED;
		}
        moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, "tempDstFilepath = [%s]\n", tempDstFilepath);
		if(NULL == (dstFd = fopen(tempDstFilepath, "wb")))
		{
            moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "fopen tmp file [%s] failed, errno = %d, desc = [%s]\n",
                tempDstFilepath, errno, strerror(errno));
            
			fclose(srcFd);
			srcFd = NULL;
            
            setProgress(pFunc, MOCRYPT_RC4_ERR_OPENFILEFAILED);
			return MOCRYPT_RC4_ERR_OPENFILEFAILED;
		}
	}
	else
	{
		if(NULL == (srcFd = fopen(srcFilepath, "rb")))
		{
            moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "fopen src file [%s] failed, errno = %d, desc = [%s]\n",
                srcFilepath, errno, strerror(errno));
            setProgress(pFunc, MOCRYPT_RC4_ERR_OPENFILEFAILED);
			return MOCRYPT_RC4_ERR_OPENFILEFAILED;
		}
		if(NULL == (dstFd = fopen(dstFilepath, "wb")))
		{
            moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "fopen dst file [%s] failed, errno = %d, desc = [%s]\n",
                dstFilepath, errno, strerror(errno));
            
			fclose(srcFd);
			srcFd = NULL;
            
            setProgress(pFunc, MOCRYPT_RC4_ERR_OPENFILEFAILED);
			return MOCRYPT_RC4_ERR_OPENFILEFAILED;			
		}
	}
    setProgress(pFunc, PROGRESS_OPENFILE);

    int srcFilesize = moUtils_File_getSize(srcFilepath);
 	ret = cryptFileBlockly(srcFd, srcFilesize, dstFd, key, keylen, pFunc);
	if(0 != ret)
	{
		goto END;
	}
    
	//Should copy contents from temp dst file to dst file
	if(NULL != tempDstFilepath)
	{
		ret = copyContentsToDstfile(srcFd, dstFd);
		if(0 != ret)
		{
			goto END;
		}
	}
    setProgress(pFunc, PROGRESS_TMP2DST);
	
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
    
    if(MOCRYPT_RC4_ERR_OK == ret)
        setProgress(pFunc, PROGRESS_CRYPTOVER);
    else
        setProgress(pFunc, ret);
    
	return ret;
}

/*
    This will be used as a thread;
*/
static void * cryptFileAsync(void *argv)
{
    MOCRYPT_RC4_FILEINFO cryptInfo;
    memcpy(&cryptInfo, argv, sizeof(MOCRYPT_RC4_FILEINFO));
    //Must free argv here, if not, memory leak appear
    free(argv);
    argv = NULL;
    
    cryptFile(cryptInfo.pSrcFilepath, cryptInfo.pDstFilepath, cryptInfo.pKey, cryptInfo.keyLen, cryptInfo.pCallback);

    return NULL;
}

int moCrypt_RC4_cryptFile(const MOCRYPT_RC4_FILEINFO *pCryptInfo)
{
	if(NULL == pCryptInfo || NULL == pCryptInfo->pSrcFilepath || NULL == pCryptInfo->pDstFilepath || 
        NULL == pCryptInfo->pKey)
	{
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "Input param is NULL.\n");
		return MOCRYPT_RC4_ERR_INPUTPARAMNULL;
	}
	if(!isRightFormatKey(pCryptInfo->pKey, pCryptInfo->keyLen))
	{
        moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "key[%s], keylen=%d, invalid!\n",
            pCryptInfo->pKey, pCryptInfo->keyLen);
		return MOCRYPT_RC4_ERR_INVALIDKEY;
	}

    //Do crypt sync or async, append on pCryptInfo->pProBarCallbackFunc is NULL or not;
    if(NULL == pCryptInfo->pCallback)
    {
        moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, "pCryptInfo->pCallback == NULL, will do crypt syncly.\n");
        return cryptFile(pCryptInfo->pSrcFilepath, pCryptInfo->pDstFilepath, pCryptInfo->pKey, pCryptInfo->keyLen, NULL);
    }
    else
    {
        //Must malloc a new memory for args send to thread
        //If not malloc, this param maybe free at any time, coredump appear
        MOCRYPT_RC4_FILEINFO * pArgv = NULL;
        pArgv = (MOCRYPT_RC4_FILEINFO *)malloc(sizeof(MOCRYPT_RC4_FILEINFO));
        if(NULL == pArgv)
        {
            moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "Malloc failed! size = %d\n",
                sizeof(MOCRYPT_RC4_FILEINFO));
            return MOCRYPT_RC4_ERR_MALLOCFAILED;
        }
        memcpy(pArgv, pCryptInfo, sizeof(MOCRYPT_RC4_FILEINFO));
        
        moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, "pArgv->pCallback == %p, will do crypt in a thread.\n", 
            pArgv->pCallback);
        pthread_t thId;
        if(0 != pthread_create(&thId, NULL, cryptFileAsync, (void *)pArgv))
        {
            if(NULL != pArgv)
            {
                free(pArgv);
                pArgv = NULL;
            }
            moLoggerError(MOCRYPT_LOGGER_MODULE_NAME, "Create thread cryptFileAsync failed!\n");
            return MOCRYPT_RC4_ERR_CREATETHREADFAIL;
        }
        else
        {
            moLoggerDebug(MOCRYPT_LOGGER_MODULE_NAME, "Create thread cryptFileAsync succeed.\n");
            return MOCRYPT_RC4_ERR_OK;
        }
    }
}





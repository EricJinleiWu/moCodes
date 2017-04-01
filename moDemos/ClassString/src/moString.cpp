#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "moString.h"

#define SUNDAY_NEXTTABLE_LEN 256

/*
    If malloc for mPtr failed, it will be set to NULL!
    This can be used to check isMallocOk!
*/
MoString::MoString()
{
    mPtr = NULL;
    mPtr = (char *)malloc(1 * sizeof(char ));
    if(NULL != mPtr)
    {
        *(mPtr + 0) = '\0';
    }
}

/*
    Using like : MoString str("abc");
*/
MoString::MoString(const char *ptr)
{
    mPtr = NULL;
    if(ptr != NULL)
    {     
        mPtr = (char *)malloc((strlen(ptr) + 1) * sizeof(char ));
        if(NULL != mPtr)
        {
            memset(mPtr, 0x00, strlen(ptr) + 1);
            strcpy(mPtr, ptr);
        }
    }
}

MoString::MoString(const int size, const char value)
{
    mPtr = NULL;
    mPtr = (char *)malloc((size + 1) * sizeof(char ));
    if(NULL != mPtr)
    {
        memset(mPtr, 0x00, size + 1);
        int i = 0;
        for(i = 0; i < size; i++)
        {
            *(mPtr + i) = value;
        }
        *(mPtr + size) = '\0';
    }
}

MoString::MoString(const MoString & other)
{
    mPtr = NULL;
    mPtr = (char *)malloc((other.length() + 1) * sizeof(char ));
    if(NULL != mPtr)
    {
        memset(mPtr, 0x00, strlen(other.mPtr) + 1);
        strcpy(mPtr, other.mPtr);
    }
}

MoString::~MoString()
{
    free(mPtr);
    mPtr = NULL;
}

MoString & MoString::operator=(const MoString & other)
{
    if(this != &other)
    {
        char *temp = (char *)malloc(sizeof(char) * (other.length() + 1));
        if(NULL != temp)
        {
            memset(temp, 0x00, other.length() + 1);
            strcpy(temp, other.mPtr);
            free(mPtr);
            mPtr = temp;
        }
    }
    return *this;
}

const char *MoString::c_str()
{
    return mPtr;
}

MoString & MoString::operator+(MoString & other)
{
    unsigned int origLen = strlen(mPtr);
    //Realloc for memory firstly, if realloc failed, will not modify original data in @mPtr
    char *tmp = NULL;
    tmp = (char *)realloc(mPtr, sizeof(char) * (strlen(mPtr) + strlen(other.c_str()) + 1));
    if(NULL == tmp)
    {
        error("Realloc failed! size = %d, errno = %d, desc = [%s]\n",
            strlen(mPtr) + strlen(other.c_str()) + 1, errno, strerror(errno));
    }
    else
    {
        mPtr = tmp;
        strncpy(mPtr + origLen, other.c_str(), strlen(other.c_str()));
        mPtr[strlen(mPtr) + strlen(other.c_str())] = 0x00;
    }
    return *this;
}

MoString & MoString::operator+(const char * str)
{
    unsigned int origLen = strlen(mPtr);
    char *tmp = NULL;
    tmp = (char *)realloc(mPtr, (strlen(mPtr) + strlen(str) + 1) * sizeof(char));
    if(NULL == tmp)
    {
        error("Realloc failed! size = %d, errno = %d, desc = [%s]\n",
            strlen(mPtr) + strlen(str) + 1, errno, strerror(errno));
    }
    else
    {
        mPtr = tmp;
        strncpy(mPtr + origLen, str, strlen(str));
        mPtr[strlen(mPtr) + strlen(str)] = 0x00;
    }
    return *this;
}

unsigned int MoString::length() const
{
    return strlen(mPtr);
}

/*
    Use Sunday algorithm to do this job;
*/
int MoString::subString(MoString & subStr)
{
    char nextTable[SUNDAY_NEXTTABLE_LEN] = {0x00};
    memset(nextTable, 0x00, SUNDAY_NEXTTABLE_LEN);
    sundayGenNext(nextTable, subStr.c_str(), subStr.length());
    int ret = sundaySearch(mPtr, strlen(mPtr), subStr.c_str(), subStr.length(), nextTable);
    debug("sundaySearch return %d. srcString is [%s], patternStr is [%s]\n",
        ret, mPtr, subStr.c_str());
    return ret;
}



/*
    In sunday algo., next pos has rules like : 
        If exist in @pPattern, moveBytes = @patternLen - rightPosOfThisChar;
        else, moveBytes = @patternLen + 1;
*/
int MoString::sundayGenNext(char *pNext, const char * pPattern, const unsigned int patternLen)
{
    if(NULL == pNext || NULL == pPattern)
    {
        error("Input param is NULL.\n");
        return -1;
    }
    unsigned int i = 0;
    for(; i < SUNDAY_NEXTTABLE_LEN; i++)
    {
        *(pNext + i) = patternLen + 1;
    }

    for(i = 0; i < patternLen; i++)
    {
        *(pNext + pPattern[i]) = patternLen - i;
    }

    return 0;
}


int MoString::sundaySearch(const char * pSrc, const unsigned int srcLen, 
    const char * pPattern, const unsigned int patternLen, 
    const char * pNext)
{
    if(NULL == pSrc || NULL == pPattern || NULL == pNext)
    {
        error("Input param is NULL!\n");
        return -1;
    }

    if(srcLen < patternLen)
    {
        error("Input srcLen = %d, patternLen = %d, cannot find pattern surely.\n",
            srcLen, patternLen);
        return -2;
    }

    if(0 == patternLen || 0 == srcLen)
    {
        error("patternLen = %d, srcLen = %d, they all cannot be 0!\n",
            patternLen, srcLen);
        return -3;
    }
    
    //Start matching
    unsigned char isPatFind = 0;
    unsigned int i = 0;
    for(i = 0; i <= srcLen - patternLen; )
    {
        unsigned char isMatched = 1;
        unsigned int j = 0;
        for(j = 0; j < patternLen; j++)
        {
            //pattern donot matched
            if(pSrc[i + j] != pPattern[j])
            {
                isMatched = 0;
                //Should jump several bytes, append on @pNext
                if(i + patternLen < srcLen) //Must cmp with @srcLen!
                {
                    i += pNext[(unsigned char )pSrc[i + patternLen]];
                    break;
                }
                //To the end of @pSrc, donot find @pPattern yet
                else
                {
                    //This increase is meaningless, just can break forLoop to i
                    i++;
                }
            }
        }
        //If we matched pattern, break is OK
        if(isMatched)
        {
            isPatFind = 1;
            break;
        }
    }

    if(isPatFind)
    {
        debug("Pattern being find! offset = %d\n", i);
        return i;
    }
    else
    {
        error("Pattern donot being find!\n");
        return -4;
    }
}




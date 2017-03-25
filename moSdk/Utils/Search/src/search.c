#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include "moUtils.h"

/*
    Input should not NULL;
    srcLen should not less than patternLen;
    return pos start from 0;
*/
int moUtils_Search_BF(const unsigned char * pSrc, const unsigned int srcLen, 
    const unsigned char * pPattern, const unsigned int patternLen)
{
    if(NULL == pSrc || NULL == pPattern)
    {
        moLogger(MOUTILS_LOGGER_MODULE_NAME, moLoggerLevelError, "Input param is NULL.\n");
        return MOUTILS_SEARCH_ERR_INPUTPARAMNULL;
    }

    //If srcLen less than patternLen, sub cannot be found surely.
    if(srcLen < patternLen)
    {
        moLogger(MOUTILS_LOGGER_MODULE_NAME, moLoggerLevelWarn, 
            "srcLen=%d, patternLen=%d, subString cannot be found in dstString surely.\n", 
            srcLen, patternLen);
        return MOUTILS_SEARCH_ERR_PATLENLARGER;
    }

    if(0 == patternLen || 0 == srcLen)
    {
        moLogger(MOUTILS_LOGGER_MODULE_NAME, moLoggerLevelError,
            "patternLen = %d, srcLen = %d, they all cannot be 0!\n",
            patternLen, srcLen);
        return MOUTILS_SEARCH_ERR_INVALIDLEN;
    }

    unsigned char isExist = 0;
    unsigned int offset = 0;
    for(offset = 0; (offset + patternLen) <= srcLen; offset++)
    {
        unsigned int i = 0;
        for(i = 0; i < patternLen; i++)
        {
            if(*(pSrc + offset + i) != *(pPattern + i))
            {
                break;
            }
        }
        if(i >= patternLen)
        {
            moLogger(MOUTILS_LOGGER_MODULE_NAME, moLoggerLevelDebug, 
                "Find subString! offset = %d.\n", offset);
            isExist = 1;
            break;
        }
    }
    if(isExist)
    {
        return offset;
    }
    else
    {
        return MOUTILS_SEARCH_ERR_PATNOTEXIST;
    }
}

/*
    Generate Partital Match Table here.
*/
static void genPMT(unsigned char * pmt, const unsigned char * pPattern, const unsigned int patternLen)
{
    //The index of @pPattern
    unsigned int i = 0;
    for(i = 0; i < patternLen; i++)
    {
        int pmValue = 0;    //current partital match value
        unsigned int j = 0;
        for(j = 0; j < i; j++)
        {
            unsigned int k = 0;
            for(k = 0; k < j; k++)
            {
                if(*(pPattern + k) != *(pPattern + i - j + k))
                {
                    break;
                }
            }
            //Some cases, we find equal values between prefix and suffix
            if(k >= j)
            {
                //@pmValue should refresh each time, to get the largest value
                pmValue = j + 1;
            }
        }
        *(pmt + i) = pmValue;

        //Dump the pmt value
        moLogger(MOUTILS_LOGGER_MODULE_NAME, moLoggerLevelDebug, 
            "i = %d, pmValue = %d\n", i, pmValue);
    }
}

/*
    generate the next pos table append on the Partitial Match Table.
*/
static void genKmpNext(unsigned char *pNext, const unsigned char *pmt, const unsigned int patternLen)
{
    unsigned int i = 0;
    for(; i < patternLen; i++)
    {
        //Start from 1, so use (i + 1), not (i)
        *(pNext + i) = (i + 1) - (*(pmt + i));
        //Dump the value of pNext
        moLogger(MOUTILS_LOGGER_MODULE_NAME, moLoggerLevelDebug,
            "i = %d, pmt[i] = %d, pNext[i] = %d\n", i, *(pmt + i), *(pNext + i));
    }
}

/*
    Generate Partital Match Table;
    Generate next[];
    start matching.
*/
int moUtils_Search_KMP(const unsigned char * pSrc, const unsigned int srcLen, 
    const unsigned char * pPattern, const unsigned int patternLen)
{
    if(NULL == pSrc || NULL == pPattern || srcLen == 0 || patternLen == 0)
    {
        moLogger(MOUTILS_LOGGER_MODULE_NAME, moLoggerLevelError, "Input param is NULL.\n");
        return MOUTILS_SEARCH_ERR_INPUTPARAMNULL;
    }

    if(patternLen > srcLen)
    {
        moLogger(MOUTILS_LOGGER_MODULE_NAME, moLoggerLevelError, 
            "Input patternLen=%d, srcLen=%d, will not be searched surely.\n",
            patternLen, srcLen);
        return MOUTILS_SEARCH_ERR_PATLENLARGER;
    }

    if(0 == patternLen || 0 == srcLen)
    {
        moLogger(MOUTILS_LOGGER_MODULE_NAME, moLoggerLevelError,
            "patternLen = %d, srcLen = %d, they all cannot be 0!\n",
            patternLen, srcLen);
        return MOUTILS_SEARCH_ERR_INVALIDLEN;
    }

    //Generate a partital match table to @pPattern
    unsigned char *pmt = NULL;
    pmt = (unsigned char *)malloc(sizeof(unsigned char) * (patternLen));
    if(NULL == pmt)
    {
        moLogger(MOUTILS_LOGGER_MODULE_NAME, moLoggerLevelError,
            "Malloc for Partital Match Table failed! table size = %d, errno = %d, desc = [%s]\n",
            patternLen, errno, strerror(errno));
        return MOUTILS_SEARCH_ERR_MALLOCFAILED;
    }
    genPMT(pmt, pPattern, patternLen);

    //Generate next[] table, this save the jump bytes number
    unsigned char *pNext = NULL;
    pNext = (unsigned char *)malloc((sizeof(unsigned char)) * patternLen);
    if(NULL == pNext)
    {
        moLogger(MOUTILS_LOGGER_MODULE_NAME, moLoggerLevelError,
            "Malloc for next table failed! size=%d, errno = %d, desc = [%s]\n",
            patternLen, errno, strerror(errno));
        //must free memory
        free(pmt);
        pmt = NULL;
        
        return MOUTILS_SEARCH_ERR_MALLOCFAILED;
    }
    genKmpNext(pNext, pmt, patternLen);

    //Start matching from the first bytes.
    unsigned char isExist = 0;
    unsigned int i = 0;
    for(i = 0; i <= srcLen - patternLen; )
    {
        unsigned int j = 0;
        for(j = 0; j < patternLen; j++)
        {
            if(*(pSrc + i + j) != *(pPattern + j))
            {
                i += pNext[j];
                break;
            }
        }
        //Find this pattern
        if(j >= patternLen)
        {
            moLogger(MOUTILS_LOGGER_MODULE_NAME, moLoggerLevelDebug,
                "pattern being find! offset of src is %d\n", i);
            isExist = 1;
            break;
        }
    }

    //Should free all resource, @pmt, @pNext
    free(pNext);
    pNext = NULL;
    free(pmt);
    pmt = NULL;

    //If find this pattern, return its pos; Or, return an errno
    if(isExist)
        return i;
    else
        return MOUTILS_SEARCH_ERR_PATNOTEXIST;
}


/*
    Generate Bad Charactor Table for BM algo.
    JumpBytes = PosInPattern - PrevPosInPattern;
*/
static void genBCT(unsigned char *pBct, const unsigned char * pPattern, const unsigned int patternLen)
{
    int i = 0;
    for(i = (int)(patternLen - 1); i >= 0; i--)
    {
        unsigned char isPrevExist = 0;
        //Find the prev pos of this charactor
        int j = 0;
        for(j = i - 1; j >=0; j--)
        {
            if(pPattern[j] == pPattern[i])
            {
                isPrevExist = 1;
                break;
            }
        }
        //Get prevPosInPattern
        unsigned int prevPos = -1;
        if(isPrevExist)
        {
            prevPos = j;
        }
        //Get JumpBytes
        pBct[i] = i - prevPos;
    }
}

/*
    Generate Good Suufix Table for BM algo.
    JumpBytes = PosInPattern - PrevPosInPattern;
*/
static void genGST(unsigned char *pGst, const unsigned char * pPattern, const unsigned int patternLen)
{
    int i = (int)(patternLen - 1);
    for(; i >= 0; i--)
    {
        //�����һ���ַ���ʼ������ֽ���ǰ��Ѱ�Һú�׺�����������λ��
        int curGoodSuffixStartPos = i;
        //���㵱ǰ�ĺú�׺�������
        int curGoodSuffixMaxLen = patternLen - curGoodSuffixStartPos;
        //�ȶ�����ȵĺú�׺��������ܹ�ͨ������ú�׺�ҵ�����λ����������������������ú�׺���д�����
        unsigned char isMaxLenMatch = 0;
        int j = i - 1;
        for(; j >= 0; j--)
        {
            //�ж��ǲ���"��һ�γ��ֵ�λ��"
            unsigned char isPrevExist = 1;
            int k = 0;
            for(k = 0; k < curGoodSuffixMaxLen; k++)
            {
                if(pPattern[j + k] == pPattern[i + k])
                    continue;
                else
                {
                    isPrevExist = 0;
                    break;
                }
            }
            //�������"��һ�γ��ֵ�λ��"��������ǰѰ��;���򣬼���ú�׺�����Ӧ�ĺ���λ���󣬴�����һ���ú�׺
            if(isPrevExist)
            {
                isMaxLenMatch = 1;
                pGst[i] = i - j;
                break;
            }
        }
        //���ͨ����ĺú�׺�Ѿ��ҵ��˺���λ���������ٶ��������ȵĺú�׺���д���
        if(isMaxLenMatch)
        {
            continue;
        }
        //�������ȵĺú�׺û���ҵ�����λ����Ҫͨ���������ȵĺú�׺Ѱ����
        unsigned char isLessLenMatch = 0;
        int tmpStartPos = curGoodSuffixStartPos;
        while((++tmpStartPos) < patternLen)
        {
            //��ǰ�ĺú�׺ʣ��ĳ���
            int curGoodSuffixLeftLen = patternLen - tmpStartPos;
            //���ļ�ͷ��ʼ��Ѱ�ҵ�ǰ���ȵĺú�׺�Ƿ��ܹ�ƥ����
            unsigned char isMatchToHead = 1;
            int k = 0;
            for(; k < curGoodSuffixLeftLen; k++)
            {
                if(pPattern[k] == pPattern[tmpStartPos + k])
                {
                    continue;
                }
                else
                {
                    isMatchToHead = 0;
                    break;
                }
            }
            //���ƥ������,�������λ��
            if(isMatchToHead)
            {
                pGst[i] = tmpStartPos;
                isLessLenMatch = 1;
                break;
            }
        }
        //����������ȵĺú�׺ƥ��ɹ��ˣ���ת����һ���ú�׺
        if(isLessLenMatch)
        {
            continue;
        }
        //������кú�׺��û��ƥ��ɹ�����һ�γ��ֵ�λ�þ���-1,����λ����������ģʽ���ĳ���
        pGst[i] = patternLen;
    }
}

/*
    Generate Bad Charactor Table;
    Generate Good Suffix Table;
    start matching.
*/
int moUtils_Search_BM(const unsigned char * pSrc, const unsigned int srcLen, 
    const unsigned char * pPattern, const unsigned int patternLen)
{
    if(NULL == pSrc || NULL == pPattern)
    {
        moLogger(MOUTILS_LOGGER_MODULE_NAME, moLoggerLevelError,
            "Input param is NULL!\n");
        return MOUTILS_SEARCH_ERR_INPUTPARAMNULL;
    }

    if(srcLen < patternLen)
    {
        moLogger(MOUTILS_LOGGER_MODULE_NAME, moLoggerLevelError,
            "Input srcLen = %d, patternLen = %d, cannot find pattern surely.\n",
            srcLen, patternLen);
        return MOUTILS_SEARCH_ERR_PATLENLARGER;
    }

    if(0 == patternLen || 0 == srcLen)
    {
        moLogger(MOUTILS_LOGGER_MODULE_NAME, moLoggerLevelError,
            "patternLen = %d, srcLen = %d, they all cannot be 0!\n",
            patternLen, srcLen);
        return MOUTILS_SEARCH_ERR_INVALIDLEN;
    }

    //generate the Bad Charactor Table
    unsigned char * pBct = NULL;
    pBct = (unsigned char *)malloc(sizeof(unsigned char) * patternLen);
    if(NULL == pBct)
    {
        moLogger(MOUTILS_LOGGER_MODULE_NAME, moLoggerLevelError,
            "Malloc for bad charactor table failed! size = %d, errno = %d, desc = [%s]\n",
            patternLen, errno, strerror(errno));
        return MOUTILS_SEARCH_ERR_MALLOCFAILED;
    }
    genBCT(pBct, pPattern, patternLen);

    //generate the Good Suffix Table
    unsigned char *pGst = NULL;
    pGst = (unsigned char *)malloc(sizeof(unsigned char) * patternLen);
    if(NULL == pGst)
    {
        moLogger(MOUTILS_LOGGER_MODULE_NAME, moLoggerLevelError,
            "Malloc for good suffix table failed! size = %d, errno = %d, desc = [%s]\n",
            patternLen, errno, strerror(errno));
        //Should free the memory being malloced by myself
        free(pBct);
        pBct = NULL;

        return MOUTILS_SEARCH_ERR_MALLOCFAILED;
    }
    genGST(pGst, pPattern, patternLen);
    
    //Start matching
    unsigned char isSearchOk = 0;
    unsigned int i = 0;
    for(i = 0; i <= srcLen - patternLen; )
    {
        unsigned char isFind = 1;
        //Start matching from end to beginning between pattern and src
        int j = 0;
        for(j = patternLen - 1; j >= 0; j++)
        {
            if(pPattern[j] == pSrc[i + j])
                continue;
            else
            {
                isFind = 0;
                break;
            }
        }
        //Find it!
        if(isFind)
        {
            isSearchOk = 1;
            break;
        }
        //Donot find it, should do jump
        unsigned char next = 0;
        //The last charactor donot match, just bad charactor table can be used
        if(j == patternLen - 1)
        {
            next = pBct[j];
        }
        else
        {
            next = (pBct[j] > pGst[j + 1]) ? pBct[j] : pGst[j + 1];
        }
        moLogger(MOUTILS_LOGGER_MODULE_NAME, moLoggerLevelDebug,
            "next = %d\n", next);
        //Jump
        i += next;
    }

    //Free all resources being malloced
    free(pGst);
    pGst = NULL;
    free(pBct);
    pBct = NULL;

    //Return the pos being find 
    if(isSearchOk)
        return i;
    else
        return MOUTILS_SEARCH_ERR_PATNOTEXIST;
}


#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include "moUtils.h"

#define DEBUG_MODE 0

static void dumpArrayInfo(const char *pArrayName, const unsigned char *pArray, const unsigned int len)
{
    printf("Dump array [%s] info start:\n\t", pArrayName);
    unsigned int i = 0;
    for(; i < len; i++)
    {
        printf("0x%04x ", *(pArray + i));
    }
    printf("\nDump array [%s] over.\n", pArrayName);
}


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

int moUtils_Search_KMP_GenNextArray(unsigned char * pNext,const unsigned char * pPattern,const unsigned int patternLen)
{
    if(NULL == pNext || NULL == pPattern)
    {
        moLogger(MOUTILS_LOGGER_MODULE_NAME, moLoggerLevelError,
            "Input param is NULL.\n");
        return MOUTILS_SEARCH_ERR_INPUTPARAMNULL;
    }

    unsigned char *pPmt = NULL;
    pPmt = (unsigned char *)malloc(sizeof(unsigned char) * patternLen);
    if(NULL == pPmt)
    {
        moLogger(MOUTILS_LOGGER_MODULE_NAME, moLoggerLevelError,
            "Malloc for PMT failed! size = %d, errno = %d, desc = [%s]\n",
            patternLen, errno, strerror(errno));
        return MOUTILS_SEARCH_ERR_MALLOCFAILED;
    }

    genPMT(pPmt, pPattern, patternLen);

    genKmpNext(pNext, pPmt, patternLen);

    free(pPmt);
    pPmt = NULL;

    return MOUTILS_SEARCH_ERR_OK;
}

/*
    Generate Partital Match Table;
    Generate next[];
    start matching.
*/
int moUtils_Search_KMP(const unsigned char * pSrc, const unsigned int srcLen, 
    const unsigned char * pPattern, const unsigned int patternLen,
    const unsigned char * pNext)
{
    if(NULL == pSrc || NULL == pPattern || NULL == pNext)
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

    //If find this pattern, return its pos; Or, return an errno
    if(isExist)
        return i;
    else
        return MOUTILS_SEARCH_ERR_PATNOTEXIST;
}


/*
    Generate Bad Charactor Table for BM algo.
    JumpBytes = PosInPattern - PrevPosInPattern;
    Just when the last charactor in pattern being not matched, BCT will be used;
    So we can save all in a table with length 256 because ASCII has 256 members.
*/
int moUtils_Search_BM_GenBCT(unsigned char * pBct,const unsigned char * pPattern,const unsigned int patternLen)
{
    if(NULL == pBct || NULL == pPattern)
    {
        moLogger(MOUTILS_LOGGER_MODULE_NAME, moLoggerLevelError,
            "Input param is NULL.\n");
        return MOUTILS_SEARCH_ERR_INPUTPARAMNULL;
    }
    
    memset(pBct, 0x00, MOUTILS_SEARCH_BM_BCT_LEN);
    int i = 0;
    for(i = 0; i < MOUTILS_SEARCH_BM_BCT_LEN; i++)
    {
        //Default, If the chactor donot exist in pattern, will jump @patternLen bytes
        pBct[i] = patternLen;
    }
    unsigned int j = 0;
    for(j = 0; j < patternLen; j++)
    {
        pBct[pPattern[j]] = patternLen - j - 1;
    }
    return MOUTILS_SEARCH_ERR_OK;
}

/*
    Generate Good Suufix Table for BM algo.
    JumpBytes = PosInPattern - PrevPosInPattern;
*/
int moUtils_Search_BM_GenGST(unsigned char *pGst, const unsigned char * pPattern, const unsigned int patternLen)
{
    if(NULL == pGst || NULL == pPattern)
    {
        moLogger(MOUTILS_LOGGER_MODULE_NAME, moLoggerLevelError,
            "Input param is NULL.\n");
        return MOUTILS_SEARCH_ERR_INPUTPARAMNULL;
    }
    
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

    return MOUTILS_SEARCH_ERR_OK;
}

/*
    Generate Bad Charactor Table;
    Generate Good Suffix Table;
    start matching.
*/
int moUtils_Search_BM(const unsigned char * pSrc, const unsigned int srcLen, 
    const unsigned char * pPattern, const unsigned int patternLen,
    const unsigned char * pBct, const unsigned char * pGst)
{
    if(NULL == pSrc || NULL == pPattern || NULL == pBct || NULL == pGst)
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
#if DEBUG_MODE
    //Dump the pattern firstly
    dumpArrayInfo("pattern", pPattern, patternLen);
#endif
    
    //Start matching
    unsigned char isSearchOk = 0;
    unsigned int i = 0;
    for(i = 0; i <= srcLen - patternLen; )
    {
        unsigned char isFind = 1;
        //Start matching from end to beginning between pattern and src
        int j = 0;
        for(j = patternLen - 1; j >= 0; j--)
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
            next = pBct[pSrc[i + j]];
        }
        //Donot the last charctor of pattern, good suffix table being used.
        else
        {
            next = pGst[j + 1];
        }
        //Jump
        i += next;
    }

    //Return the pos being find 
    if(isSearchOk)
    {
        moLogger(MOUTILS_LOGGER_MODULE_NAME, moLoggerLevelDebug,
            "BM algo find pattern in %d pos.\n", i);
        return i;
    }
    else
    {
        moLogger(MOUTILS_LOGGER_MODULE_NAME, moLoggerLevelDebug,
            "BM algo donot find pattern.\n");
        return MOUTILS_SEARCH_ERR_PATNOTEXIST;
    }
}


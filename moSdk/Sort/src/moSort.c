#include <stdio.h>
#include <string.h>

#include "moLogger.h"
#include "moSort.h"

static int directInsertSortL2B(int *pArray, const unsigned int arrayLength)
{
	unsigned int i = 0;
	for(i = 0; i < arrayLength - 1; i++)
	{
		//The node will be insert is array[i+1]
		unsigned int k = 0;
		for(k = 0; k <= i; k++)
		{
			if(pArray[i + 1] <= pArray[k])
			{
				int temp = pArray[i + 1];
				unsigned int j = 0;
				for(j = i + 1; j > k; j--)
				{
					pArray[j] = pArray[j - 1];
				}
				pArray[k] = temp;
			}
		}
	}
    return 0;
}

static int directInsertSortB2L(int *pArray, const unsigned int arrayLength)
{
	unsigned int i = 0;
	for(; i < arrayLength - 1; i++)
	{
		//The node will be insert is array[i+1]
		unsigned int k = 0;
		for(k = 0; k <= i; k++)
		{
			if(pArray[i + 1] >= pArray[k])
			{
				int temp = pArray[i + 1];
				unsigned int j = 0;
				for(j = i + 1; j > k; j--)
				{
					pArray[j] = pArray[j - 1];
				}
				pArray[k] = temp;
			}
		}
	}
    return 0;
}

/* ��Ҫ����������У�����ǰN-1�����ֶ����ź���ģ���ô����N�����֣�����������ȷ��λ�ã���ôN������Ҳ��������ģ�
 * */
int moSort_directInsertSort(int *pArray, const unsigned int arrayLength,  const MOSORT_DST_SEQUENCE seq)
{
	if(NULL == pArray)
	{
		moLoggerError(MOSORT_LOGGER_MODULE_NAME, "Input param is NULL!\n");
		return -1;
	}

    if(arrayLength <= 1)
    {
        moLoggerDebug(MOSORT_LOGGER_MODULE_NAME, "Input length of array is %d, its a sorted array!\n",
            arrayLength);
        return 0;
    }

    int ret = 0;
    switch(seq)
    {
        case MOSORT_DST_SEQUENCE_B2L:
            ret = directInsertSortB2L(pArray, arrayLength);
            break;
            
        case MOSORT_DST_SEQUENCE_L2B:
            ret = directInsertSortL2B(pArray, arrayLength);
            break;

        default:
            moLoggerError(MOSORT_LOGGER_MODULE_NAME, "Input seq = %d, cannot parse it.\n", seq);
            ret = -2;
            break;
    }
    
	return ret;
}

/*
    Select sorting, from little to Big;
*/
static int selectSortL2B(int *pArray, const unsigned int arrayLength)
{
    int i = 0;
    for(i = arrayLength - 1; i >= 1; i--)
    {
        int maxValue = pArray[i];
        int maxPos = i;
        //select the biggest one, and set to pArray[i]
        int j = 0;
        for(j = i - 1; j >= 0; j--)
        {
            if(pArray[j] > maxValue)
            {
                //bigger than maxValue, save info
                maxValue = pArray[j];
                maxPos = j;
            }
        }
        //If maxPos donot equal with the last pos currently, should change their place
        if(maxPos != i)
        {
            int tmp = pArray[i];
            pArray[i] = pArray[maxPos];
            pArray[maxPos] = tmp;
        }
    }
    return 0;
}

/*
    Select sorting, from big to little;
*/
static int selectSortB2L(int *pArray, const unsigned int arrayLength)
{
    int i = 0;
    for(i = arrayLength - 1; i >= 1; i--)
    {
        int minValue = pArray[i];
        int minPos = i;
        //select the smallest one, and set to pArray[i]
        int j = 0;
        for(j = i - 1; j >= 0; j--)
        {
            if(pArray[j] < minValue)
            {
                //bigger than maxValue, save info
                minValue = pArray[j];
                minPos = j;
            }
        }
        //If maxPos donot equal with the last pos currently, should change their place
        if(minPos != i)
        {
            int tmp = pArray[i];
            pArray[i] = pArray[minPos];
            pArray[minPos] = tmp;
        }
    }
    return 0;
}

/*
    select sorting.
*/
int moSort_selectSort(int *pArray, const unsigned int arrayLength,  const MOSORT_DST_SEQUENCE seq)
{
	if(NULL == pArray)
	{
		moLoggerError(MOSORT_LOGGER_MODULE_NAME, "Input param is NULL!\n");
		return -1;
	}

    if(arrayLength <= 1)
    {
        moLoggerDebug(MOSORT_LOGGER_MODULE_NAME, "Input length of array is %d, its a sorted array!\n",
            arrayLength);
        return 0;
    }

    int ret = 0;
    switch(seq)
    {
        case MOSORT_DST_SEQUENCE_B2L:
            ret = selectSortB2L(pArray, arrayLength);
            break;
            
        case MOSORT_DST_SEQUENCE_L2B:
            ret = selectSortL2B(pArray, arrayLength);
            break;

        default:
            moLoggerError(MOSORT_LOGGER_MODULE_NAME, "Input seq = %d, cannot parse it.\n", seq);
            ret = -2;
            break;
    }
    
	return ret;
}




/*
    Bubble sorting, from little to Big;
*/
static int bubbleSortL2B(int *pArray, const unsigned int arrayLength)
{
    int i = 0;
    for(i = arrayLength - 1; i >= 1; i--)
    {
        //From 0 to i, if find bigger one, change position
        int j = 0;
        for(j = 0; j < i; j++)
        {
            if(pArray[j] > pArray[j + 1])
            {
                int tmp = pArray[j];
                pArray[j] = pArray[j + 1];
                pArray[j + 1] = tmp;
            }
        }
    }
    return 0;
}

/*
    Bubble sorting, from big to little;
*/
static int bubbleSortB2L(int *pArray, const unsigned int arrayLength)
{
    int i = 0;
    for(i = arrayLength - 1; i >= 1; i--)
    {
        //From 0 to i, if find smaller one, change position
        int j = 0;
        for(j = 0; j < i; j++)
        {
            if(pArray[j] < pArray[j + 1])
            {
                int tmp = pArray[j];
                pArray[j] = pArray[j + 1];
                pArray[j + 1] = tmp;
            }
        }
    }
    return 0;
}


int moSort_bubbleSort(int *pArray, const unsigned int arrayLength,  const MOSORT_DST_SEQUENCE seq)
{
	if(NULL == pArray)
	{
		moLoggerError(MOSORT_LOGGER_MODULE_NAME, "Input param is NULL!\n");
		return -1;
	}

    if(arrayLength <= 1)
    {
        moLoggerDebug(MOSORT_LOGGER_MODULE_NAME, "Input length of array is %d, its a sorted array!\n",
            arrayLength);
        return 0;
    }

    int ret = 0;
    switch(seq)
    {
        case MOSORT_DST_SEQUENCE_B2L:
            ret = bubbleSortB2L(pArray, arrayLength);
            break;
            
        case MOSORT_DST_SEQUENCE_L2B:
            ret = bubbleSortL2B(pArray, arrayLength);
            break;

        default:
            moLoggerError(MOSORT_LOGGER_MODULE_NAME, "Input seq = %d, cannot parse it.\n", seq);
            ret = -2;
            break;
    }
    
	return ret;
}


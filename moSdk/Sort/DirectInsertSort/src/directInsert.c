#include <stdio.h>
#include <string.h>

#include "moLogger.h"
#include "moSort.h"
#include "directInsert.h"

static int sortL2B(int *pArray, const unsigned int arrayLength)
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

static int sortB2L(int *pArray, const unsigned int arrayLength)
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

/* 在要排序的数列中，假设前N-1个数字都是排好序的，那么将第N个数字，插入其中正确的位置，那么N个数字也将是有序的；
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
            ret = sortB2L(pArray, arrayLength);
            break;
            
        case MOSORT_DST_SEQUENCE_L2B:
            ret = sortL2B(pArray, arrayLength);
            break;

        default:
            moLoggerError(MOSORT_LOGGER_MODULE_NAME, "Input seq = %d, cannot parse it.\n", seq);
            ret = -2;
            break;
    }
    
	return ret;
}

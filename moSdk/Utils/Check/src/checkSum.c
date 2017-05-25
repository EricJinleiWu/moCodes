#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "moLogger.h"
#include "moUtils.h"

/*
    Calc Sum, set to @pSum;
*/
int moUtils_Check_getSum(const unsigned char *pSrc, const unsigned int len, 
    unsigned char *pSum)
{
    if(NULL == pSrc || 0 == len || NULL == pSum)
    {
        moLoggerError(MOUTILS_LOGGER_MODULE_NAME, "Input param is NULL.\n");
        return MOUTILS_CHECK_ERR_INPUTPARAMNULL;
    }

    *pSum = 0;
    unsigned int i = 0;
    for(; i < len; i++)
    {
        *pSum += pSrc[i];
    }

    return MOUTILS_CHECK_ERR_OK;
}

/*
    Check @sum is correct or not;
*/
int moUtils_Check_checkSum(const unsigned char *pSrc, const unsigned int len, 
    const unsigned char sum)
{
    if(NULL == pSrc || 0 == len)
    {
        moLoggerError(MOUTILS_LOGGER_MODULE_NAME, "Input param is NULL.\n");
        return MOUTILS_CHECK_ERR_INPUTPARAMNULL;
    }

    unsigned char rightSum = 0;
    moUtils_Check_getSum(pSrc, len, &rightSum);

    if(rightSum != sum)
    {
        moLoggerWarn(MOUTILS_LOGGER_MODULE_NAME, "check sum failed! right sum = %d, input sum = %d\n",
            rightSum, sum);
        return MOUTILS_CHECK_ERR_CHECKSUMFAILED;
    }
    else
    {
        return MOUTILS_CHECK_ERR_OK;
    }
}





/*
    Algorithm : MD5.
    Rules of this algorithm wrote in ../doc.
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

#include "moCrypt.h"
#include "utils.h"

typedef struct 
{
    unsigned int state[4];
}STATES;

typedef struct
{
    //The bits number of orginal string, this should be used when append at the last 16bits;
    unsigned long long int orgBitsNum;
    //The units number(512bits is a unit in MD5 algo.) of original string
    unsigned long long int orgUnitsNum;
    //Should append in MD5 algo., we should know how many units being appended at the end of this string
    unsigned char appendUnitsNum;
    //A pointer which point to the string
    unsigned char *pOrgUnit;
    //A pointer which point to the units we append at the end
    unsigned char *pAppendUnit;
    //The four states which save md5 value.
    STATES states;  //A,B,C,D, this will convert to result at last
}STR_INFO;

/*
    pStart is the start position of a unit, will do a update to the following 512bits data.
*/
static void updateUnitStates(const unsigned char *pStart, STATES * pState)
{
    //Seprate the units to sub
    unsigned int M[16] = {0x00};
    int i = 0;
    for(i = 0; i < 16; i++)
    {
        M[i] = ( ( (unsigned int)(*(pStart + i * 4 + 0)) ) << 0 ) + 
            ( ( (unsigned int)(*(pStart + i * 4 + 1)) ) << 8 ) +
            ( ( (unsigned int)(*(pStart + i * 4 + 2)) ) << 16 ) +
            ( ( (unsigned int)(*(pStart + i * 4 + 3)) ) << 24 ) ;
    }
#if DEBUG_MODE
    printf("pStart info is : \n");
    for(i = 0; i < 64; i++)
    {
        printf("0x%02x, ", *(pStart + i));
    }
    printf("\n");

    printf("M info is : \n");
    for(i = 0; i < 16; i++)
    {
        printf("0x%x, ", M[i]);
    }
    printf("\n");
#endif

    unsigned int a = pState->state[0];
    unsigned int b = pState->state[1];
    unsigned int c = pState->state[2];
    unsigned int d = pState->state[3];

    //The first loop    
    FF(a, b, c, d, M[0], S11, 0xd76aa478L);
    FF(d, a, b, c, M[1], S12, 0xe8c7b756L);
    FF(c, d, a, b, M[2], S13, 0x242070dbL);
    FF(b, c, d, a, M[3], S14, 0xc1bdceeeL);
    FF(a, b, c, d, M[4], S11, 0xf57c0fafL);
    FF(d, a, b, c, M[5], S12, 0x4787c62aL);
    FF(c, d, a, b, M[6], S13, 0xa8304613L);
    FF(b, c, d, a, M[7], S14, 0xfd469501L);
    FF(a, b, c, d, M[8], S11, 0x698098d8L);
    FF(d, a, b, c, M[9], S12, 0x8b44f7afL);
    FF(c, d, a, b, M[10], S13, 0xffff5bb1L);
    FF(b, c, d, a, M[11], S14, 0x895cd7beL);
    FF(a, b, c, d, M[12], S11, 0x6b901122L);
    FF(d, a, b, c, M[13], S12, 0xfd987193L);
    FF(c, d, a, b, M[14], S13, 0xa679438eL);
    FF(b, c, d, a, M[15], S14, 0x49b40821L);
    //The second loop
    GG(a, b, c, d, M[1], S21, 0xf61e2562L);
    GG(d, a, b, c, M[6], S22, 0xc040b340L);
    GG(c, d, a, b, M[11], S23, 0x265e5a51L);
    GG(b, c, d, a, M[0], S24, 0xe9b6c7aaL);
    GG(a, b, c, d, M[5], S21, 0xd62f105dL);
    GG(d, a, b, c, M[10], S22, 0x02441453L);
    GG(c, d, a, b, M[15], S23, 0xd8a1e681L);
    GG(b, c, d, a, M[4], S24, 0xe7d3fbc8L);
    GG(a, b, c, d, M[9], S21, 0x21e1cde6L);
    GG(d, a, b, c, M[14], S22, 0xc33707d6L);
    GG(c, d, a, b, M[3], S23, 0xf4d50d87L);
    GG(b, c, d, a, M[8], S24, 0x455a14edL);
    GG(a, b, c, d, M[13], S21, 0xa9e3e905L);
    GG(d, a, b, c, M[2], S22, 0xfcefa3f8L);
    GG(c, d, a, b, M[7], S23, 0x676f02d9L);
    GG(b, c, d, a, M[12], S24, 0x8d2a4c8aL);
    //The third loop    
    HH(a, b, c, d, M[5], S31, 0xfffa3942L);
    HH(d, a, b, c, M[8], S32, 0x8771f681L);
    HH(c, d, a, b, M[11], S33, 0x6d9d6122L);
    HH(b, c, d, a, M[14], S34, 0xfde5380cL);
    HH(a, b, c, d, M[1], S31, 0xa4beea44L);
    HH(d, a, b, c, M[4], S32, 0x4bdecfa9L);
    HH(c, d, a, b, M[7], S33, 0xf6bb4b60L);
    HH(b, c, d, a, M[10], S34, 0xbebfbc70L);
    HH(a, b, c, d, M[13], S31, 0x289b7ec6L);
    HH(d, a, b, c, M[0], S32, 0xeaa127faL);
    HH(c, d, a, b, M[3], S33, 0xd4ef3085L);
    HH(b, c, d, a, M[6], S34, 0x04881d05L);
    HH(a, b, c, d, M[9], S31, 0xd9d4d039L);
    HH(d, a, b, c, M[12], S32, 0xe6db99e5L);
    HH(c, d, a, b, M[15], S33, 0x1fa27cf8L);
    HH(b, c, d, a, M[2], S34, 0xc4ac5665L);
    //The fourth loop    
    II(a, b, c, d, M[0], S41, 0xf4292244L);
    II(d, a, b, c, M[7], S42, 0x432aff97L);
    II(c, d, a, b, M[14], S43, 0xab9423a7L);
    II(b, c, d, a, M[5], S44, 0xfc93a039L);
    II(a, b, c, d, M[12], S41, 0x655b59c3L);
    II(d, a, b, c, M[3], S42, 0x8f0ccc92L);
    II(c, d, a, b, M[10], S43, 0xffeff47dL);
    II(b, c, d, a, M[1], S44, 0x85845dd1L);
    II(a, b, c, d, M[8], S41, 0x6fa87e4fL);
    II(d, a, b, c, M[15], S42, 0xfe2ce6e0L);
    II(c, d, a, b, M[6], S43, 0xa3014314L);
    II(b, c, d, a, M[13], S44, 0x4e0811a1L);
    II(a, b, c, d, M[4], S41, 0xf7537e82L);
    II(d, a, b, c, M[11],S42, 0xbd3af235L);
    II(c, d, a, b, M[2], S43, 0x2ad7d2bbL);
    II(b, c, d, a, M[9], S44, 0xeb86d391L);

    //Refresh the states value
    pState->state[0] += a;
    pState->state[1] += b;
    pState->state[2] += c;
    pState->state[3] += d;
}

static void final(const STATES * pState, MO_MD5_VALUE * pValue)
{
    //convert states to value
    int i = 0;
    for(i = 0; i < 16; i++)
    {
        pValue->md5[i] = (char )( pState->state[i / 4] >> ( (i % 4) * 8 ) );
    }
#if DEBUG_MODE
    printf("pState value is : \n");
    for(i = 0; i < 4; i++)
    {
        printf("0x%x, ", pState->state[i]);
    }
    printf("\n");
    
    printf("pValue value is : \n");
    for(i = 0; i < 16; i++)
    {
        printf("0x%02x, ", pValue->md5[i]);
    }
    printf("\n");
#endif
}

static void initStrInfo(STR_INFO * pInfo)
{
    pInfo->orgBitsNum = 0;
    pInfo->orgUnitsNum = 0;
    pInfo->appendUnitsNum = 0;
    pInfo->pOrgUnit = NULL;
    pInfo->pAppendUnit = NULL;
    pInfo->states.state[0] = A_INIT_VALUE;
    pInfo->states.state[1] = B_INIT_VALUE;
    pInfo->states.state[2] = C_INIT_VALUE;
    pInfo->states.state[3] = D_INIT_VALUE;
}

static void unInitStrInfo(STR_INFO * pInfo)
{
    //This pointer is malloced, so we must free it, or memory leak is appeared.
    if(pInfo->pAppendUnit != NULL)
    {
        free(pInfo->pAppendUnit);
        pInfo->pAppendUnit = NULL;
    }
}

/*
    get orginal string length(in bits);
    get orginal string units(512bits is a unit) number;
    get append units number;
    malloc appendMemory;
    set append contents to appendMemory: include 0x8000.... and length;
    refresh pInfo elements;
*/
static int appendCont2Str(const unsigned char *pCont, STR_INFO *pInfo)
{
    if(NULL == pCont || NULL == pInfo)
    {
        error("Input param is NULL!\n");
        return MOCRYPTMD5_ERR_INPUTNULL;
    }

    //Init elements 
    int orgStrLen = strlen((char *)pCont);  //'\0' is invalid when do md5, 
    pInfo->orgBitsNum = (orgStrLen << 3);    //convert from bytes to bits
    pInfo->orgUnitsNum = orgStrLen / UNIT_LEN_BYTES;  //The last bytes in the last unit will be set to appendUnits
    pInfo->pOrgUnit = pCont;
    debug("pCont = [%s], orgStrLen = %d, pInfo->orgBitsNum = %lld, pInfo->orgUnitsNum = %lld\n",
        pCont, orgStrLen, pInfo->orgBitsNum, pInfo->orgUnitsNum);

    //Appending
    unsigned int remainderBytesNum = orgStrLen % UNIT_LEN_BYTES;
    debug("remainderBytesNum = %d\n", remainderBytesNum);
    if(remainderBytesNum < MOD_LEN_BYTES)
    {
        //Just need append to 1 unit, 0x8000... and the length will be set in this unit together.
        pInfo->appendUnitsNum = 1;
    }
    else
    {
        //Need append to 2 units
        pInfo->appendUnitsNum = 2;
    }
    debug("pInfo->appendUnitsNum = %d\n", pInfo->appendUnitsNum);

    //Malloc for append unit    
    pInfo->pAppendUnit = (unsigned char *)malloc((sizeof(char)) * (pInfo->appendUnitsNum * UNIT_LEN_BYTES));
    if(NULL == pInfo->pAppendUnit)
    {
        error("malloc failed! errno = %d, desc = [%s]\n", errno, strerror(errno));
        return MOCRYPTMD5_ERR_MALLOCFAIL;
    }
    memset(pInfo->pAppendUnit, 0x00, pInfo->appendUnitsNum * UNIT_LEN_BYTES);

    //Copy the last units data to this unit firstly
    int orgLastUnitDataBytes = orgStrLen % UNIT_LEN_BYTES;
    debug("orgLastUnitDataBytes = %d\n", orgLastUnitDataBytes);
    memcpy(pInfo->pAppendUnit, pCont + (pInfo->orgUnitsNum * UNIT_LEN_BYTES),
        orgLastUnitDataBytes);

    //append 0x8000... to this units secondly
    *(pInfo->pAppendUnit + orgLastUnitDataBytes) = 0x80;

    //append length to this unit finally
    memcpy(pInfo->pAppendUnit + (pInfo->appendUnitsNum * UNIT_LEN_BYTES) - sizeof(pInfo->orgBitsNum),
        &(pInfo->orgBitsNum), sizeof(pInfo->orgBitsNum));

#if DEBUG_MODE
    //dump the append unit info to check right or not, should delete this when release
    printf("\n");
    int i = 0;
    for(i = 0; i < pInfo->appendUnitsNum * UNIT_LEN_BYTES; i++)
    {
        printf("0x%02x, ", *(pInfo->pAppendUnit + i));
    }
    printf("i = %d\n", i);
#endif

    return MOCRYPTMD5_ERR_OK;
}

/*
    Two parts: Org string pointer point to the first part, append string pointer point to the second one.
*/
static void updateStrStates(STR_INFO * pInfo)
{
    //Do state update for original string
    unsigned int i = 0;
    for(i = 0; i < pInfo->orgUnitsNum; i++)
    {
        updateUnitStates(pInfo->pOrgUnit + i * UNIT_LEN_BYTES, &(pInfo->states));
    }

    //Do state update for append string
    for(i = 0; i < pInfo->appendUnitsNum; i++)
    {
        updateUnitStates(pInfo->pAppendUnit + i * UNIT_LEN_BYTES, &(pInfo->states));
    }
    
}

int moCrypt_MD5_String(const unsigned char *pCont, MO_MD5_VALUE *pValue)
{
    if(NULL == pCont || NULL == pValue)
    {
        error("Input param is NULL!\n");
        return MOCRYPTMD5_ERR_INPUTNULL;
    }

    //Init the info which we will used below.
    STR_INFO info;
    memset(&info, 0x00, sizeof(STR_INFO));
    initStrInfo(&info);

    //1.get the original length of @pCont, '\0' being calculated, too. In bits!!!
    //2.append 0x8000.... to @pCont, and get unitsNum after appending all.
    int ret = appendCont2Str(pCont, &info);
    if(ret != 0)
    {
        error("Append content failed! ret = %d.\n", ret);
        return ret;
    }

    //looply to do update
    updateStrStates(&info);

    //get final md5 hash value
    final(&(info.states), pValue);

    //Do uninit is necessary, because we have pAppendUnit is malloced.
    unInitStrInfo(&info);

    return MOCRYPTMD5_ERR_OK;
}

typedef struct
{
    //The file descriptor for source file
    FILE *pFd;
    //current unit read from source file
    unsigned char curUnitData[UNIT_LEN_BYTES];
    //The complete units in this file
    unsigned long long int orgCompleteUnitsNum;
    //The bytes number in the last unit of this file
    unsigned char lastUnitBytesNum;
    //The memory being malloced when we append info to the end of file
    unsigned char *pAppendUnits;
    //states
    STATES states;
}FILE_INFO;

/*
    Check input @pFilepath is valid or not;
    If pFilepath is NULL, or point to a directory, or cannot open for read, will return error;
*/
static int checkFile(const char * pFilepath)
{
    if(NULL == pFilepath)
    {
        error("Input param is NULL!\n");
        return MOCRYPTMD5_ERR_INPUTNULL;
    }

    struct stat curStat;
    memset(&curStat, 0x00, sizeof(struct stat));
    int ret = stat(pFilepath, &curStat);
    if(ret != 0)
    {
        error("File [%s] donot exist!\n", pFilepath);
        return MOCRYPTMD5_ERR_FILENOTEXIST;
    }

    if(!S_ISREG(curStat.st_mode) && !S_ISLNK(curStat.st_mode))
    {
        error("File [%s] donot a regular file or a link file, cannot being deal with currently.\n",
            pFilepath);
        return MOCRYPTMD5_ERR_WRONGFILEMODE;
    }

    debug("File [%s] is valid, can do md5 now.\n", pFilepath);
    return MOCRYPTMD5_ERR_OK;
}

static int initFileInfo(const char *pFilepath, FILE_INFO * pInfo)
{
    if(NULL == pFilepath || NULL == pInfo)
    {
        error("Input param is NULL!\n");
        return MOCRYPTMD5_ERR_INPUTNULL;
    }

    memset(pInfo->curUnitData, 0x00, UNIT_LEN_BYTES);
    pInfo->lastUnitBytesNum = 0;
    pInfo->orgCompleteUnitsNum = 0;
    pInfo->pAppendUnits = NULL;
    pInfo->pFd = NULL;
    pInfo->states.state[0] = A_INIT_VALUE;
    pInfo->states.state[1] = B_INIT_VALUE;
    pInfo->states.state[2] = C_INIT_VALUE;
    pInfo->states.state[3] = D_INIT_VALUE;

    pInfo->pFd = fopen(pFilepath, "rb+");
    if(NULL == pInfo->pFd)
    {
        error("Open file [%s] for read failed! errno = %d, desc = [%s]\n",
            pFilepath, errno, strerror(errno));
        return MOCRYPTMD5_ERR_OPENFILEFAIL;
    }
    
    return MOCRYPTMD5_ERR_OK;
}

static void unInitFileInfo(FILE_INFO * pInfo)
{
    if(pInfo->pFd != NULL)
    {
        fclose(pInfo->pFd);
        pInfo->pFd = NULL;
    }

    if(pInfo->pAppendUnits != NULL)
    {
        free(pInfo->pAppendUnits);
        pInfo->pAppendUnits = NULL;
    }
}

/*
    Read unit from file looply, and update each unit sepratorly;
    states will be freshed by each unit;
*/
static int updateFileStates(FILE_INFO *pInfo)
{
    int readLen = 0;
    while((readLen = fread(pInfo->curUnitData, 1, UNIT_LEN_BYTES, pInfo->pFd)) == UNIT_LEN_BYTES)
    {
        //read 64bytes data, this is a complete unit, we record it, because we will calc length at the last unit when appending
        pInfo->orgCompleteUnitsNum++;

        //Do update states with this unit
        updateUnitStates(pInfo->curUnitData, &(pInfo->states));

        //reset
        memset(pInfo->curUnitData, 0x00, UNIT_LEN_BYTES);
    }

    //Should deal with the last data in the last unit
    int appendUnitNum = 2;
    pInfo->lastUnitBytesNum = readLen;
    //If less than 56bytes, append to 1 unit, or, 2units being used;
    if(pInfo->lastUnitBytesNum < MOD_LEN_BYTES)
    {
        appendUnitNum = 1;
    }
    debug("pInfo->lastUnitBytesNum = %d, appendUnitNum = %d\n", pInfo->lastUnitBytesNum, appendUnitNum);

    //get memory 
    pInfo->pAppendUnits = (unsigned char *)malloc(sizeof(unsigned char ) * (appendUnitNum * UNIT_LEN_BYTES));
    if(NULL == pInfo->pAppendUnits)
    {
        error("malloc failed! errno = %d, desc = [%s]\n", errno, strerror(errno));
        return MOCRYPTMD5_ERR_MALLOCFAIL;
    }
    memset(pInfo->pAppendUnits, 0x00, appendUnitNum * UNIT_LEN_BYTES);

    //copy the last unit data to append unit
    memcpy(pInfo->pAppendUnits, pInfo->curUnitData, pInfo->lastUnitBytesNum);
    //append 0x8000... to append units then
    *(pInfo->pAppendUnits + pInfo->lastUnitBytesNum) = 0x80;
    //set length to append unit
    unsigned long long int orgBitsNum = 
        ( ((pInfo->orgCompleteUnitsNum * UNIT_LEN_BYTES) + pInfo->lastUnitBytesNum) << 3 );
    memcpy(pInfo->pAppendUnits + (appendUnitNum * UNIT_LEN_BYTES) - sizeof(orgBitsNum),
        &orgBitsNum, sizeof(orgBitsNum));

    //Do update to the last units
    int i = 0;
    for(i = 0; i < appendUnitNum; i++)
    {
        updateUnitStates(pInfo->pAppendUnits + i * UNIT_LEN_BYTES, &(pInfo->states));
    }

    return MOCRYPTMD5_ERR_OK;
}

/*
    1.Check input file valid or not;
    2.read each 512 bits data from file, and do MD5 update for this unit;
    3.when goto the last unit, should do appending!
    4.updating MD5 for the last units(maybe one unit or two units);
    5.convert states to value;
*/
int moCrypt_MD5_File(const char *pSrcFilepath, MO_MD5_VALUE *pValue)
{
    int ret = checkFile(pSrcFilepath);
    if(ret != MOCRYPTMD5_ERR_OK)
    {
        error("checkFile failed! ret = 0x%x, srcfilepath = [%s]\n", ret, pSrcFilepath);
        return ret;
    }

    //Open file for read, and init states and other elements firstly.
    FILE_INFO info;
    memset(&info, 0x00, sizeof(FILE_INFO));
    ret = initFileInfo(pSrcFilepath, &info);
    if(ret != MOCRYPTMD5_ERR_OK)
    {
        error("initFileInfo failed! ret = 0x%x, srcFilepath = [%s]\n", ret, pSrcFilepath);
        unInitFileInfo(&info);
        return ret;
    }

    //Update states info, looply read unit from file, and update looply
    ret = updateFileStates(&info);
    if(ret != MOCRYPTMD5_ERR_OK)
    {
        error("updateFileStates failed! ret = 0x%x\n", ret);
        unInitFileInfo(&info);
        return ret;
    }

    //convert states to md5 value here.
    final(&(info.states), pValue);

    //free memory and other uninit operations
    unInitFileInfo(&info);
    
    return 0;
}

void moCrypt_MD5_dump(const MO_MD5_VALUE *pValue)
{
    int i = 0;
    for(i = 0; i < 16; i++)
    {
        printf("%02x", pValue->md5[i]);
    }
    printf("\n");
}




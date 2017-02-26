#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>

#include "moIniParser.h"

#define FILEPATH_MAX_LEN			256
#define MAX_LINE_LEN				512

/* ASCII value for several symbols*/
#define TABLE_SYMBOL			9	//Table
#define SPACE_SYMBOL			32	//Space
#define SEMICOLON_SYMBOL		59	//;
#define BRACKET_LEFT_SYMBOL		91	//[
#define BRACKET_RIGHT_SYMBOL	93	//]
#define EQUAL_SYMBOL			61	//=

#define trace(format, ...) printf("[%s, %d] : "format"\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define debug(format, ...) printf("[%s, %d] : "format"\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define info(format, ...) printf("[%s, %d] : "format"\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define warn(format, ...) printf("[%s, %d] : "format"\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define error(format, ...) printf("[%s, %d] : "format"\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define fatal(format, ...) printf("[%s, %d] : "format"\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)

typedef enum
{
	LINE_ERROR = 0,
	LINE_SECTION = 1,
	LINE_ATTR = 2,
	LINE_COMMENT = 3,
	LINE_SPACE = 4
}LINE_TYPE;

typedef enum
{
	TYPE_INVALID,		/* Table or space */
	TYPE_SEMICOLON,		/* ; */
	TYPE_LEFT_BRACKET,		/* [ */
	TYPE_OTHER_VALID_CHAR		/* other valid charactors */
}FIRST_VALID_CHAR_TYPE;

/* *************************** Operation for section list and attribute list ***************************/

/* Do init for pSecInfoHeadNode, malloc for it*/
static SECTION_INFO_NODE * InitSecInfoList(void)
{
    SECTION_INFO_NODE *pSecInfoHeadNode = NULL;
	pSecInfoHeadNode = (SECTION_INFO_NODE *)malloc(sizeof(SECTION_INFO_NODE));
	if(NULL == pSecInfoHeadNode)
	{
		error("Malloc for pSecInfoHeadNode failed!\n");
		return NULL;
	}
	pSecInfoHeadNode->pHeadAttrNode = (ATTR_INFO_NODE *)malloc(sizeof(ATTR_INFO_NODE));
	if(NULL == pSecInfoHeadNode->pHeadAttrNode)
	{
		error("Malloc for the head of pSecInfoHeadNode->pHeadAttrNode failed!\n");
        free(pSecInfoHeadNode);
        pSecInfoHeadNode = NULL;
		return NULL;
	}
	//These two are unuseable for pSecInfoHeadNode
	memset(pSecInfoHeadNode->sectionName, 0x00, SECTION_NAME_MAX_LEN);
	memset(pSecInfoHeadNode->pHeadAttrNode, 0x00, sizeof(ATTR_INFO_NODE));
	pSecInfoHeadNode->next = NULL;

	return pSecInfoHeadNode;
}

/* Free all resource being malloced, should looply find all of the section node, and find all
 * of the attribute node in it, and free all of them;
 * */
static void UnInitSecInfoList(SECTION_INFO_NODE *pSecInfoHeadNode)
{
	SECTION_INFO_NODE *pCurSecInfoNode = pSecInfoHeadNode->next;
	while(pCurSecInfoNode != NULL)
	{
		ATTR_INFO_NODE *pCurAttrInfoNode = pCurSecInfoNode->pHeadAttrNode->next;
		while(pCurAttrInfoNode != NULL)
		{
			//Move the pointer of head to the next node, and release current node;
			pCurSecInfoNode->pHeadAttrNode->next = pCurAttrInfoNode->next;
			free(pCurAttrInfoNode);
			pCurAttrInfoNode = pCurSecInfoNode->pHeadAttrNode->next;
		}
		//free the head of attributes list which in this section
		free(pCurSecInfoNode->pHeadAttrNode);

		//Move the pointer to the next section, and free current section node
		pSecInfoHeadNode->next = pCurSecInfoNode->next;
		free(pCurSecInfoNode);
		pCurSecInfoNode = pSecInfoHeadNode->next;
	}
	//After all resource being released, the head node should free, too
	if(NULL != pSecInfoHeadNode->pHeadAttrNode)
	{
        free(pSecInfoHeadNode->pHeadAttrNode);
        pSecInfoHeadNode->pHeadAttrNode = NULL;
    }
	free(pSecInfoHeadNode);
	pSecInfoHeadNode = NULL;
}

/* Add pSecInfoNode to the list which being pointed with pSecInfoHeadNode
 *
 * 1.Check pSecInfoHeadNode is NULL or not:
 *		1.1.if NULL, return errno;
 *		1.2.else, step2;
 * 2.Check the input node has been exist in list or not:
 * 		2.1.if exist, return errno;
 * 		2.2.step3;
 * 3.Go to the end of pSecInfoHeadNode, and add the input node to it;
 *
 * */
static int AddSecInfoNode(const char *pSecName, const SECTION_INFO_NODE *pSecInfoHeadNode)
{
	if(NULL == pSecName)
	{
		error("Input param is NULL!\n");
		return -1;
	}

	if(NULL == pSecInfoHeadNode)
	{
		error("pSecInfoHeadNode is NULL! Do init firstly before adding section node!\n");
		return -2;
	}

	int isNodeExist = -1;
	SECTION_INFO_NODE *pLastSecInfoNode = pSecInfoHeadNode;

	while(pLastSecInfoNode->next != NULL)
	{
		if(0 == strcmp(pLastSecInfoNode->sectionName, pSecName))
		{
			isNodeExist = 0;
			break;
		}
		pLastSecInfoNode = pLastSecInfoNode->next;
	}
	if(0 == isNodeExist)
	{
		error("Input section node (sectionname=[%s]) has been exist in section list!"
				"Cannot being add again!\n", pSecName);
		return -3;
	}
	//Malloc memory for this new section node
	SECTION_INFO_NODE *pNewSecInfoNode = NULL;
	pNewSecInfoNode = (SECTION_INFO_NODE *)malloc(sizeof(SECTION_INFO_NODE));
	if(NULL == pNewSecInfoNode)
	{
		error("Malloc for the new section(name=[%s]) failed!"
			  "Cannot being added!\n", pSecName);
		return -4;
	}
	pNewSecInfoNode->pHeadAttrNode = (ATTR_INFO_NODE *)malloc(sizeof(ATTR_INFO_NODE));
	if(NULL == pNewSecInfoNode->pHeadAttrNode)
	{
        free(pNewSecInfoNode);
        pNewSecInfoNode = NULL;
		error("Malloc for the head of section(name=[%s]) failed!"
				"Cannot being added!\n", pSecName);
		return -5;
	}

	pNewSecInfoNode->next = NULL;
	strncpy(&pNewSecInfoNode->sectionName, pSecName, SECTION_NAME_MAX_LEN);
	memset(pNewSecInfoNode->pHeadAttrNode, 0x00, sizeof(ATTR_INFO_NODE));

	//Add the new node to the section node list
	pLastSecInfoNode->next = pNewSecInfoNode;

	return 0;
}

/* Add the attr info to the special section node
 * 		1.If input section info node does not exist in section list, return errno;
 * 		2.If input attr info node has been exist in this section, return errno;
 * 		3.Gen the node for the attribute, and add this node to section;
 * */
static int AddAttrInfoNode(const ATTR_INFO *pAttrInfo, const char *pSecName, const SECTION_INFO_NODE *pSecInfoHeadNode)
{
	if(NULL == pAttrInfo || NULL == pSecName)
	{
		error("Input param is NULL!\n");
		return -1;
	}

	if(NULL == pSecInfoHeadNode)
	{
		error("pSecInfoHeadNode is NULL, cannot add attribute to any section!\n");
		return -2;
	}

	//Depend on the pSecName, find the section node in global section node list
	int isFindSection = -1;
	SECTION_INFO_NODE *pTargetSecNode = pSecInfoHeadNode->next;
	while(pTargetSecNode != NULL)
	{
		if(0 == strcmp(pSecName, pTargetSecNode->sectionName))
		{
			isFindSection = 0;
			break;
		}
		pTargetSecNode = pTargetSecNode->next;
	}
	if(0 != isFindSection)
	{
		error("The section(name=[%s]) donot being found in gSecInfoNodeList!"
				"Cannot add attribute to it surely.\n", pSecName);
		return -3;
	}
	//Add input attribute node to the section being found
	int isFindAttr = -1;
	ATTR_INFO_NODE *pLastAttrInfoNode = pTargetSecNode->pHeadAttrNode;

	while(pLastAttrInfoNode->next != NULL)
	{
		pLastAttrInfoNode = pLastAttrInfoNode->next;
		if(0 == strcmp(pLastAttrInfoNode->attrInfo.key, pAttrInfo->key))
		{
			isFindAttr = 0;
			break;
		}
	}
	if(0 == isFindAttr)
	{
		error("Attr node has been exist in section [%s], cannot add it again!\n",
				__FUNCTION__, __LINE__, pSecName);
		return -4;
	}
	//Malloc a new node for the attribute
	ATTR_INFO_NODE *pNewAttrInfoNode = NULL;
	pNewAttrInfoNode = (ATTR_INFO_NODE *)malloc(sizeof(ATTR_INFO_NODE));
	if(NULL == pNewAttrInfoNode)
	{
		error("Malloc for new attribute failed!\n");
		return -5;
	}
	memcpy(&pNewAttrInfoNode->attrInfo, pAttrInfo, sizeof(ATTR_INFO));
	pNewAttrInfoNode->next = NULL;
	//Add this new node to the end of section list
	pLastAttrInfoNode->next = pNewAttrInfoNode;

	return 0;
}



/* Return :
 * 		0 & 0+ : The position of first valid charactor;
 * 		0- : error;
 * */
static int GetFirstValidCharType(FIRST_VALID_CHAR_TYPE *type, const char *line)
{
	if(NULL == line)
	{
		error("Input line is NULL!\n");
		return -1;
	}
	int ret = 0;
	char firstValidChar = 0x00;
	for(ret = 0; *(line + ret) != 0x00; ret++)
	{
		if(*(line + ret) == TABLE_SYMBOL || *(line + ret) == SPACE_SYMBOL)
		{
			continue;
		}
		firstValidChar = *(line + ret);
		break;
	}
	if(SEMICOLON_SYMBOL == firstValidChar)
	{
		*type = TYPE_SEMICOLON;
	}
	else if(BRACKET_LEFT_SYMBOL == firstValidChar)
	{
		*type = TYPE_LEFT_BRACKET;
	}
	else if(0x00 == firstValidChar)
	{
		*type = TYPE_INVALID;
	}
	else
	{
		*type = TYPE_OTHER_VALID_CHAR;
	}
	return ret;
}

/* Last valid charactor must be ']', or its not in right format
 * return :
 * 		0:sectionName being get;
 * 		-1:input error;
 * 		-2:valid charactor appeared after ']'(line do not ends with ']' before comment);
 * 		-3:sectionName length error;
 * */
static int GetSectionNameFromLine(char *sectionName, char *line, const unsigned int firstValidCharPos)
{
	if(NULL == sectionName || NULL == line)
	{
		error("Input param is NULL!\n");
		return -1;
	}
	char validLineInfo[MAX_LINE_LEN] = {0x00};
	//Get valid content from line, delete comments
	char *firstSemicolonPos = NULL;
	firstSemicolonPos = strchr(line, SEMICOLON_SYMBOL);
	if(NULL != firstSemicolonPos)
	{
		strncpy(validLineInfo, line + firstValidCharPos, firstSemicolonPos - line - firstValidCharPos);
	}
	else
	{
		strcpy(validLineInfo, line + firstValidCharPos);
	}
	//Check ] exist in this line or not
	char *lastRightBracketPos = NULL;
	lastRightBracketPos = strrchr(validLineInfo, BRACKET_RIGHT_SYMBOL);
	if(NULL == lastRightBracketPos)
	{
		error("line [%s] donot find symbol ], not a right format section line.\n", line);
		return -2;
	}
	int i = 1;
	for(; *(lastRightBracketPos + i) != 0x00; i++)
	{
		if(*(lastRightBracketPos + i) != TABLE_SYMBOL && *(lastRightBracketPos + i) != SPACE_SYMBOL)
		{
			error("In line [%s], after ], valid charactor [%c] appeared! Its not a right format section!",
					line, *(lastRightBracketPos + i));
			return -2;
		}
	}
	//Get sectionName
	char tempSectionName[MAX_LINE_LEN];
	strncpy(tempSectionName, validLineInfo + 1, lastRightBracketPos - validLineInfo - 1);
	tempSectionName[lastRightBracketPos - validLineInfo - 1] = 0x00;
	if(strlen(tempSectionName) >= SECTION_NAME_MAX_LEN)
	{
		error("line [%s] has section [%s], its length [%d], larger than SECTION_NAME_MAX_LEN(%d)\n",
			  line, tempSectionName, strlen(tempSectionName), SECTION_NAME_MAX_LEN);
		return -3;
	}
	strcpy(sectionName, tempSectionName);
	return 0;
}

/* Get key and value from this line
 * return :
 * 		0 : Get ok, key and value are valid;
 * 		-1 : input error;
 * 		-2 : donot contain '=' in this line;
 * 		-3 : More than 1 '=' in this line;
 * 		-4 : Key length not right;
 * 		-5 : Value length not right;
 * */
static int GetAttrFromLine(char *key, char *value, char *line, const unsigned int firstValidCharPos)
{
	if(NULL == key || NULL == value || NULL == line)
	{
		error("Input param is NULL!\n");
		return -1;
	}
	char validLineInfo[MAX_LINE_LEN] = {0x00};
	//Get valid content from line, delete comments
	char *firstSemicolonPos = NULL;
	firstSemicolonPos = strchr(line, SEMICOLON_SYMBOL);
	if(NULL != firstSemicolonPos)
	{
		strncpy(validLineInfo, line + firstValidCharPos, firstSemicolonPos - line - firstValidCharPos);
	}
	else
	{
		strcpy(validLineInfo,  line + firstValidCharPos);
	}
	//Get number of '=' in valid line
	char *firstEqualPos = strchr(validLineInfo, EQUAL_SYMBOL);
	if(NULL == firstEqualPos)
	{
		error("In line [%s], no valid equal symbol appeared, not an attribute line!\n", line);
		return -2;
	}
	char *lastEqualPos = strrchr(validLineInfo, EQUAL_SYMBOL);
	if(firstEqualPos != lastEqualPos)
	{
		error("In line [%s], more than 1 equal appeared, not an attribute line!\n", line);
		return -3;
	}
	//Get key and value from line
	char tempKey[MAX_LINE_LEN] = {0x00}, tempValue[MAX_LINE_LEN] = {0x00}, temp[MAX_LINE_LEN] = {0x00};
	strncpy(tempKey, validLineInfo, firstEqualPos - validLineInfo);
	strcpy(tempValue, validLineInfo + (firstEqualPos - validLineInfo) + 1);
	//Get Valid key and value info
	int tempKeyLen = strlen(tempKey);
	int i = 0, j = 0;
	for(i = 0; i < tempKeyLen && (*(tempKey + i) == TABLE_SYMBOL || *(tempKey + i) == SPACE_SYMBOL); i++)
	{
		;
	}
	for(j = tempKeyLen - 1; j > 0 && (*(tempKey + j) == TABLE_SYMBOL || *(tempKey + j) == SPACE_SYMBOL); j--)
	{
		;
	}
	if(j < i)
	{
		error("In line [%s], Key = [%s], not valid!\n", line, tempKey);
		return -4;
	}
    memset(temp, 0x00, MAX_LINE_LEN);
    strncpy(temp, tempKey + i, j - i + 1);
    temp[j - i + 1] = 0x00;
	memcpy(tempKey, temp, MAX_LINE_LEN);
    
	int tempValueLen = strlen(tempValue);
	for(i = 0; i < tempValueLen && (*(tempValue + i) == TABLE_SYMBOL || *(tempValue + i) == SPACE_SYMBOL); i++)
	{
		;
	}
	for(j = tempValueLen - 1; j > 0 && (*(tempValue + j) == TABLE_SYMBOL || *(tempValue + j) == SPACE_SYMBOL); j--)
	{
		;
	}
	if(j < i)
	{
		error("In line [%s], Value = [%s], not valid!\n", line, tempValue);
		return -4;
	}
    memset(temp, 0x00, MAX_LINE_LEN);
    strncpy(temp, tempValue + i, j - i + 1);
    temp[j - i + 1] = 0x00;
	memcpy(tempValue, temp, MAX_LINE_LEN);
	//Check length
	tempKeyLen = strlen(tempKey);
	if(tempKeyLen <= 0 || tempKeyLen >= ATTR_KEY_MAX_LEN)
	{
		error("In line [%s], Key = [%s], length(%d) less than ATTR_KEY_MIN_LEN(1) or "
			  "larger than ATTR_KEY_MAX_LEN(%d)!\n",
			  line, tempKey, tempKeyLen, ATTR_KEY_MAX_LEN);
		return -4;
	}
	tempValueLen = strlen(tempValue);
	if(tempValueLen <= 0 || tempValueLen >= ATTR_VALUE_MAX_LEN)
	{
		error("In line [%s], Value = [%s], length(%d) less than ATTR_KEY_MIN_LEN(1) or "
			  " larger than ATTR_VALUE_MAX_LEN(%d)!\n",
			  line, tempValue, tempValueLen, ATTR_VALUE_MAX_LEN);
		return -5;
	}
	//Get key and value
	strcpy(key, tempKey);
	strcpy(value, tempValue);

	return 0;
}

/* 1.Check first valid charactor:
 *		1.1. ';' : comment line, return LINE_COMMENT;
 *		1.2. '[' : maybe section line, step2;
 *		1.3. Other valid charactor : maybe attribute line, step3;
 *		1.4. No valid charactor in this line : NULL space line, return LINE_SPACE;
 * 2. Get ']' in this line:
 * 		2.1. get it, check length:
 * 			2.1.1.length in [1, SECTION_NAME_MAX_LEN] : section line, return LINE_SECTION;
 * 			2.1.2.length not in [1, SECTION_NAME_MAX_LEN] : error line, return LINE_ERROR;
 * 		2.2.not get it : error line, return LINE_ERROR;
 * 3.Get '=' in this line:
 * 		3.1.get key and value from it:
 * 			3.1.1.key_length in [1, ATTR_KEY_MAX_LEN], and value length in [1, ATTR_VALUE_MAX_LEN] : attr line, return LINE_ATTR;
 * 			3.1.2.length not right : error line, return LINE_ERROR;
 * 		3.2.donot find '=' : error line, return LINE_ERROR;
 * */
static LINE_TYPE GetLineType(char *sectionName, char *key, char *value, char *line)
{
	LINE_TYPE type = LINE_ERROR;

	FIRST_VALID_CHAR_TYPE firstValidCharType;
	int firstValidCharPos = GetFirstValidCharType(&firstValidCharType, line);
	if(0 > firstValidCharPos)
	{
		error("GetFirstValidCharType failed, ret = %d.\n", firstValidCharPos);
	}
	else if(firstValidCharType == TYPE_INVALID)		//null line, no valid charactors
	{
		type = LINE_SPACE;
	}
	else if(firstValidCharType == TYPE_SEMICOLON)	//comment
	{
		type = LINE_COMMENT;
	}
	else if(firstValidCharType == TYPE_LEFT_BRACKET)	//section
	{
		int ret = GetSectionNameFromLine(sectionName, line, firstValidCharPos);
		if(0 == ret)
		{
			type = LINE_SECTION;
		}
		else
		{
			error("GetSectionNameFromLine failed, ret = %d, line = [%s].\n", ret, line);
			type = LINE_ERROR;
		}
	}
	else if(firstValidCharType == TYPE_OTHER_VALID_CHAR)	//attr
	{
		int ret = GetAttrFromLine(key, value, line, firstValidCharPos);
		if(0 == ret)
		{
			type = LINE_ATTR;
		}
		else
		{
			error("GetAttrFromLine failed, ret = %d, line = [%s].\n", ret, line);
			type = LINE_ERROR;
		}
	}

	return type;
}

/* 1.Read a line;
 * 2.Check line type :
 * 		2.1.comment: step1;
 * 		2.2.section: step3;
 * 		2.3.attr: step4;
 * 		2.4.error: break;
 * 		2.5.space: step1;
 * 3.Get section info to current local section;
 * 4.Get attr info to current local attr, and add it to section;
 * 5.loop to step1;
 * */
static int GetSectionList(FILE *fp, SECTION_INFO_NODE *pSecInfoHeadNode)
{
	if(NULL == fp)
	{
		error("Input param is NULL!\n");
		return -1;
	}
	if(NULL == pSecInfoHeadNode)
	{
		error("pSecInfoHeadNode is NULL! Do init firstly!\n");
		return -2;
	}

	int ret = 0;

	char line[MAX_LINE_LEN];
	memset(line, 0x00, MAX_LINE_LEN);
	char curSectionName[SECTION_NAME_MAX_LEN];
	memset(curSectionName, 0x00, SECTION_NAME_MAX_LEN);
	while(NULL != fgets(line, MAX_LINE_LEN, fp))
	{
		int lineLen = strlen(line);
		if(line[lineLen - 1] == '\n')
		{
			line[lineLen - 1] = '\0';
		}
		char sectionName[SECTION_NAME_MAX_LEN], key[ATTR_KEY_MAX_LEN], value[ATTR_VALUE_MAX_LEN];
		LINE_TYPE lineType = GetLineType(sectionName, key, value, line);
		switch(lineType)
		{
		case LINE_SECTION:
		{
			ret = AddSecInfoNode(sectionName, pSecInfoHeadNode);
			if(0 != ret)
			{
				error("AddSecInfoNode for section [%s] failed!\n", sectionName);
				ret = -1;
				break;
			}
			strcpy(curSectionName, sectionName);

			break;
		}
		case LINE_ATTR:
		{
			if(0 == strlen(curSectionName))
			{
				error("Find attribute which donot being contained in a section! "
					  "This is not right format ini configuration file! This attribute key=[%s], value=[%s]\n",
					  key, value);
				ret = -2;
				break;
			}

			ATTR_INFO attrInfo;
			strcpy(attrInfo.key, key);
			strcpy(attrInfo.value, value);
			ret = AddAttrInfoNode(&attrInfo, curSectionName, pSecInfoHeadNode);
			if(0 != ret)
			{
				error("AddAttrInfoNode(attrKey=[%s], attrValue=[%s]) for section [%s] failed!\n",
						attrInfo.key, attrInfo.value, sectionName);
				ret = -3;
			}
			break;
		}
		case LINE_ERROR:
		{
 			error("Parse line [%s] error, it is not a valid config file.\n", line);
			ret = -4;
			break;
		}
		case LINE_COMMENT:
		case LINE_SPACE:
		default:
			//Do nothing
			break;
		}
		if(0 != ret)
		{
			error("GetSectionList failed! ret = %d\n", ret);
			break;
		}
	}
	return ret;
}

/* 1.Check filepath is exist or not;
 * 2.Get a line from file:
 * 		2.1.file type : seciton, attr, comment, error;
 * 		2.2.file content;
 * 3.Get all info to local;
 * */
SECTION_INFO_NODE * moIniParser_Init(const char *filepath)
{
	if(NULL == filepath)
	{
		error("Input filepath is NULL!\n");
		return NULL;
	}
    SECTION_INFO_NODE *pHeadNode = NULL;
	pHeadNode = InitSecInfoList();
	if(NULL == pHeadNode)
	{
		error("InitSecInfoList failed!\n");
		return NULL;;
	}

	FILE *fp = NULL;
	fp = fopen(filepath, "r");
	if(NULL == fp)
	{
		error("Open file [%s] failed!\n", filepath);

        free(pHeadNode->pHeadAttrNode);
        pHeadNode->pHeadAttrNode = NULL;
        free(pHeadNode);
        pHeadNode = NULL;
        
		return NULL;
	}
	int ret = GetSectionList(fp, pHeadNode);
	if(0 != ret)
	{
		error("GetSectionList from [%s] failed!, ret = %d\n", filepath, ret);

        /* Free all memory being alloced when parse file */
        UnInitSecInfoList(pHeadNode);
	}

    fclose(fp);
    fp = NULL;

    return pHeadNode;
}


static void DumpSectionInfo(const SECTION_INFO_NODE *pSecInfoNode)
{
	printf("Section : [%s]\n", pSecInfoNode->sectionName);
	ATTR_INFO_NODE *pCurAttrInfoNode = pSecInfoNode->pHeadAttrNode->next;
	while(NULL != pCurAttrInfoNode)
	{
		printf("\t Key=[%s], Value=[%s]\n", pCurAttrInfoNode->attrInfo.key, pCurAttrInfoNode->attrInfo.value);
		pCurAttrInfoNode = pCurAttrInfoNode->next;
	}
	printf("\n");
}

void moIniParser_DumpAllInfo(const SECTION_INFO_NODE *pSecInfoHeadNode)
{
	if(NULL == pSecInfoHeadNode)
	{
		error("Should do init firstly, or cannot dump anything!\n");
		return ;
	}
	fprintf(stdout, "=========== Dump section list info start =============\n");
	SECTION_INFO_NODE *pCurSecInfoNode = pSecInfoHeadNode->next;
	while(NULL != pCurSecInfoNode)
	{
		DumpSectionInfo(pCurSecInfoNode);
		pCurSecInfoNode = pCurSecInfoNode->next;
	}
	fprintf(stdout, "=========== Dump section list info end =============\n");
}

/* Get the value of an attribute with input section and key*/
int moIniParser_GetAttrValue(const char *pSecName, const char *pAttrKey, char *pAttrValue, 
        const SECTION_INFO_NODE *pSecInfoHeadNode)
{
	if(NULL == pSecInfoHeadNode)
	{
		error("Do not init config file, cannot get attr info!\n");
		return -1;
	}
	if(NULL == pSecName || NULL == pAttrKey || NULL == pAttrValue)
	{
		error("Input param is NULL!\n");
		return -2;
	}
	int isFindSec = -1;
	SECTION_INFO_NODE *pCurSecNode = pSecInfoHeadNode->next;
	while(NULL != pCurSecNode)
	{
		if(0 == strcmp(pSecName, pCurSecNode->sectionName))
		{
			isFindSec = 0;
			break;
		}
		pCurSecNode = pCurSecNode->next;
	}
	if(0 != isFindSec)
	{
		error("Donot find input section [%s], cannot get any attribute infomation!\n",
				pSecName);
		return -3;
	}
	int isFindAttr = -1;
	ATTR_INFO_NODE *pCurAttrNode = pCurSecNode->pHeadAttrNode->next;
	while(NULL != pCurAttrNode)
	{
		if(0 == strcmp(pAttrKey, pCurAttrNode->attrInfo.key))
		{
			isFindAttr = 0;
			break;
		}
		pCurAttrNode = pCurAttrNode->next;
	}
	if(0 != isFindAttr)
	{
		error("Donot find input attr key [%s](section=[%s]), cannot get value for it!\n",
				pAttrKey, pSecName);
		return -4;
	}
	strcpy(pAttrValue, pCurAttrNode->attrInfo.value);
	return 0;
}

int moIniParser_UnInit(SECTION_INFO_NODE *pSecInfoHeadNode)
{
	if(NULL == pSecInfoHeadNode)
	{
		error("Do not init config file, cannot do uninit operation!\n");
		return -1;
	}

	UnInitSecInfoList(pSecInfoHeadNode);

	return 0;
}




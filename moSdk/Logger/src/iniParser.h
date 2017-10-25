#ifndef __INI_PARSER_H__
#define __INI_PARSER_H__

#ifdef __cplusplus
extern "C"{
#endif

#define SECTION_NAME_MAX_LEN	128
#define ATTR_KEY_MAX_LEN		128
#define ATTR_VALUE_MAX_LEN		128

/* The attribute*/
typedef struct
{
	char key[ATTR_KEY_MAX_LEN];
	char value[ATTR_VALUE_MAX_LEN];
}ATTR_INFO;

/* Record attr info of a section in a list*/
typedef struct ATTR_NODE
{
	ATTR_INFO attrInfo;
	struct ATTR_NODE *next;
}ATTR_INFO_NODE;

typedef struct SECTION_NODE
{
	char sectionName[SECTION_NAME_MAX_LEN];
	ATTR_INFO_NODE *pHeadAttrNode;		//The head of the list which record all attr info node;
	struct SECTION_NODE *next;
}SECTION_INFO_NODE;

/* Do init operation
 * If init succeed, return valid pointer;
 * otherwise, return NULL;
 * 
 * Make sure to call UnInit! Or memory leakage!
 * */
SECTION_INFO_NODE * iniParserInit(const char *filepath);

/* Dump all section info*/
void moIniParser_DumpAllInfo(const SECTION_INFO_NODE *pSecInfoHeadNode);

/* Get value with defined section and key
 * If get succeed, return value is 0, and the attribute value will set to pAttrValue;
 * else, return value is 0-;
 * */
int iniParserGetAttrValue(const char *pSecName, const char *pAttrKey, char *pAttrValue,
        const SECTION_INFO_NODE *pSecInfoHeadNode);

/* Do uninit operation*/
int iniParserUnInit(SECTION_INFO_NODE *pSecInfoHeadNode);


#ifdef __cplusplus
}
#endif

#endif

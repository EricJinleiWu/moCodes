#ifndef __CONF_PARSER_H__
#define __CONF_PARSER_H__

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_IP_LEN  16  //255.255.255.255 is the max value of ip

typedef struct
{
    char ip[MAX_IP_LEN];
    int port;
}IP_ADDR_INFO;

/*
    parse this config file;
*/
int confParserInit(const char * pFilepath);

/*
    get the server ip address info;
*/
int confParserGetServerInfo(IP_ADDR_INFO *pServInfo);

//TODO, if config file add other attributes, add functions here.

/*
    free all resources;
*/
void confParserUnInit();


#ifdef __cplusplus
}
#endif

#endif

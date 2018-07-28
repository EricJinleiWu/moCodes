#ifndef __HTTPSERVER_H__
#define __HTTPSERVER_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <sys/types.h> 
#include <regex.h>

#ifdef __cplusplus
extern "C" {
#endif

enum{
	HTTPSERVER_STATUS_OK = 200,

	HTTPSERVER_STATUS_CREATED = 201,
	HTTPSERVER_STATUS_ACCEPTED = 202,
	HTTPSERVER_STATUS_NOCONTENT = 204,

	HTTPSERVER_STATUS_BADREQUEST = 400, // (Bad Request/��������)
	HTTPSERVER_STATUS_UNAUTHORIZED = 401, // (Unauthorized/δ��Ȩ)
	HTTPSERVER_STATUS_PAYMENTREQUIRED = 402, // (Payment Required)  ��״̬����Ϊ�˽������ܵ������Ԥ����
	HTTPSERVER_STATUS_FORBIDDEN = 403, // (Forbidden/��ֹ)
	HTTPSERVER_STATUS_NOTFOUND = 404, // (Not Found/δ�ҵ�)
	HTTPSERVER_STATUS_METHODNOTALLOWED = 405, // (Method Not Allowed/����δ����)
	HTTPSERVER_STATUS_NotACCEPYABLE = 406, // (Not Acceptable/�޷�����)
	HTTPSERVER_STATUS_REQUESTTIMEOUT = 408, //(Request Timeout/����ʱ)

	HTTPSERVER_STATUS_INTERNALSERVERERROR = 500, //(Internal Server Error/�ڲ�����������)
	HTTPSERVER_STATUS_NOTIMPLEMENTED = 501, //(Not Implemented/δʵ��)
	HTTPSERVER_STATUS_SERVICEUNAVAILABLE = 503, //(Service Unavailable/�����޷����)

	HTTPSERVER_STATUS_MAX = 1000
};

enum{
	WEBSOCKET_REQUEST = 0,
	WEBSOCKET_RECV_DATA,
	WEBSOCKET_CLOSE,
};

typedef void (*HTTPSERVER_API_CALLBACK)(void *);
typedef int (*HTTPSERVER_CHECKPARAM_CALLBACK)(char *, void *);
typedef int (*HTTPSERVER_REQ_CALLBACK)(unsigned long, void*, char*, char*, char*, char*, int, void*, void *);

typedef int (*WEBSOCKET_REQ_CALLBACK)(char*, void *, char*, int, char*, int);
typedef int (*WEBSOCKET_API_CALLBACK)(char*, void *, char*, int, char*, int, void*);

typedef struct _httpHeadPrm{

	char cToken[512];
	char cRemoteIp[32];

}HttpHeadPrm, *pHttpHeadPrm;

typedef struct _httpServerPrm{

	char cWebRoot[128];
	char cCertPath[128];

	int  iHttpPort;

	HTTPSERVER_REQ_CALLBACK pHttpSerReqCbFunc; //  http ��������ص�������ַ
	WEBSOCKET_REQ_CALLBACK  pWebSktReqCbFunc; // web socket ����ص�������ַ


}HttpServerPrm, *pHttpServerPrm;

/* custom memory allocation */

typedef void *(*hs_malloc_t)(size_t);
typedef void (*hs_free_t)(void *);

void httpSer_set_alloc_funcs(hs_malloc_t malloc_fn, hs_free_t free_fn);
void httpSer_get_alloc_funcs(hs_malloc_t *malloc_fn, hs_free_t *free_fn);

int httpSerInit(pHttpServerPrm param);
int httpSerDeinit();
int httpSerDaemon();

int httpSerDoResponse(unsigned long handle, char *reqData, int status, char *result);
int httpSerAddEndpoint(char *method, char *url, HTTPSERVER_API_CALLBACK apiFunc, HTTPSERVER_CHECKPARAM_CALLBACK checkFunc);

int webSktAddEndpoint(char *url);
int webSktSendData(void *handle, char *data, int len);

#ifdef __cplusplus
}
#endif

#endif /* __HTTPSERVER_H__  */


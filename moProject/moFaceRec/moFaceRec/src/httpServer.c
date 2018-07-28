#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <pthread.h>

#include "httpServer.h"
#include "mongoose.h"

/* C89 allows these to be macros */
#undef malloc
#undef free

/* memory function pointers */
static hs_malloc_t do_malloc = malloc;
static hs_free_t do_free = free;


// ======================================================================================================
typedef struct _HSEPNode{
	pthread_mutex_t mxCmd;

	char cMethod[8];
	char cURL[128];

	regex_t stRegex;
	//regmatch_t pMatch[1];

	HTTPSERVER_API_CALLBACK pApiFunc;
	HTTPSERVER_CHECKPARAM_CALLBACK pCheckFunc;

	struct _HSEPNode *pPre;
	struct _HSEPNode *pNext;

}st_HSEpNode, *pst_HSEpNode;


typedef struct _WSEPNode{

	char cURL[128];

	HTTPSERVER_API_CALLBACK pApiFunc;
	HTTPSERVER_CHECKPARAM_CALLBACK pCheckFunc;

	struct _WSEPNode *pPre;
	struct _WSEPNode *pNext;

}st_WSEpNode, *pst_WSEpNode;

// ======================================================================================================

static struct mg_serve_http_opts s_http_server_opts;
static struct mg_mgr mgr;
static pst_HSEpNode gHSEpList = NULL; // http server 
static pst_WSEpNode gWSEpList = NULL; // web socket

// ======================================================================================================

void *hs_malloc(size_t size)
{
    if(!size)
        return NULL;

    return (*do_malloc)(size);
}

void hs_free(void *ptr)
{
    if(!ptr)
        return;

    (*do_free)(ptr);
}

void hs_set_alloc_funcs(hs_malloc_t malloc_fn, hs_free_t free_fn)
{
    do_malloc = malloc_fn;
    do_free = free_fn;
}

void hs_get_alloc_funcs(hs_malloc_t *malloc_fn, hs_free_t *free_fn)
{
    if (malloc_fn)
        *malloc_fn = do_malloc;
    if (free_fn)
        *free_fn = do_free;
}

// ======================================================================================================

static sig_atomic_t s_received_signal = 0;
static sock_t sock[2];
static unsigned long s_next_id = 0;

// This info is passed to the worker thread
struct work_request {
	unsigned long conn_id;  // needed to identify the connection where to send the reply

	HttpHeadPrm headPrm;
	char url[256];
	char method[8];
	char query[128];

	void *reqData;
	int  reqLen;

	void *httpReqFunc;
	void *apiFunc;
	void *checkFunc;

	void *mutex;
	// optionally, more data that could be required by worker 
};

// This info is passed by the worker thread to mg_broadcast
struct work_result {
	unsigned long conn_id;

	int status;
	void *reqData;
	char *result;
};

static void signal_handler(int sig_num)
{
	printf(" [%s] ######### Line : %d\n",__FUNCTION__,__LINE__);

	signal(sig_num, signal_handler);
	s_received_signal = sig_num;
}

static void on_work_complete(struct mg_connection *nc, int ev, void *ev_data)
{
	(void) ev;
//	char s[32];
	struct mg_connection *c;
	
	for(c=mg_next(nc->mgr, NULL); c!=NULL; c=mg_next(nc->mgr, c))
	{
		if(c->user_data != NULL)
		{
			struct work_result *res = (struct work_result *)ev_data;
			if((unsigned long)c->user_data == res->conn_id)
			{
				/* Send headers */
				mg_printf(c, "HTTP/1.1 %d OK\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n", res->status);

				/* Compute the result and send it back as a JSON object */
				if(res->result && strcmp(res->result, "") != 0)
					mg_printf_http_chunk(c, "%s", res->result);

				mg_send_http_chunk(c, "", 0); /* Send empty chunk, the end of response */

				//nc->flags |= MG_F_CLOSE_IMMEDIATELY;
				c->flags |= MG_F_SEND_AND_CLOSE;

				if(res->reqData)
				{
					hs_free(res->reqData);
					res->reqData = NULL;
				}

				if(res->result)
				{
					hs_free(res->result);
					res->result = NULL;
				}
			}
		}
	}
}

void *worker_thread_proc(void *param)
{
//	struct mg_mgr *mgr = (struct mg_mgr *)param;
	struct work_request req = {0};

	while(s_received_signal == 0)
	{
		if(read(sock[1], &req, sizeof(req)) < 0)
		{
			perror("Reading worker sock");
		}

		if(req.httpReqFunc)
		{
			pthread_mutex_lock((pthread_mutex_t *)req.mutex);
			((HTTPSERVER_REQ_CALLBACK)req.httpReqFunc)(req.conn_id, 
														&req.headPrm, 
														req.method, req.url, req.query, 
														req.reqData, req.reqLen,
														req.apiFunc, req.checkFunc);
			pthread_mutex_unlock((pthread_mutex_t *)req.mutex);
		}

	}

	return NULL;
}

/*
    匹配url;
    在http请求到来后，url默认是我们定义好的值，例如我们为格灵深瞳定义的"/bstar/glintEvtCenter",
    这种时候，使用ev_handler-MG_EV_HTTP_REQUEST-regexec，做正则表达式是可以正确找到url的;

    但是格灵深瞳的url发过来的时候，默认带着其ip，例如:"http://192.168.11.199:8089/bstar";
    这种时候，完整匹配的正则表达式就无法匹配到了,因此封装了这个函数，来做url匹配;

    @curUrl:我们定义好的url路径,例如"/bstar/glintEvtCenter";
    @cmpUrl:格灵深瞳发过来的url，例如"http://192.168.11.199:8089/bstar";

    匹配规则:
        @cmpUrl中找到IP地址后的第一个"/",根据其后的内容进行比较;

    返回值:
        0 : 匹配成功
        0-: 匹配失败
*/
static int urlMatch(const char *curUrl, const char *cmpUrl)
{
    if(NULL == curUrl || NULL == cmpUrl)
    {
        fprintf(stderr, "Input param is NULL.\n");
        return -1;
    }

    if(strlen(curUrl) > strlen(cmpUrl))
    {
        fprintf(stderr, "curUrl=[%s], length=%d, cmpUrl=[%s], length=%d, will not match of course.\n",
            curUrl, strlen(curUrl), cmpUrl, strlen(cmpUrl));
        return -2;
    }

    char * pStartPos = NULL;
    //如果@cmpUrl以"http://"作为起始，我们认为他包含了完整的ip地址信息,需要寻找真正的url内容
    if(strncmp(cmpUrl, "http://", strlen("http://")) == 0)
    {
        pStartPos = strchr(cmpUrl + strlen("http://"), '/');
        if(NULL == pStartPos)
        {
            fprintf(stderr, "cmpUrl=[%s], donot find valid url value in it!\n", cmpUrl);
            return -3;
        }
    }
    else
    {
        pStartPos = cmpUrl;
    }

    return (strcmp(curUrl, pStartPos) == 0) ? 0 : -4;        
}

static int getPureUrl(char * url)
{
    if(NULL == url)
    {
        fprintf(stderr, "Input param is NULL.\n");
        return -1;
    }
    if(strncmp(url, "http://", strlen("http://")) == 0)
    {
        char * pPurePos = strchr(url + strlen("http://"), '/');
        if(NULL == pPurePos)
        {
            fprintf(stderr, "donot find valid symbol in url(%s)\n", url);
            return -2;
        }
        int len = strlen(url) - (pPurePos - url);
        memmove(url, pPurePos, len);
        url[len] = 0x00;
    }
    return 0;
}

static void ev_handler(struct mg_connection *nc, int ev, void *ev_data)
{
	struct http_message *hm = (struct http_message *)ev_data;
	struct mg_str *hdr = NULL;

	char cURL[256] = {0};
//	char cMethod[8] = {0};
//	char cQuery[128] = {0};

	HttpHeadPrm headPrm;
	memset(&headPrm, 0, sizeof(headPrm));

	switch(ev){
		case MG_EV_WEBSOCKET_HANDSHAKE_REQUEST:
		{
			memcpy(cURL, hm->uri.p, hm->uri.len);

			pst_WSEpNode cur = gWSEpList;
			while(cur)
			{
				if(strcmp(cURL, cur->cURL) == 0)
				{
					break;
				}

				cur = cur->pNext;
			}

			if(cur == NULL)
			{
				nc->flags |= MG_F_CLOSE_IMMEDIATELY;
				break;
			}

			char addr[32];
			mg_sock_addr_to_str(&nc->sa, addr, sizeof(addr), MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);

			if(nc->websocket_cb)
				((WEBSOCKET_REQ_CALLBACK)(nc->websocket_cb))(addr, (void *)nc, cURL, WEBSOCKET_REQUEST, NULL, 0);

			nc->flags |= MG_F_CONNECTING;

			break;
		}
		case MG_EV_WEBSOCKET_HANDSHAKE_DONE:
		{
			break;
		}
		case MG_EV_WEBSOCKET_FRAME:
		{
			char addr[32];
			mg_sock_addr_to_str(&nc->sa, addr, sizeof(addr), MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);

			struct websocket_message *wm = (struct websocket_message *) ev_data;

			if(nc->websocket_cb)
				((WEBSOCKET_REQ_CALLBACK)(nc->websocket_cb))(addr, (void *)nc, NULL, WEBSOCKET_RECV_DATA, (char *)wm->data, wm->size);
#if 0
			struct websocket_message *wm = (struct websocket_message *) ev_data;
			/* New websocket message. Tell everybody. */
			struct mg_str d = {(char *) wm->data, wm->size};
			mg_send_websocket_frame(nc, WEBSOCKET_OP_TEXT, d.p, d.len);
#endif

			break;
		}

		case MG_EV_ACCEPT:
		{
			nc->user_data = (void *)++s_next_id;
			if(nc->user_data == 0)
				nc->user_data ++;
			break;
		}
		case MG_EV_HTTP_REQUEST:
		{
			struct work_request req;
			memset(&req, 0, sizeof(req));

			struct http_message *hm = (struct http_message *)ev_data;

			memcpy(req.url, hm->uri.p, hm->uri.len);
            getPureUrl(req.url);
			memcpy(req.method, hm->method.p, hm->method.len);
			memcpy(req.query, hm->query_string.p, hm->query_string.len);

			req.reqLen = hm->body.len;
			req.reqData = (char *)hs_malloc(req.reqLen + 1);
			memset(req.reqData, 0, req.reqLen + 1);
			memcpy(req.reqData, hm->body.p, req.reqLen);

			//*
			// 检测头信息
			if((hdr = mg_get_http_header(hm, "Authorization")) == NULL)
			{
				hdr = mg_get_http_header(hm, "authorization");
			}
			if(hdr)
			{
				memcpy(req.headPrm.cToken, hdr->p, hdr->len);
			}

			if((hdr = mg_get_http_header(hm, "X-Real_IP")) != NULL)
			{
				memcpy(req.headPrm.cRemoteIp, hdr->p, hdr->len);
			}
            else
            {
                //如果http header中没有包括对端IP,那么直接使用其http连接建立的nc参数中获取
                strcpy(req.headPrm.cRemoteIp, inet_ntoa(nc->sa.sin.sin_addr));
            }
	
			pst_HSEpNode cur = gHSEpList;
			while(cur)
			{
				if(strcmp(req.method, cur->cMethod) == 0)
				{
//					regmatch_t match[1];
                    int ret = 0;
					if((ret = regexec(&cur->stRegex, req.url, 0, NULL, 0)) == REG_NOERROR)
//					if((ret = urlMatch(cur->cURL, req.url)) == 0)
					{
						req.conn_id = (unsigned long)nc->user_data;
						req.httpReqFunc = nc->httpserver_cb;
						req.apiFunc = cur->pApiFunc;
						req.checkFunc = cur->pCheckFunc;
						req.mutex = &cur->mxCmd;

						break;
					}
				}
	
				cur = cur->pNext;
			}
	
			if(cur == NULL)
			{
				mg_serve_http(nc, hm, s_http_server_opts); /* Serve static content */
			}
			else
			{				
				if(write(sock[0], &req, sizeof(req)) < 0)
					perror("Writing worker sock");
			}

			break;
		}
		case MG_EV_CLOSE:
		{
			if(nc->user_data)
				nc->user_data = NULL;
			
			if(nc->flags & MG_F_IS_WEBSOCKET)
			{
				char addr[32];
				mg_sock_addr_to_str(&nc->sa, addr, sizeof(addr), MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);

				if(nc->websocket_cb)
					((WEBSOCKET_REQ_CALLBACK)(nc->websocket_cb))(addr, (void *)nc, NULL, WEBSOCKET_CLOSE, NULL, 0);

				nc->flags |= MG_F_CLOSE_IMMEDIATELY;
			}
			break;
		}

		default:
			break;
	}
}

/**
 * @fn  httpSerInit
 * @brief http 服务初始化函数
 * @param[in] param:初始化信息参数
 * @param[out]
 * @return
 * @retval 0: 成功
 * @retval other: 失败
 */
int httpSerInit(pHttpServerPrm param)
{
	if(param == NULL)
		return -1;

	struct mg_connection *nc;
	struct mg_bind_opts bind_opts;

	char http_port[16] = {0};
	const char *err_str;

	int s_num_worker_threads = 10;
	int i = 0;

	if(mg_socketpair(sock, SOCK_STREAM) == 0)
	{
		perror("Opening socket pair");
		return -1;
	}

	//signal(SIGTERM, signal_handler);
	//signal(SIGINT, signal_handler);

	mg_mgr_init(&mgr, NULL);

	s_http_server_opts.document_root = param->cWebRoot;

	/* Set HTTP server options */
	memset(&bind_opts, 0, sizeof(bind_opts));
	bind_opts.error_string = &err_str;
#if MG_ENABLE_SSL
	if(strlen(param->cCertPath) > 0)
	{
		bind_opts.ssl_cert = param->cCertPath;
	}
#endif

	if(param->iHttpPort > 0)
		sprintf(http_port, "%d", param->iHttpPort);
	else
		sprintf(http_port, "%d", 8000);

	bind_opts.httpserver_cb = param->pHttpSerReqCbFunc;
	bind_opts.websocket_cb = param->pWebSktReqCbFunc;

	nc = mg_bind_opt(&mgr, http_port, ev_handler, bind_opts);
	if(nc == NULL)
	{
		fprintf(stderr, "[%s] Error starting server on port %s: %s\n",__FUNCTION__, http_port,*bind_opts.error_string);
		return -1;
	}

	mg_set_protocol_http_websocket(nc);
	s_http_server_opts.enable_directory_listing = "no";

	for(i=0; i<s_num_worker_threads; i++)
	{
		mg_start_thread(worker_thread_proc, &mgr);
	}

	gHSEpList = NULL;
	gWSEpList = NULL;

	printf("[%s] Starting RESTful server on port %s, serving %s\n",__FUNCTION__, http_port, s_http_server_opts.document_root);

	return 0;
}


/**
 * @fn  httpSerDeinit
 * @brief http 服务注销函数
 * @param[in]
 * @param[out]
 * @return
 * @retval 0: 成功
 * @retval other: 失败
 */
int httpSerDeinit()
{
	mg_mgr_free(&mgr);

	closesocket(sock[0]);
	closesocket(sock[1]);

	/*释放URL 终端响应函数节点*/
	while(gHSEpList)
	{
		pst_HSEpNode node = gHSEpList;
	
		gHSEpList = gHSEpList->pNext;

		regfree(&node->stRegex);

		hs_free(node);
		node = NULL;
	}

	/*释放URL 终端响应函数节点*/
	while(gWSEpList)
	{
		pst_WSEpNode node = gWSEpList;
	
		gWSEpList = gWSEpList->pNext;

		hs_free(node);
		node = NULL;
	}

	return 0;
}

/**
 * @fn  httpSerDaemon
 * @brief http 服务循环检测模块(死循环)
 * @param[in]
 * @param[out]
 * @return
 * @retval 0: 成功
 * @retval other: 失败
 */
int httpSerDaemon()
{
	while(true)
	{
		mg_mgr_poll(&mgr, 5000); // 5000 ms 超时
	}

	return 0;
}

int httpSerGetVarInt(char *src, char *key, int *val)
{
	struct mg_str buf;
	buf.p = src;
	buf.len = strlen(buf.p);

	char cValue[256] = {0};
	if(mg_get_http_var(&buf, key, cValue, sizeof(cValue)) > 0)
	{
		*val = atoi(cValue);
	}

	return 0;
}

int httpSerGetVarStr(char *src, char *key, char *val, int len)
{
	struct mg_str buf;
	buf.p = src;
	buf.len = strlen(buf.p);

	char cValue[256] = {0};
	if(mg_get_http_var(&buf, key, cValue, sizeof(cValue)) > 0)
	{
		strncpy(val, cValue, sizeof(len) - 1);
	}

	return 0;
}

/**
 * @fn  httpSerDoResponse
 * @brief http 服务回复响应函数
 * @param[in] handle:http 请求上下文句柄
                     reqData : http body 请求内容指针
                     status : http 返回状态值
                     result : http 返回内容(如果不为NULL, 函数内部会执行free 操作, 使用此函数时请注意!!!!)
 * @param[out]
 * @return
 * @retval 0: 成功
 * @retval other: 失败
 */
int httpSerDoResponse(unsigned long handle, char *reqData, int status, char *result)
{
	unsigned long conn_id = handle;

	struct work_result res;
	memset(&res, 0, sizeof(res));
	res.conn_id = conn_id;
	res.status = status;
	res.reqData = reqData;
	res.result = result;
	mg_broadcast(&mgr, on_work_complete, (void *)&res, sizeof(res));

	return 0;
}

/**
 * @fn  httpSerAddEndpoint
 * @brief http 服务添加响应的URL 及相关信息
 * @param[in] method : 此URL 的方法
                     url : 此URL 的地址内容
                     apiFunc : 此URL 响应的回调函数入口地址
                     checkFunc : 此URL 响应的参数检查函数入口地址
 * @param[out]
 * @return
 * @retval 0: 成功
 * @retval other: 失败
 */
int httpSerAddEndpoint(char *method, char *url, HTTPSERVER_API_CALLBACK apiFunc, HTTPSERVER_CHECKPARAM_CALLBACK checkFunc)
{
	// may be you could call function "mg_register_http_endpoint" ....

	if(method == NULL || url == NULL || apiFunc == NULL)
		return -1;

	/*
		编译正则表达式
		REG_EXTENDED 以功能更加强大的扩展正则表达式的方式进行匹配。
		REG_ICASE 匹配字母时忽略大小写。
		REG_NOSUB 不用存储匹配后的结果。
		REG_NEWLINE 识别换行符，这样'$'就可以从行尾开始匹配，'^'就可以从行的开头开始匹配。
	*/ 
	int flags = REG_EXTENDED | REG_NOSUB;
	char ebuf[128];

	char urlRegular[256];
	memset(urlRegular, 0, sizeof(urlRegular));
	sprintf(urlRegular, "^%s$", url);
	//sprintf(urlRegular, "%s", url);

	if(gHSEpList == NULL)
	{
		pst_HSEpNode node = (pst_HSEpNode)hs_malloc(sizeof(st_HSEpNode));
		memset(node, 0, sizeof(*node));

		pthread_mutex_init(&node->mxCmd, NULL);
		strncpy(node->cMethod, method, sizeof(node->cMethod) - 1);
		strncpy(node->cURL, url, sizeof(node->cURL) - 1);
		node->pApiFunc = apiFunc;
		node->pCheckFunc = checkFunc;

		int z = regcomp(&node->stRegex, urlRegular, flags);
		if(z != 0)
		{ 
			regerror(z, &node->stRegex, ebuf, sizeof(ebuf));
			fprintf(stderr, "%s: pattern '%s' \n", ebuf, url);
			return -1;
		}

		gHSEpList = node;
	}
	else
	{
		pst_HSEpNode cur = gHSEpList;
		pst_HSEpNode last = NULL;
		while(cur)
		{
			if(strcmp(method, cur->cMethod) == 0
				&& strcmp(url, cur->cURL) == 0)
			{
				cur->pApiFunc = apiFunc;

				return 0;
			}

			last = cur;
			cur = cur->pNext;
		}

		pst_HSEpNode node = (pst_HSEpNode)hs_malloc(sizeof(st_HSEpNode));
		memset(node, 0, sizeof(*node));

		pthread_mutex_init(&node->mxCmd, NULL);
		strncpy(node->cMethod, method, sizeof(node->cMethod) - 1);
		strncpy(node->cURL, url, sizeof(node->cURL) - 1);
		node->pApiFunc = apiFunc;
		node->pCheckFunc = checkFunc;

		int z = regcomp(&node->stRegex, urlRegular, flags);
		if(z != 0)
		{ 
			regerror(z, &node->stRegex, ebuf, sizeof(ebuf));
			fprintf(stderr, "%s: pattern '%s' \n", ebuf, url);
			return -1;
		}

		node->pPre = last;
		node->pNext = NULL;
		last->pNext = node;
	}

	return 0;
}

/**
 * @fn  webSktAddEndpoint
 * @brief web socket 添加响应的URL 及相关信息
 * @param[in] url : 此URL 的地址内容
 * @param[out]
 * @return
 * @retval 0: 成功
 * @retval other: 失败
 */
int webSktAddEndpoint(char *url)
{
	if(url == NULL)
		return -1;

	if(gWSEpList == NULL)
	{
		pst_WSEpNode node = (pst_WSEpNode)hs_malloc(sizeof(st_WSEpNode));
		memset(node, 0, sizeof(*node));

		strncpy(node->cURL, url, sizeof(node->cURL) - 1);

		gWSEpList = node;
	}
	else
	{
		pst_WSEpNode cur = gWSEpList;
		pst_WSEpNode last = NULL;
		while(cur)
		{
			if(strcmp(url, cur->cURL) == 0)
			{
				return 0;
			}

			last = cur;
			cur = cur->pNext;
		}

		pst_WSEpNode node = (pst_WSEpNode)hs_malloc(sizeof(st_WSEpNode));
		memset(node, 0, sizeof(*node));

		strncpy(node->cURL, url, sizeof(node->cURL) - 1);

		node->pPre = last;
		node->pNext = NULL;
		last->pNext = node;
	}

	return 0;
}

/**
 * @fn  webSktSendData
 * @brief web socket 发送数据函数
 * @param[in] handle : web socket 请求上下文句柄
                     data : 发送的数据内容
                     len : 发送的数据长度
 * @param[out]
 * @return
 * @retval 0: 成功
 * @retval other: 失败
 */
int webSktSendData(void *handle, char *data, int len)
{
	mg_send_websocket_frame(handle, WEBSOCKET_OP_TEXT, data, len);
}


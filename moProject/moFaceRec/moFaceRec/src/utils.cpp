#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "utils.h"

int isValidIpAddr(const char * pIp)
{
    struct in_addr addr;
    memset(&addr, 0x00, sizeof(struct in_addr));
    return ((pIp == NULL) || (inet_aton(pIp, &addr) == 0)) ? 0 : 1;
}

int isValidUserPort(const int port)
{
    return (port >= 1024 && port <= 65535) ? 1 : 0;
}

int setSockReusable(const int sockId)
{
    //Set this socket can be reused
    int on = 1;
    int ret = setsockopt(sockId, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    if(ret != 0)
    {
        dbgError("setsockopt failed! ret = %d, errno = %d, desc = [%s]\n", ret, errno, strerror(errno));
        return -1;
    }
    return 0;
}

int setSock2NonBlock(const int sockId)
{
    if(sockId < 0)
    {
        dbgError("Input sockId=%d, invalid value!", sockId);
        return -1;
    }
    
    int b_on = 1;
    ioctl(sockId, FIONBIO, &b_on);
    return 0;
}
 
int readn(const int sockId, char* buf, const int len)
{
	int n = 0;
	char *ptr = (char*)buf;
	int left = len;
	
	while( left > 0 ) 
	{
		if((n = read(sockId, ptr, left)) < 0)
		{
			if(EINTR == errno)
				continue;
			else if(EAGAIN == errno)
				break;
			else
				return -1;
		}
		else if(0 == n)
		{
			return -2;
		}
		
		left -= n;
		ptr += n;
	}
	
	return (len - left);
}

int writen(const int sockId, const char* buf, const int len)
{
	int n = 0;
	int left = len;
	char *ptr = (char *)buf;
	
	while(left > 0) 
	{
		if((n = write(sockId, ptr, left)) < 0)
		{
			if(EINTR == errno)
				continue;
            else if(EAGAIN == errno)
				break;
			else
				return -1;
		}
		else if(0 == n)
		{
			return -2;
		}
		
		left -= n;
		ptr += n;
	}
	
	return (len - left);
}


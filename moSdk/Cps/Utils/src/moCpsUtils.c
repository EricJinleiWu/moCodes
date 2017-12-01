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

#include "moCpsUtils.h"
#include "moLogger.h"

int isValidIpAddr(const char * pIp)
{
    struct in_addr addr;
    memset(&addr, 0x00, sizeof(struct in_addr));
    return ((pIp == NULL) || (inet_aton(pIp, &addr) == 0)) ? 0 : 1;
}

int setSock2NonBlock(const int sockId)
{
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
			if (EINTR == errno)
				continue;
			else if( errno == EAGAIN )
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
			if (EINTR == errno)
				continue;
            else if(errno == EAGAIN)
				break;
			else
				return -1;
		}
		else if (0 == n)
		{
			return -2;
		}
		
		left -= n;
		ptr += n;
	}
	
	return (len - left);
}

int splitInt2Char(const int src, char dst[4])
{
    int i = 0;
    int tmp = src;
    for(; i < 4; i++)
    {
        dst[i] = tmp % 0xff;
        tmp /= 0xff;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "src=%d(0x%x), dst=[%d(0x%x), %d(0x%x), %d(0x%x), %d(0x%x)]\n",
        src, src, dst[0], dst[0], dst[1], dst[1], dst[2], dst[2], dst[3], dst[3]);
    return 0;
}

int mergeChar2Int(const char src[4], int *dst)
{
    int i = 0;
    int ret = 0;
    for(; i < 4; i++)
    {
        int tmp = src[i];
        ret += tmp << (8 * i);
    }
    *dst = ret;
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "src=[%d(0x%x), %d(0x%x), %d(0x%x), %d(0x%x)], dst=%d(0x%x)\n",
        src[0], src[0], src[1], src[1], src[2], src[2], src[3], src[3], *dst, *dst);
    return 0;
}


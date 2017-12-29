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

/*
    1.check the thread running or not;
    2.send signal to stop it;
    3.wait it stoped;
*/
int killThread(const pthread_t thId)
{
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "The thread being stopped with ID=%lu\n", (unsigned long int)thId);
    //check this thread running or not now.
    int ret = pthread_kill(thId, 0);
    if(ret != 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "pthread_kill failed! ret=%d\n", ret);
        if(ret == ESRCH)
        {
            moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Thread donot running now! cannot stop it!\n");
            return -1;
        }
        else if(ret == EINVAL)
        {
            moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Invalid signal!\n");
            return -2;
        }
        else
        {
            moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Other error! ret = %d\n", ret);
            return -3;
        }
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "This thread still running, can stop it now.\n");

    //send signal to stop it
    ret = pthread_kill(thId, MOCPS_STOP_THR_SIG);
    if(ret != 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "pthread_kill failed! ret = %d\n", ret);
        return -4;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "pthread_kill succeed.\n");

    //wait the thread stopped
    ret = pthread_join(thId, NULL);
    if(ret != 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "pthread_join failed! ret = %d\n", ret);
        return -5;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "pthread_join succeed!\n");

    return 0;
}

void threadExitSigCallback(int sigNo)
{
    moLoggerInfo(MOCPS_MODULE_LOGGER_NAME, 
        "Thread (id=%lu) recv the stopped signal(sigNo=%d), will stop now!\n",
        pthread_self(), sigNo);
}

/*
    1.sigaction;
    2.sigadd;
*/
int threadRegisterSignal(sigset_t * pSet)
{
    struct sigaction actions;
    memset(&actions, 0x00, sizeof(struct sigaction));
    sigemptyset(&actions.sa_mask);
    actions.sa_flags = 0;
    actions.sa_handler = threadExitSigCallback;
    int ret = sigaction(MOCPS_STOP_THR_SIG, &actions, NULL);
    if(ret != 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "sigaction failed! " \
            "ret = %d, errno = %d, desc = [%s]\n", ret, errno, strerror(errno));
        return -1;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "sigaction succeed.\n");

    sigemptyset(pSet);
    sigaddset(pSet, MOCPS_STOP_THR_SIG);

    return 0;
}










































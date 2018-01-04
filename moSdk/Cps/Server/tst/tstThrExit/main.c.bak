#include <stdio.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>

static void signalExitFunc(int sigNo)
{
	printf("Recv sigNo = %d\n", sigNo);
	printf("will exit this thread! thId = %u\n", pthread_self());
	return ;
}

static void * thrFunc(void * args)
{
#if 1
	struct sigaction actions;
	memset(&actions, 0x00, sizeof(struct sigaction));
	sigemptyset(&actions.sa_mask);
	actions.sa_flags = 0;
	actions.sa_handler = signalExitFunc;

	int ret = sigaction(SIGALRM, &actions, NULL);
	printf("sigaction return %d, errno=%d, desc=[%s]\n", 
		ret, errno, strerror(errno));
#endif
	printf("Thread start now.\n");

	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGALRM);
    	//sigprocmask(SIG_SETMASK,&set,NULL);

	struct timespec tm;
	tm.tv_sec = 1;
	tm.tv_nsec = 0;
	while(1)
	{
	ret = sigtimedwait(&set, NULL, &tm);
	printf("sigtimedwait return %d\n", ret);
	if(ret < 0)
	{
		if(errno == EAGAIN)
		{
			printf("Donot recv signal in timeout, loop.\n");
			continue;
		}
		else
		{
			printf("errno = %d(%s), exit!\n", errno, strerror(errno));
			break;
		}
	}
	else
	{
		printf("Get a signal = %d\n", ret);
		if(ret == SIGALRM)
		{
			printf("SIGALRM get, exit!\n");
			return NULL;
		}
	}
	}

#if 0
	int leftSeconds = sleep(10);
	int cnt = 0;
	while(1)
	{
		cnt++;
		sleep(1);
		printf("cnt = %d\n", cnt);
	}

	//printf("Thread stop now. leftSeconds = %d\n", leftSeconds);
	printf("Thread stop now.\n");
#endif
}

int main(int argc, char **argv)
{
	pthread_t thId = 0, th1Id = 0;
	int ret = pthread_create(&thId, NULL, thrFunc, NULL);
	printf("pthread_create return %d, thId = %u\n", ret, thId);
	ret = pthread_create(&th1Id, NULL, thrFunc, NULL);
	printf("pthread_create return %d, th1Id = %u\n", ret, th1Id);

	sleep(4);
	printf("start kill this thread.\n");

	pthread_kill(thId, SIGALRM);

	printf("kill over.\n");
	sleep(4);

	printf("main process exit now.\n");
	
	return 0;
}

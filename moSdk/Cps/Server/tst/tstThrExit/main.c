#include <stdio.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>

static void signalExitFunc(int sigNo)
{
	printf("Recv sigNo = %d\n", sigNo);
	printf("will exit this thread! thId = %u\n", pthread_self());
}

static void * thrFunc(void * args)
{
#if 0
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
    
    sigset_t set, orgSet;
    sigemptyset(&set);
    sigemptyset(&orgSet);
    sigaddset(&set, SIGALRM);
    sigprocmask(SIG_BLOCK, &set, &orgSet);
    signal(SIGALRM, signalExitFunc);

	while(1)
	{
    	struct timespec tm;
    	tm.tv_sec = 1;
    	tm.tv_nsec = 0;
    	struct timespec tm1;
    	tm1.tv_sec = 0;
    	tm1.tv_nsec = 0;
        
        int ret = pselect(0, NULL, NULL, NULL, &tm, &orgSet);
        if(ret < 0)
        {
            printf("ret=%d, errno=%d, desc=[%s]\n", ret, errno, strerror(errno));
            if(errno == EINTR)
            {
                printf("signal check now.\n");
                ret = sigtimedwait(&set, NULL, &tm1);
                if(ret == SIGALRM)
//                if(!gThrRunning)
                {
                    printf("Thread will exit now!\n");
                    break;
                }
            }
            break;
        }
        else if(ret == 0)
        {
            printf("timeout.\n");
            if(errno == EINTR)
            {
                printf("signal check now.\n");
                ret = sigtimedwait(&set, NULL, &tm1);
                if(ret == SIGALRM)
//                if(!gThrRunning)
                {
                    printf("Thread will exit now!\n");
                    break;
                }
            }
            continue;
        }
        else
        {
            printf("ret > 0.");
            continue;
        }
	}
//    pthread_exit(NULL);
    return NULL;
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
    ret = pthread_join(thId, NULL);
    printf("pthread_join ret=%d, errno=%d, desc=[%s]\n",
        ret, errno, strerror(errno));

	printf("kill over.\n");
	sleep(4);

	printf("main process exit now.\n");
	
	return 0;
}

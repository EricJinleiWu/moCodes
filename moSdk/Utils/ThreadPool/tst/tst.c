#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "moUtils.h"
#include "thMgr.h"
#include "taskQueue.h"

static void func1(void *args)
{
    args = args;
    static int func1_var = 0;
    func1_var++;
    printf("function:[%s], var=%d\n", __FUNCTION__, func1_var);
}

static void func2(void *args)
{
    args = args;
    static int func2_var = 10240;
    func2_var++;
    printf("function:[%s], var=%d\n", __FUNCTION__, func2_var);
}


/*
    1.init and uninit, check memory OK or leak;
    2.add and get, check content right or not;
*/
static void tst_taskQueue(void)
{
    TASKDQUEUE queue;
    memset(&queue, 0x00, sizeof(TASKDQUEUE));
    int ret = taskQueueInit(&queue);
    if(ret != MOUTILS_TP_ERR_OK)
    {
        printf("taskQueueInit failed! ret = 0x%x\n", ret);
        return ;
    }
    printf("taskQueueInit succeed.\n");

    //add a task
    MOUTILS_TP_TASKINFO taskInfo;
    memset(&taskInfo, 0x00, sizeof(MOUTILS_TP_TASKINFO));
    taskInfo.pTaskFunc = func1;
    taskInfo.args4TaskFunc = NULL;
    ret = taskQueueAddTask(&queue, taskInfo);
    if(ret != MOUTILS_TP_ERR_OK)
    {
        printf("taskQueueAddTask failed! ret = 0x%x\n", ret);
        taskQueueUnInit(&queue);
        return ;
    }
    printf("taskQueueAddTask succeed.\n");
    
    //get task, and do it.
    TASKNODE *pTaskNode = taskQueueGetTask(&queue);
    if(NULL != pTaskNode)
    {
        printf("get task node succeed first time.\n");
        pTaskNode->taskInfo.pTaskFunc(pTaskNode->taskInfo.args4TaskFunc);
        free(pTaskNode);
        pTaskNode = NULL;
    }
    
    TASKNODE *pTaskNode2 = taskQueueGetTask(&queue);
    if(NULL != pTaskNode2)
    {
        printf("get task node succeed second time.\n");
        pTaskNode->taskInfo.pTaskFunc(pTaskNode->taskInfo.args4TaskFunc);
        free(pTaskNode2);
        pTaskNode2 = NULL;
    }
    
    taskQueueUnInit(&queue);
}

static void tst_thMgr(void)
{
    THMGR_INFO *pThMgr = (THMGR_INFO *)moUtils_TP_init(16);
    if(NULL == pThMgr)
    {
        printf("moUtils_TP_init failed!\n");
        return ;
    }
    printf("moUtils_TP_init succeed!\n");

    int i = 0;
    for(i = 0; i < 16; i++)
    {
        MOUTILS_TP_TASKINFO taskInfo;
        
        memset(&taskInfo, 0x00, sizeof(MOUTILS_TP_TASKINFO));
        taskInfo.pTaskFunc = func1;
        taskInfo.args4TaskFunc = NULL;
        moUtils_TP_addTask(pThMgr, taskInfo);
        
        memset(&taskInfo, 0x00, sizeof(MOUTILS_TP_TASKINFO));
        taskInfo.pTaskFunc = func2;
        taskInfo.args4TaskFunc = NULL;
        moUtils_TP_addTask(pThMgr, taskInfo);
    }
    sleep(3);
    moUtils_TP_uninit(pThMgr);
}

int main(int argc, char **argv)
{
    srand((unsigned int )time(NULL));

//    tst_taskQueue();

    tst_thMgr();
    
	return 0;
}

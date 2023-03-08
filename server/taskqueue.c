 ///
 /// @file    taskqueue.c
 /// @author  lemon(haohb13@gmail.com)
 /// @date    2022-06-06 11:07:34
 ///
 
#include <func.h>
#include "taskqueue.h"

void queueInit(task_queue_t * que)
{
	if(que) {
		que->pFront = NULL;
		que->pRear = NULL;
		que->queSize = 0;
		que->exitFlag = 0;
		int ret = pthread_mutex_init(&que->mutex, NULL);
		THREAD_ERROR_CHECK(ret, "pthread_mutex_init");

		ret = pthread_cond_init(&que->cond, NULL);
		THREAD_ERROR_CHECK(ret, "pthread_cond_init");
	}
}

void queueDestroy(task_queue_t* que)
{
	if(que) {
		int ret = pthread_mutex_destroy(&que->mutex);
		THREAD_ERROR_CHECK(ret, "pthread_mutex_destroy");

		ret = pthread_cond_destroy(&que->cond);
		THREAD_ERROR_CHECK(ret, "pthread_cond_destroy");
	}
}

int getTaskSize(task_queue_t * que)
{	return que->queSize;	}

int queueIsEmpty(task_queue_t *que)
{	return que->queSize == 0;	}

//入队操作
void taskEnqueue(task_queue_t* que, int peerfd, MsgType taskType, char* recv_command)
{
	int ret = pthread_mutex_lock(&que->mutex);
	THREAD_ERROR_CHECK(ret, "pthread_mutex_lock");
	
    task_t * ptask = (task_t*)calloc(1, sizeof(task_t));
	ptask->peerfd = peerfd;
    ptask->taskType = taskType;

    memset(ptask->command, '\0', sizeof(ptask->command));
    strncpy(ptask->command, recv_command, strlen(recv_command));
	
    ptask->pnext = NULL;
	
    if(queueIsEmpty(que)) {
		que->pFront = que->pRear = ptask;
	}else {
		que->pRear->pnext = ptask;	
		que->pRear = ptask;
	}
	++que->queSize;
	ret = pthread_mutex_unlock(&que->mutex);
	THREAD_ERROR_CHECK(ret, "pthread_mutex_unlock");
	//通知消费者线程取任务
	ret = pthread_cond_signal(&que->cond);
}

//出队操作
void taskDequeue(task_queue_t * que, task_t* ptask)
{
    ptask->pnext=NULL;
	int ret = pthread_mutex_lock(&que->mutex);
	THREAD_ERROR_CHECK(ret, "pthread_mutex_lock");
	//当队列为空时，进入等待状态
	while(!que->exitFlag && queueIsEmpty(que)) {//防止虚假唤醒
		pthread_cond_wait(&que->cond, &que->mutex);
	}

	if(!que->exitFlag) {
		task_t * pFree = NULL;
		ptask->peerfd= que->pFront->peerfd;
		ptask->taskType= que->pFront->taskType;
		strcpy(ptask->command, que->pFront->command);
		
        pFree = que->pFront;
		if(getTaskSize(que) > 1) {
			que->pFront = que->pFront->pnext;
		} else {
			pFree = que->pFront;
			que->pFront = que->pRear = NULL;
		}
		free(pFree);
		--que->queSize;
	}
	ret = pthread_mutex_unlock(&que->mutex);
	THREAD_ERROR_CHECK(ret, "pthread_mutex_unlock");

	return;
}

void queueWakeup(task_queue_t* que) 
{
	que->exitFlag = TASKQUEUE_READY_EXIT_FLAG;
	int ret = pthread_cond_broadcast(&que->cond);
	THREAD_ERROR_CHECK(ret, "pthread_cond_broadcast");
}


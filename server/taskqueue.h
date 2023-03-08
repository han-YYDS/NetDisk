 ///
 /// @file    taskqueue.h
 /// @author  lemon(haohb13@gmail.com)
 /// @date    2022-06-06 10:20:23
 ///
 
#ifndef __WD_TASKQUEUE_H__
#define __WD_TASKQUEUE_H__

#include <pthread.h>
#include "tcp.h"


#define TASKQUEUE_NOT_EXIT_FLAG 0
#define TASKQUEUE_READY_EXIT_FLAG 1


typedef struct task_s{
	int peerfd;
    MsgType taskType;
    char command[100];
	struct task_s * pnext;
} task_t;

typedef struct taskqueue_s{
	task_t * pFront;
	task_t * pRear;
	int queSize;
	int exitFlag;//退出的标志位
	pthread_mutex_t mutex;
	pthread_cond_t cond;
} task_queue_t;
 
void queueInit(task_queue_t *);
void queueDestroy(task_queue_t*);
int queueIsEmpty(task_queue_t *);
int getTaskSize(task_queue_t *);
void taskEnqueue(task_queue_t*, int peerfd, MsgType taskType, char* recv_command);
void taskDequeue(task_queue_t*, task_t* task);
void queueWakeup(task_queue_t*);
 
#endif

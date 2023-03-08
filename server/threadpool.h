 ///
 /// @file    processpool.h
 /// @author  lemon(haohb13@gmail.com)
 /// @date    2022-06-01 20:20:27
 ///
 
#ifndef __WD_THREADPOOL_H__
#define __WD_THREADPOOL_H__

#include "tcp.h"
#include "taskqueue.h"

#include <func.h>

#define MAXCONNS 1000

typedef struct {
	pthread_t * threads;
	int threadNumber;
	task_queue_t queue;
} thread_pool_t, *pThread_pool_t;


void * threadFunc(void*);

void threadpoolInit(thread_pool_t*, int);
void threadpoolDestroy(thread_pool_t*);
void threadpoolStart(thread_pool_t * pthreadpool);
 
int transferFile(int peerfd, const char * filename);
void handleTask(task_t* ptask); 
void handleRegisterTask(task_t *ptask);
void handleLoginTask(task_t *ptask);
void handleCommandTask(task_t *ptask);

void cmd_ls(int peerfd);
void cmd_cd(int peerfd, char* parameter); 
void cmd_pwd(int peerfd);
void cmd_puts(int peerfd, char* parameter);
void cmd_gets(int peerfd, char* parameter);
void cmd_rm(int peerfd, char* parameter);
void cmd_mkdir(int peerfd, char* parameter);
void cmd_quit(int peerfd);

char *trans_file_access_form(void* file_stat);
int parse_pwd(char* pwd);
void pwd_up(char* pwd);
int delete_dir(int dir_id);

#endif

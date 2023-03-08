#include "threadpool.h"
#include "tools.h"
#include "tcp.h"
#include "userlist.h"
#include "slotlist.h"
#include "transferfile.h"
#include "server2.h"

#include "hashmap.h"

#define KEY_MAX_LENGTH 8
#define TIMEOUT 60

typedef struct item_s
{
    char key[KEY_MAX_LENGTH];
    int index;
} item_t;


epfd_t epfd;
int exitpipe[2];
UserList* userList;
SlotList* circle[TIMEOUT + 1];

char ip[16];
uint16_t port;
int threadNum;
int cur_index;
map_t mymap;

void * clockloop(void *arg);
void sigfunc(int num) ;
int epollEventLoop(epfd_t* epfd, int listenfd, thread_pool_t * threadpool);
 
int main(int argc, char* argv[])
{
    //./server ../conf/server.conf
    ARGS_CHECK(argc,2);

    // 加载服务器配置
    memset(ip,'\0',sizeof(ip));
    load_config_file(argv[1], ip, &port, &threadNum);
    printf("ip = %s, port = %d, threadNum = %d\n\n", ip, port, threadNum);

	signal(SIGUSR1, sigfunc);//10号信号
    
    pipe(exitpipe);//创建退出的管道

	pid_t pid = fork();
	if(pid > 0) {
		close(exitpipe[0]);
		wait(NULL);
		exit(0);
	}
	
    // 子进程
	close(exitpipe[1]);
    thread_pool_t threadpool;
	threadpoolInit(&threadpool, threadNum);
	threadpoolStart(&threadpool);

	// 创建TCP的监听套接字listenfd
	int listenfd = tcpServerInit(ip, port);

    // 创建子线程 server2 用来上传、下载文件的子线程
    pthread_t server2_tid;
    tcp_t server2_val;
    
    strcpy(server2_val.ip, ip);
    server2_val.port = 9999;
    
    int ret = pthread_create(&server2_tid, NULL, server2, (void *)&server2_val);
    THREAD_ERROR_CHECK(ret, "pthread_create");

    // 初始化已经登录的用户链表
    userList = create_userlist();

    // 初始化circle[]
    for(int i = 0; i < TIMEOUT + 1 ; i++){
        circle[i] = create_slotlist(); 
        circle[i]->size = 0;
    }
    
    // 创建子线程 clockloop 用来计时
    pthread_t clock_tid;
    
    ret = pthread_create(&clock_tid, NULL, clockloop, NULL);
    THREAD_ERROR_CHECK(ret, "pthread_create");
    
    // 创建一个hashmap, 用来存放peerfd和index
    mymap = hashmap_new();
    

    // 事件监听大循环
    epollEventLoop(&epfd, listenfd, &threadpool);


    // 回收资源
    hashmap_free(mymap);

    ret = pthread_join(server2_tid, NULL);
    THREAD_ERROR_CHECK(ret, "pthread_join");
    
    ret = pthread_join(clock_tid, NULL);
    THREAD_ERROR_CHECK(ret, "pthread_join");
    
    return 0;
}

// clock
void * clockloop(void *arg){
	// 启动server2
    printf("clockloop is running\n");
    printf("clockloop thread id: %ld\n", pthread_self());
    
    epfd_t epfdnull;
    epollCreate(&epfdnull);

    cur_index = 0;
    while(1){
        // 1s响应一次
        //epoll_wait(epfdnull.fd, NULL, 10, 1000);
        sleep(1);
        //printf("1s pass ..., cur_index = %d\n", cur_index);
        
        cur_index++;
        if(cur_index > TIMEOUT){
            cur_index = 0;
        }

        //遍历当前slot里的fd ，断开这些fd的连接,并从slot中删除，并更新userList链表
        slotnode_t* curr = circle[cur_index]->head;
        while(curr != NULL){
            //从用户链表中删除
            delete_userlist_node(userList, curr->peerfd);
            
            //从hashmap中删除
            item_t* tempItem;
            tempItem =(item_t*) malloc(sizeof(item_t));
            sprintf(tempItem->key, "%d", curr->peerfd);
            hashmap_remove(mymap, tempItem->key);

            //关闭连接
            close(curr->peerfd);
            printf("message timeout: close fd = %d\n", curr->peerfd);
            
            slotnode_t* next = curr->next;
            free(curr);
            curr = next;
        }
        
        circle[cur_index]->head = NULL;
    
    }
}


int epollEventLoop(epfd_t* epfd, int listenfd, thread_pool_t * threadpool) {
	// 创建epoll的实例, 并注册(监听)相应文件描述符上的事件
	epollCreate(epfd);
	epollAddReadEvent(epfd, listenfd);
	epollAddReadEvent(epfd, exitpipe[0]);

	struct epoll_event * pevtList = (struct epoll_event*)
		calloc(MAXCONNS, sizeof(struct epoll_event));

    // 主线程 server1
    char recvbuf[NETBUFSIZE];
	int nready;
    while(1) {
		do {
			// epoll等待事件就绪
			nready = epoll_wait(epfd->fd, pevtList, MAXCONNS, 5000);
		
        }while(-1 == nready && errno == EINTR);

		if(0 == nready) {
			printf(">> server1 epoll timeout.\n");
			continue;
		} else if(-1 == nready) {
			perror("epoll_wait");
			return EXIT_FAILURE;
		} else {            // nready > 0
			
            for(int i = 0; i < nready; ++i) {
                
                int fd = pevtList[i].data.fd;
				
                if(fd == listenfd) {
                    //  处理新到来的客户端
					int peerfd = accept(listenfd, NULL, NULL);
                    if(peerfd < 0) {
						perror("accept");
					}
                    
                    // 加入slot 和 map
                    int index = cur_index - 1;
                    add_slotlist_before_head(circle[index], peerfd);

                    item_t* newitem;
                    newitem =(item_t*) malloc(sizeof(item_t));
                    snprintf(newitem->key, KEY_MAX_LENGTH, "%d", peerfd);
                    newitem->index = index;
                    hashmap_put(mymap, newitem->key, newitem);
                    printf("init fd: %s index: %d\n", newitem->key, newitem->index);

                    // 转到登录注册界面 
                    char temp[200] = "-------------------登录注册界面-------------------\n1. 登录\n2. 注册\n请输入'1'或'2':";
                    send_message(peerfd, temp, COMMON);
                    
                    epollAddReadEvent(epfd, peerfd);
                    printf("server1 accept a new conn, peerfd: %d\n", peerfd);

                    char record[100];
                    sprintf(record, "server1 accpet a new conn, fd = %d", peerfd);
                    log_client(record);
                
                } else if(pevtList[i].events & EPOLLIN){ //新消息到来
                    // 更新该连接与超时相关的信息
                    // 从hashmap中查找fd在哪个index
                    item_t* olditem;
                    olditem =(item_t*) malloc(sizeof(item_t));
                    sprintf(olditem->key, "%d", fd);
                    hashmap_get(mymap, olditem->key, (void**)(&olditem));
                    int old_index = olditem->index;
                    printf("old fd: %s index: %d\n", olditem->key, olditem->index);

                    // 从该index中删除该fd
                    delete_slotlist_node(circle[old_index], fd);

                    // 重新将fd加入slot
                    int new_index = cur_index - 1;
                    add_slotlist_before_head(circle[new_index], fd);

                    // 更新hashmap
                    hashmap_remove(mymap, olditem->key);
                    
                    item_t* newitem;
                    newitem =(item_t*) malloc(sizeof(item_t));
                    snprintf(newitem->key, KEY_MAX_LENGTH, "%d", fd);
                    newitem->index = new_index;
                    hashmap_put(mymap, newitem->key, newitem);
                    printf("new fd: %s index: %d\n", newitem->key, newitem->index);

                    //
                    // 接收和处理客户端的消息
                    int ret ;
                    
                    //ret = recv(fd, &length, 4, MSG_DONTWAIT);
                    //ret = recv(fd, &msgType, 4, MSG_DONTWAIT);
                    memset(recvbuf,'\0', sizeof(recvbuf));
                    ret = recv(fd, recvbuf, NETBUFSIZE, MSG_DONTWAIT);
                    if(ret == 0){
                        epollDelEvent(epfd, fd);
                        continue;
                    }

                    //printf("recvbuf: %s\n", recvbuf);
                    
                    //判断是那种类型任务
                    if(strcmp(recvbuf,"1\n")==0){
                        //login
                        if(search_userlist(userList, fd) == -1){
                            // 删除监听
                            epollDelEvent(epfd, fd);
					    
                            //让fd,taskType,command加入任务队列
                            taskEnqueue(&threadpool->queue, fd, TASK_LOGIN, recvbuf);
                        
                        }
                    }else if(strcmp(recvbuf,"2\n")==0){
                        //register
                        if(search_userlist(userList, fd) == -1){
                            // 删除监听
                            epollDelEvent(epfd, fd);
                        
                            //让fd,taskType,command加入任务队列
					        taskEnqueue(&threadpool->queue, fd, TASK_REGISTER, recvbuf);
                        }

                    }else{
                        //command
                        if(search_userlist(userList, fd) != -1){
                            // 删除监听
                            epollDelEvent(epfd, fd);
                        
                            //让fd,taskType,command加入任务队列
					        //printf("have command task: %s\n", recvbuf);
                            taskEnqueue(&threadpool->queue, fd, TASK_COMMAND, recvbuf);

                        }
                        
                    }
                } else if(fd == exitpipe[0]) {
					char exitflag;
					read(exitpipe[0], &exitflag, 1);
					printf("parent process ready to exit.\n");
					
                    //收到退出的通知之后，子线程要逐步退出
					queueWakeup(&threadpool->queue);
					for(int j = 0; j < threadNum; ++j) {
						pthread_join(threadpool->threads[j], NULL);
					}
					printf("parent process pool exit\n");
					threadpoolDestroy(threadpool);
					exit(0);
                }
            }
        }
    }

    free(pevtList); 
	close(listenfd);
	close(epfd->fd);

    return 0;
}


void sigfunc(int num) 
{
	printf("sig %d is coming\n", num);
	//通过管道通知父进程退出
	char ch = 1;
	write(exitpipe[1], &ch, 1);
}



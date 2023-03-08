 ///
 /// @file    tcp.c
 /// @author  lemon(haohb13@gmail.com)
 /// @date    2022-05-28 08:26:36
 ///
 

#include "tcp.h"

void setNonblock(int fd)
{ 
	int flags = fcntl(fd, F_GETFL, 0); 
	flags |= O_NONBLOCK;
	fcntl(fd, F_SETFL, flags);
}

struct sockaddr_in inetaddress(const char * ip, uint16_t port)
{
	struct sockaddr_in serveraddr;
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(port);
	serveraddr.sin_addr.s_addr = inet_addr(ip);
	return serveraddr;
}

int socketInit()
{
	int listenfd = socket(AF_INET, SOCK_STREAM, 0); 
	ERROR_CHECK(listenfd, -1, "socket");
	return listenfd;
}

int tcpServerInit(char * ip, uint16_t port)
{
	int ret;
	
    //1. 创建tcp套接字
	int listenfd = socketInit();

	int reuse = 1;
	if(setsockopt(listenfd, SOL_SOCKET, 
			SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) 
	{	perror("setsockopt");	}

	//2. 绑定网络地址
	struct sockaddr_in serveraddr = inetaddress(ip, port);
	ret = bind(listenfd, 
			(struct sockaddr*)&serveraddr, 
			sizeof(serveraddr));
	ERROR_CHECK(ret, -1, "bind");

	//3. 进行监听
	ret = listen(listenfd, 100);
	ERROR_CHECK(ret, -1, "listen");
	printf("listen %s : %d\n",ip, port);
	return listenfd;
}

int sendn(int fd, const void * buf, int len)
{
	int left = len;
	char * pbuf = (char *)buf;
	int ret;
	while(left > 0) {
		ret = send(fd, pbuf, left, 0);
		if(ret < 0) {
			perror("send");
			break;
		} else {
			pbuf += ret;
			left -= ret;
		}
	}
	return len - left;
}

int send_message(int peerfd, char* msgContent, MsgType msgType ){
    train_t train;

    memset(train.data, '\0', sizeof(train.data));                                                                   
    strcpy(train.data, msgContent);
    train.length = strlen(train.data);                     
    train.msgType = msgType;
    sendn(peerfd, &train, 8+train.length);

    return 1;
}

void channelsInit(channel_t * p, int length)
{
	for(int i = 0; i < length; ++i) {
		channelInit(p + i);
	}
}

void channelInit(channel_t * p) {
	if(p) {
		p->sockfd = -1;//未使用
		p->recvbufSize = 0;
		p->sendbufSize = 0;
		memset(p->recvbuff, 0, BUFFSIZE);
		memset(p->sendbuff, 0, BUFFSIZE);
	}
}

void channelDestroy(channel_t * p)
{
	if(p) {
		close(p->sockfd);
		channelInit(p);
	}
}

void channelAdd(channel_t * p, int length, int fd)
{
	for(int i = 0; i < length; ++i) {
		if(p[i].sockfd == -1) {
			p[i].sockfd = fd;//保存下来
			clearRecvBuff(p + i);
			clearSendBuff(p + i);
			break;
		}
	}
}

void channelDel(channel_t * p, int length, int fd)
{
	int idx = channelGetIndex(p, length, fd);
	if(idx != -1) {
		channelDestroy(p + idx);
	}
}

//>=0 查找到了某一个fd
//-1  表示没有找到
int channelGetIndex(channel_t* p, int length, int fd)
{
	if(p) {
		for(int i = 0; i < length; ++i) {
			if(p[i].sockfd == fd) {
				return i;//查找到了
			}
		}
		return -1;//没有找到
	}
}

void clearRecvBuff(channel_t * p)
{
	if(p) {
		p->recvbufSize = 0;
		memset(p->recvbuff, 0, BUFFSIZE);
	}
}

void clearSendBuff(channel_t * p)
{
	if(p) {
		p->sendbufSize = 0;
		memset(p->sendbuff, 0, BUFFSIZE);
	}
}

// epoll相关操作进行封装
void epollCreate(epfd_t* epfd )
{
	epfd->fd = epoll_create1(0);
	ERROR_CHECK(epfd->fd, -1, "epoll_create1");
	
    int ret = pthread_mutex_init(&epfd->mutex, NULL);
    THREAD_ERROR_CHECK(ret, "pthread_mutex_init");

}

void epollAddReadWriteEvent(epfd_t* epfd, int fd)
{
    int ret = pthread_mutex_lock(&epfd->mutex);
	THREAD_ERROR_CHECK(ret, "pthread_mutex_lock");  
    
    struct epoll_event evt;
	evt.data.fd = fd;
	evt.events = EPOLLIN | EPOLLOUT;
	if(epoll_ctl(epfd->fd, EPOLL_CTL_ADD, fd, &evt) < 0) {
		perror("epoll_ctl");
	}
    
    ret = pthread_mutex_unlock(&epfd->mutex);
    THREAD_ERROR_CHECK(ret, "pthread_mutex_unlock");

}

void epollAddReadEvent(epfd_t* epfd, int fd)
{
    int ret = pthread_mutex_lock(&epfd->mutex);
	THREAD_ERROR_CHECK(ret, "pthread_mutex_lock");  
	
    struct epoll_event evt;
	evt.data.fd = fd;
	evt.events = EPOLLIN;
	if(epoll_ctl(epfd->fd, EPOLL_CTL_ADD, fd, &evt) < 0) {
		perror("epoll_ctl");
	}
    
    ret = pthread_mutex_unlock(&epfd->mutex);
    THREAD_ERROR_CHECK(ret, "pthread_mutex_unlock");
}

//初始情况下，监听的是读写事件
//现在希望他只监听读事件
void epollClearWriteEvent(epfd_t* epfd, int fd)
{
    int ret = pthread_mutex_lock(&epfd->mutex);
	THREAD_ERROR_CHECK(ret, "pthread_mutex_lock");  
	
    struct epoll_event evt;
	evt.data.fd = fd;
	evt.events = EPOLLIN;
	if(epoll_ctl(epfd->fd, EPOLL_CTL_MOD, fd, &evt) < 0) {
		perror("epoll_ctl");
	}
    
    ret = pthread_mutex_unlock(&epfd->mutex);
    THREAD_ERROR_CHECK(ret, "pthread_mutex_unlock");
}

//初始情况下，监听的是读事件
//现在希望他监听读写事件
void epollSetWriteEvent(epfd_t* epfd, int fd)
{
    int ret = pthread_mutex_lock(&epfd->mutex);
	THREAD_ERROR_CHECK(ret, "pthread_mutex_lock");  
	
    struct epoll_event evt;
	evt.data.fd = fd;
	evt.events = EPOLLIN | EPOLLOUT;
	if(epoll_ctl(epfd->fd, EPOLL_CTL_MOD, fd, &evt) < 0) {
		perror("epoll_ctl");
	}
    
    ret = pthread_mutex_unlock(&epfd->mutex);
    THREAD_ERROR_CHECK(ret, "pthread_mutex_unlock");
}

void epollDelEvent(epfd_t* epfd, int fd)
{
    int ret = pthread_mutex_lock(&epfd->mutex);
	THREAD_ERROR_CHECK(ret, "pthread_mutex_lock");  
	
    struct epoll_event evt;
	evt.data.fd = fd;
	if(epoll_ctl(epfd->fd, EPOLL_CTL_DEL, fd, &evt) < 0) {
		perror("epoll_ctl");
	}
    
    ret = pthread_mutex_unlock(&epfd->mutex);
    THREAD_ERROR_CHECK(ret, "pthread_mutex_unlock");
}



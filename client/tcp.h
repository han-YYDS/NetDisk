#ifndef __WD_TCPHEADER_H__
#define __WD_TCPHEADER_H__

#include <func.h>

#define BUFFSIZE 2000
#define NETBUFSIZE 5000   

typedef enum e_MsgType{
    COMMON,
 
    SALT,
 
    DOWNLOAD,
    UPLOAD,
 
    TASK_REGISTER,
    TASK_LOGIN,
    TASK_COMMAND,
 
    INVALID_MESSAGE,
    TOKEN,
    OK,
    REFUSE
}MsgType;

typedef struct thread_arg_s{
    char ip[16];
    int port;
    MsgType msgType;
    char filename[50];
    char jwt[300];
}thread_arg_t;  

                                                                                                           
typedef struct  {
    int length; 
    int msgType;
    char data[NETBUFSIZE];
} train_t;


//抽象出一个通道层
typedef struct Channel {
	int  sockfd;
	int  recvbufSize;		//接收缓冲区已经存放了多少数据
	char recvbuff[BUFFSIZE];//应用层接收缓冲区
	int  sendbufSize;		//发送缓冲区还有多少数据
	char sendbuff[BUFFSIZE];//应用层发送缓冲区
} channel_t;

void setNonblock(int fd);

int socketInit();

struct sockaddr_in inetaddress(const char * ip, uint16_t port);

int tcpServerInit(char * ip, uint16_t port);

int sendn(int fd, const void * buf, int len);
int send_message(int peerfd, char* msgContent, MsgType msgType);

void channelsInit(channel_t * p, int length);

//针对于某一个通道的销毁
void channelInit(channel_t * p);
void channelDestroy(channel_t * p);
void clearRecvBuff(channel_t * p);
void clearSendBuff(channel_t * p);
//通道数组中增删查
void channelAdd(channel_t * p, int length, int fd);
void channelDel(channel_t * p, int length, int fd);
//>=0 查找到了某一个fd
//-1  表示没有找到
int channelGetIndex(channel_t* p, int length, int fd);
 
//epoll相关操作进行封装
int epollCreate();

void epollAddReadWriteEvent(int epfd, int fd);
void epollAddReadEvent(int epfd, int fd);
void epollClearWriteEvent(int epfd, int fd);
void epollSetWriteEvent(int epfd, int fd);
void epollDelEvent(int epfd, int fd);

//void epollHandleReadEvent(int epfd, int fd);
//void epollHandleWriteEvent(int epfd, int fd);
 
#endif

#include "threadFunc.h"
#include "tcp.h"
#include "transferfile.h"

extern char jwt[300];

void * threadFunc(void * arg){
	printf("sub thread is running\n");
	printf("sub thread id: %ld\n", pthread_self());
	
    // 获取 ip、port
    thread_arg_t * server2_arg = (thread_arg_t *)arg;

    // 创建套接字
	int clientfd2 =  socket(AF_INET, SOCK_STREAM, 0);
	ERROR_CHECK(clientfd2, -1, "socket");

	struct sockaddr_in serveraddr;
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(server2_arg->port);
	serveraddr.sin_addr.s_addr = inet_addr(server2_arg->ip);

	// 向服务器server2发起建立连接的请求
	int ret;
    ret = connect(clientfd2, 
			(struct sockaddr *)&serveraddr, 
			sizeof(serveraddr));
	ERROR_CHECK(ret, -1, "connect");
	
    printf("sub thread connect has completed.\n");
	printf("ip: %s, port: %d\n", server2_arg->ip, server2_arg->port);
   
    char recvbuf[NETBUFSIZE];
    int length;
    int msgType;

    // 连接成功, 将jwt发送给服务端server2                                                                                                                         
    send_message(clientfd2, server2_arg->jwt, server2_arg->msgType);
    printf("> send to server2 jwt: %s\n", server2_arg->jwt);

    // 接收服务端server2的验证结果
    ret = recv(clientfd2, &length, 4, MSG_WAITALL);
    ret = recv(clientfd2, &msgType, 4, MSG_WAITALL);
    memset(recvbuf, '\0', sizeof(recvbuf));
    ret = recv(clientfd2, recvbuf, length, MSG_WAITALL);
    
    //printf("> length: %d   msgtype: %d\n", length, msgType);
    
    if(msgType == TOKEN){
        memset(jwt, '\0', sizeof(jwt));
        strcpy(jwt, recvbuf);
        printf("> new jwt: %s\n", recvbuf);
    }

    if(msgType == OK || msgType == TOKEN){
        // 判断上传还是下载
        if( server2_arg->msgType == DOWNLOAD ){
        
            // 开始下载文件
            download_file(clientfd2, server2_arg->filename);       
    
        }else if(server2_arg->msgType == UPLOAD){
        
            // 开始上传文件
            upload_file(clientfd2, server2_arg->filename);    
        }

    }else{
        printf("> jwt认证失败！\n");
    
    }
}



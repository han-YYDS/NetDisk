#define _XOPEN_SOURCE 

#include "cJSON.h"
#include "md5.h"

#include "tcp.h"
#include "tools.h"
#include "transferfile.h"
#include "threadFunc.h"

#define READ_DATA_SIZE	1024
#define MD5_SIZE		16
#define MD5_STR_LEN		(MD5_SIZE * 2)

char jwt[300];
pthread_t server2_tid;
thread_arg_t server2_arg;

void client_loop(char* ip, uint16_t port);
int Compute_file_md5(const char *file_path, char *value);

int main(int argc, char *argv[])
{
    //./client ip port
    ARGS_CHECK(argc, 3);
    
    printf("\033[;47;32m你好，欢迎使用！\033[0m\n");
    
    //while(1){
        client_loop(argv[1],atoi(argv[2]));
    //}

    return 0;
}

void client_loop(char* ip,uint16_t port)
{
	// 创建套接字
	int clientfd =  socket(AF_INET, SOCK_STREAM, 0);
	ERROR_CHECK(clientfd, -1, "socket");

	struct sockaddr_in serveraddr;
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(port);
	serveraddr.sin_addr.s_addr = inet_addr(ip);

	// 向server1服务器发起建立连接的请求
	int ret;
    ret = connect(clientfd, 
			(struct sockaddr *)&serveraddr, 
			sizeof(serveraddr));
	ERROR_CHECK(ret, -1, "connect");
	printf("main thread: connect has completed.\n");
    
    fd_set readfds;
    
    int length=0;
    int msgType=0;
    
    char buff[NETBUFSIZE];
    
    char salt[20];
    char passwd[100];
    char cryptpasswd[100];
    char filename_down[50];
    char filename_up[50];
    
    while(1){
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        FD_SET(clientfd, &readfds);
        
        select(clientfd + 1, &readfds, NULL, NULL, NULL);
            
        if(FD_ISSET(clientfd, &readfds)){
            // 接收数据长度和数据类型
            int retNet;
            retNet = recv(clientfd, &length, 4, MSG_WAITALL);
            retNet = recv(clientfd, &msgType, 4, MSG_WAITALL);
	        
            printf("\033[;47;32mMsgLen:%5d, MsgType:%2d\033[0m\n", length, msgType);
            
            if(msgType == COMMON){          // 普通
	            // 显示短命令效果
                memset(buff, '\0', sizeof(buff));
	            ret = recv(clientfd, buff, length, MSG_WAITALL);
	            printf("%s\n", buff);

            }else if(msgType == SALT){          //salt
	            memset(salt, '\0', sizeof(salt));
	            ret = recv(clientfd, salt, length, MSG_WAITALL);
	            //printf("salt: %s\n",salt);
                printf(">> 请输入密码:\n");

            }else if(msgType == TASK_LOGIN){   // 登录
	            // 接收jwt
                memset(jwt, '\0', sizeof(jwt));
	            ret = recv(clientfd, jwt, length, MSG_WAITALL);
	            printf("jwt: %s\n", jwt);
            
            }else if(msgType == DOWNLOAD){        // 下载文件
                // 接收文件的名字
                memset(filename_down, '\0', sizeof(filename_down));
                ret = recv(clientfd, filename_down, length, MSG_WAITALL);
	            printf("> filename: %s\n", filename_down);

                // 获取端口号，创建子线程
                //pthread_t server2_tid;
                //thread_arg_t server2_arg;

                strcpy(server2_arg.ip, ip);
                server2_arg.port = 9999;
                server2_arg.msgType = DOWNLOAD;
                strcpy(server2_arg.filename, filename_down);
                memset(server2_arg.jwt, '\0', sizeof(server2_arg.jwt));
                strcpy(server2_arg.jwt, jwt);


                // 创建子线程,与server2连接传输文件
	            int retPth = pthread_create(&server2_tid, NULL, threadFunc, (void *)&server2_arg);
	            THREAD_ERROR_CHECK(retPth, "pthread_create");
            
                // 恢复消息类型
                msgType = COMMON;

            }else if(msgType == UPLOAD){         // 上传文件
                // 接收数据内容
	            memset(buff, '\0', sizeof(buff));
	            ret = recv(clientfd, buff, length, MSG_WAITALL);
                
                if(strcmp(buff, "ack") == 0){
                    printf("%s\n", buff);

                    // 获取端口号，创建子线程, 用来传输文件
                    //pthread_t server2_tid;
                    //thread_arg_t server2_arg;

                    strcpy(server2_arg.ip, ip);
                    server2_arg.port = 9999;
                    server2_arg.msgType = UPLOAD;
                    strcpy(server2_arg.filename, filename_up);
                    strcpy(server2_arg.jwt, jwt);
                    
                    // 创建子线程,与server2连接传输文件
	                int retPth = pthread_create(&server2_tid, NULL, threadFunc, (void *)&server2_arg);
	                THREAD_ERROR_CHECK(retPth, "pthread_create");
            
                }else if(strcmp(buff, "skip") == 0){
                    printf("%s\n", buff);

                    // 什么都不用做

                }else{
                    // 接收从服务端返回的filename
                    memset(filename_up, '\0', sizeof(filename_up));
	                strncpy(filename_up, buff, length);

                    // 拼接完整的文件路径
                    char file_path[50];
                    memset(file_path, '\0', sizeof(file_path));
                    strcat(file_path, "../clientware/");
                    strcat(file_path, filename_up);
                    
                    // 打开文件
                    int fileFd = open(file_path, O_RDWR);
	                
                    // 生成md5
                    char md5_str[MD5_STR_LEN + 1];
	                int retVal = Compute_file_md5(file_path, md5_str);

                    if (0 == retVal)
	                {
                        printf("> %s md5: %s\n", filename_up, md5_str);
                   
                        // 获取文件大小
                        struct stat st;
                        fstat(fileFd, &st);
                        printf("> fileSize: %ld\n", st.st_size);

                        //发文件的md5和filesize给对端server1
                        cJSON* root = NULL;
                        char* out = NULL;
                        root = cJSON_CreateObject();
                        cJSON_AddStringToObject(root, "md5", md5_str);
                        cJSON_AddNumberToObject(root, "filesize", st.st_size);
                        out = cJSON_Print(root);
                        cJSON_Delete(root);

                        send_message(clientfd, out, UPLOAD);
	                }

                }

                // 恢复消息类型
                msgType = COMMON;
            }
            
            if(retNet == 0 ){
                // 服务器server1断开连接
                printf("conn %d was broken by server1...\n", clientfd);
                
                // 等待子线程完成传输
                int retPth = pthread_join(server2_tid, NULL);
                THREAD_ERROR_CHECK(retPth, "pthread_join");

                break;
            }
        }
            
        if(FD_ISSET(STDIN_FILENO, &readfds)){
            memset(buff, '\0', sizeof(buff));
            read(STDIN_FILENO, buff, sizeof(buff));
            
            if(strcmp(buff, "\n") != 0){
                
                if(msgType == SALT){ 
                    // 接收键盘输入的密码，与salt加密后发送 
	                memset(passwd, '\0', sizeof(passwd));
                    strncpy(passwd, buff, strlen(buff));
	                passwd[strlen(passwd)-1] = '\0';
                    //printf("passwd: %s\n",passwd);

	                memset(cryptpasswd, '\0', sizeof(cryptpasswd));
                    strcpy(cryptpasswd, crypt(passwd, salt));
                    send(clientfd, cryptpasswd, strlen(cryptpasswd), 0);
                
                }else{
                    // printf("send buff: %s\n", buff);  
                    ret = send(clientfd, buff, strlen(buff), 0);
                
                }
                
                printf("\n");
            }
        }
    }
   
	close(clientfd);
}

// 计算 md5
int Compute_file_md5(const char *file_path, char *md5_str)
{
	int i;
	int fd;
	int ret;
	unsigned char data[READ_DATA_SIZE];
	unsigned char md5_value[MD5_SIZE];
	MD5_CTX md5;

    printf("%s md5 is calculating...\n", file_path);
    
	fd = open(file_path, O_RDONLY);
	if (-1 == fd)
	{
		perror("open");
		return -1;
	}

	// init md5
	MD5Init(&md5);

	while (1)
	{
		ret = read(fd, data, READ_DATA_SIZE);
		if (-1 == ret)
		{
			perror("read");
			return -1;
		}

		MD5Update(&md5, data, ret);

		if (0 == ret || ret < READ_DATA_SIZE)
		{
			break;
		}
	}

	close(fd);

	MD5Final(&md5, md5_value);

	for(i = 0; i < MD5_SIZE; i++)
	{
		snprintf(md5_str + i*2, 2+1, "%02x", md5_value[i]);
	}
	md5_str[MD5_STR_LEN] = '\0'; // add end

	return 0;
}

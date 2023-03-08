 ///
 /// @file    transferfile.c
 /// @author  lemon(haohb13@gmail.com)
 /// @date    2022-06-02 11:22:31
 ///
 
#include <func.h>
#include "threadpool.h"

// 客户端下载文件
void download_file(int peerfd, const char * filename)
{
    printf("> 进入 download file!\n");

    train_t train;
    int ret;
    
    // 读取server本地的文件
    char file_path[50];

    // 拼接完整的文件路径    
    memset(file_path, '\0', sizeof(file_path));
    strcat(file_path, "../serverware/");
    strcat(file_path, filename);
    printf("> file_path: %s\n", file_path);

    // 打开文件
    int fileFd = open(file_path, O_RDWR);

    // 发送文件的大小给对端
    struct stat st;
    fstat(fileFd, &st);
    printf("> fileSize:%ld\n", st.st_size);
    
    char filesize[8];
    memset(filesize, '\0', sizeof(filesize));
    sprintf(filesize, "%ld", st.st_size); 
    
    send_message(peerfd, filesize, DOWNLOAD);

    // 发送文件内容
    off_t total = 0;
	while(total < st.st_size) {
		memset(train.data, '\0', sizeof(train.data));
		
        ret = read(fileFd, train.data, NETBUFSIZE);
		
        if(0 == ret) {
			printf("> %s send over!\n", filename);
			break;
		}
		
        ERROR_CHECK(ret, -1, "read");
		
        train.length = ret;
        train.msgType = DOWNLOAD;
		ret = sendn(peerfd, &train, 8 + train.length);
		
        total += train.length;
		
	}
	
    printf("> %s send over!\n", filename); 
    
    close(fileFd);

}

// 客户端上传文件
int upload_file(int peerfd, const char* filename ){
    printf("> 进入 upload file!\n");
	
    int length;
    int msgType;
	int ret;
    
    //应用层接收缓冲区
	char recvBuff[NETBUFSIZE];
	
	printf("> filename:%s\n", filename);

	// 在服务端本地创建文件
	char file_path[100];
    
    // 拼接完整的文件路径
    memset(file_path, '\0', sizeof(file_path));
    strcat(file_path, "../serverware/");
    strcat(file_path, filename);
    printf("> file_path: %s\n", file_path);

    // 打开文件
    int fileFd = open(file_path, O_CREAT|O_RDWR, 0644);

	// 接收文件的大小
	char fileSize_str[10];
    off_t fileSize;
	
    ret = recv(peerfd, &length, 4, MSG_WAITALL);
	printf("> length: %d\n", length);
	ret = recv(peerfd, &msgType, 4, MSG_WAITALL);
	printf("> MsgType: %d\n", msgType);
    ret = recv(peerfd, fileSize_str, length, MSG_WAITALL);
    fileSize = atol(fileSize_str);
	printf("> fileSize: %ld\n", fileSize);

    // 分成100片，计算每一片的大小
    off_t slice = fileSize / 100;

	// 接收文件的内容
	off_t downloadSize = 0;
	off_t lastSize = 0;
    
    char bar[52];
    memset(bar,'\0',sizeof(bar));
    
    const char *lable="|/-\\";
	
    int i = 0, cnt = 0;
    while(downloadSize < fileSize) {
		ret = recv(peerfd, &length, 4, MSG_WAITALL);
		ret = recv(peerfd, &msgType, 4, MSG_WAITALL);
		
        if(0 == ret) {
			printf("> %s has recv all data\n", filename);
			break;
		}
        
		memset(recvBuff, '\0', sizeof(recvBuff));
		ret = recv(peerfd, recvBuff, length, MSG_WAITALL);
		//printf("recv ret=%d,%d\n",ret,length);
		
        downloadSize += ret;

		if(downloadSize - lastSize >slice ||lastSize==0 ||downloadSize==fileSize) {
			//printf(">> download percent:%5.2lf%%\n", (double)downloadSize / fileSize * 100);
            //打印进度条
            if(cnt % 2 == 0){
                bar[i] = '#';
                i++;
            }
            printf("[%-51s][%d%%] %c\r", bar, cnt, lable[cnt%4]);
            fflush(stdout);
            cnt++;
			lastSize = downloadSize;
        }

		// 写入本地
        ret = write(fileFd, recvBuff, ret);
		//printf("write ret=%d,%d\n",ret,length);
		ERROR_CHECK(ret, -1, "write");
	}
    
    ret = chown(file_path, 1000, 1000);
    ret = chmod(file_path, 0755);

    printf("\n");
    printf("> Upload file completed!\n");

    return fileSize;
}

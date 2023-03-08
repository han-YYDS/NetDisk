#include "server2.h"
#include "tools.h"

#include "l8w8jwt/base64.h"
#include "l8w8jwt/encode.h"
#include "l8w8jwt/decode.h"

// server2
void * server2(void * arg)
{
    // 启动server2
    printf("server2 is running\n");
    printf("server2 thread id: %ld\n", pthread_self());
 
    epfd_t server2_epfd;
	epollCreate(&server2_epfd);

    // 获取 ip、port
    tcp_t * server2 = (tcp_t *)arg;

	// 创建TCP的监听套接字listenfd
	int listenfd = tcpServerInit(server2->ip, server2->port);

	// 创建epoll的实例, 并注册(监听)相应文件描述符上的事件
	epollCreate(&server2_epfd);
	epollAddReadEvent(&server2_epfd, listenfd);

	struct epoll_event * pevtList = (struct epoll_event*)
		calloc(MAXCONNS, sizeof(struct epoll_event));
    
    int ret;
    int nready;
    while(1) {
		do {
			// epoll等待事件就绪
			nready = epoll_wait(server2_epfd.fd, pevtList, MAXCONNS, 5000);
		
        }while(-1 == nready && errno == EINTR);

		if(0 == nready) {
			//printf(">> server2 epoll timeout.\n");
			continue;
		} else if(-1 == nready) {
			perror("epoll_wait");
		} else {
            // nready > 0
			for(int i = 0; i < nready; ++i) {
                
                int fd = pevtList[i].data.fd;
				
                if(fd == listenfd) {
                    //  处理新到的客户端连接
					int peerfd = accept(listenfd, NULL, NULL);
                    if(peerfd < 0) {
						perror("accept");
					}
                    
                    epollAddReadEvent(&server2_epfd, peerfd);
                    printf("server2 accept a new conn, peerfd: %d\n", peerfd);

                }else{
                    //  处理已经建立的客户端连接
                    if(pevtList[i].events & EPOLLIN){
                        printf("server2 EPOLLIN: fd = %d\n", fd);
                        
                        int length;
                        int msgType;
                        char jwt[300];

                        // 接收客户端的jwt
                        ret = recv(fd, &length, 4, MSG_DONTWAIT);
                        ret = recv(fd, &msgType, 4, MSG_DONTWAIT);
                        memset(jwt, '\0', sizeof(jwt));
                        ret = recv(fd, jwt, length, MSG_DONTWAIT);
                        if(ret == 0){
                            epollDelEvent(&server2_epfd, fd);
                            continue;
                        }

                        printf("> server2 recv jwt: %s\n", jwt);
                        
                        // 验证jwt
                        // 解码，得到user_id
                        struct l8w8jwt_decoding_params decoding_params;
                        l8w8jwt_decoding_params_init(&decoding_params);
                    
                        decoding_params.alg = L8W8JWT_ALG_HS256;
                        decoding_params.jwt = jwt;
                        decoding_params.jwt_length = strlen(jwt);
                        decoding_params.verification_key = "123456";
                        decoding_params.verification_key_length = strlen(decoding_params.verification_key);
                        decoding_params.validate_exp = true;
                        decoding_params.exp_tolerance_seconds = 10;

                        size_t claims_length;
                        struct l8w8jwt_claim* claims;
                        enum l8w8jwt_validation_result validation_result;
                        ret = l8w8jwt_decode(&decoding_params, &validation_result, &claims, &claims_length);
                        printf("> server2 validation_result: %d\n", validation_result);

                        struct l8w8jwt_claim* payload_claims = l8w8jwt_get_claim(claims, claims_length, "uid", 3);
                        printf("> server2 payload_claims->value: %s\n", payload_claims->value); 

#if 0
                        int user_id = atoi(payload_claims->value);
                        char sql[100];

                        memset(sql, '\0', sizeof(sql));
                        sprintf(sql, "select id from UserToken where user_id = %d and token = '%s'", user_id, jwt);
                        printf("$$ sql: %s\n", sql);

                        MYSQL_RES * result=myQuery(sql);
  
                        int rows = mysql_num_rows(result);
#endif

                        if(validation_result == L8W8JWT_VALID || validation_result == L8W8JWT_EXP_FAILURE){
                            //epollDelEvent(&server2_epfd, fd);
 				            
                            if(validation_result == L8W8JWT_EXP_FAILURE){
                                printf("> JWT EXP\n");

                                // 重新生成jwt, 10分钟有效
                                char* out;
                                size_t out_length;
                                struct l8w8jwt_encoding_params params;

                                l8w8jwt_encoding_params_init(&params);
                                params.additional_payload_claims = payload_claims;
                                params.additional_payload_claims_count = 1;

                                params.secret_key = "123456";
                                params.secret_key_length = strlen(params.secret_key);
            
                                params.out = &out;
                                params.out_length = &out_length;
           
                                time_t now = time(NULL);
                                time_t expire = now + 10;

                                params.iat = now;
                                params.exp = expire;
                                params.iss = "haizi";
                                params.aud = "admin";
                                params.alg = L8W8JWT_ALG_HS256;

                                l8w8jwt_encode(&params);
                                printf("> server2重新生成jwt: %s\n", out);
                                
                                // 将jwt发送给客户端
                                send_message(fd, out, TOKEN);
                            }else{ 
                                // 验证jwt通过, 给对端发ok
                                char temp[100] = "ok";
                                send_message(fd, temp, OK);
                            }

                            // 接收文件名
                            char filename[50];
                            ret = recv(fd, &length, 4, MSG_WAITALL);
                            ret = recv(fd, &msgType, 4, MSG_WAITALL);
                            memset(filename, '\0', sizeof(filename));
                            ret = recv(fd, filename, length, MSG_WAITALL);
                            
                            if(msgType == DOWNLOAD){
                                //进入下载接口
                                download_file(fd, filename);
                                
                                close(fd);

                            }else if(msgType == UPLOAD){
                                //进入上传接口
                                int filesize = upload_file(fd, filename);
                                printf("> filesize: %d\n", filesize);
                        
                                close(fd);
                            
                            }
                       
                        }else{
                            // 验证jwt未通过
                            char temp[100] = "refuse";
                            send_message(fd, temp, REFUSE);
                            
                            close(fd);
                        
                        }
                    }
                    //处理写事件
				    //if(pevtList[i].events & EPOLLOUT) {
				    //    epollHandleWriteEvent(&server2_epfd, fd);
				    // }
                }
            }
        }
    }   
}

#if 0
void epollHandleWriteEvent(int epfd, int netfd)
{
	//printf("\nbegin send...\n");
	int i = currentChannelIndex(netfd);
	if(i != -1) {
		char * pbuf = channels[i].sendbuff;
		int sendLength = channels[i].sendLength;
		int nsend = send(netfd, pbuf, sendLength, 0);
		if(nsend == sendLength) {
			//1.如果发送缓冲区中的所有数据全部发送完成
			//后续不再监听写事件
			clearSendBuffer(i);
			epollClearWriteEvent(epfd, netfd);
		} else if(nsend > 0 && nsend < sendLength) {
			//2. 如果只发送了部分数据, 将未发送的数据移到开始位置
			channels[i].sendLength = sendLength - nsend;
			memmove(channels[i].sendbuff, channels[i].sendbuff + nsend, sendLength - nsend);
		}
		printf("\nsend over...\n");
	}
}

#endif

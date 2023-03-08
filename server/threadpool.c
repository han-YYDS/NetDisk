#define _XOPEN_SOURCE  

#include "threadpool.h"
#include "transferfile.h"
#include "userlist.h"
#include "tools.h"

#include "cJSON.h"
#include "l8w8jwt/base64.h"
#include "l8w8jwt/encode.h"
#include "l8w8jwt/decode.h"

#define rowNum 8

extern epfd_t epfd;
extern UserList* userList;

char cmds[rowNum][8]={"cd", "ls", "pwd", "puts", "gets", "rm", "mkdir", "quit"};

void threadpoolInit(thread_pool_t* pthreadpool, int threadNum)
{
	pthreadpool->threadNumber = threadNum;
	pthreadpool->threads = (pthread_t*)calloc(threadNum, sizeof(pthread_t));
	queueInit(&pthreadpool->queue);
}

void threadpoolDestroy(thread_pool_t* pthreadpool)
{
	free(pthreadpool->threads);
	queueDestroy(&pthreadpool->queue);
}

void threadpoolStart(thread_pool_t * pthreadpool)
{
	for(int idx = 0; idx < pthreadpool->threadNumber; ++idx) {
		int ret = pthread_create(&pthreadpool->threads[idx],
			NULL, 
			threadFunc, 
			pthreadpool);
		THREAD_ERROR_CHECK(ret, "pthread_create");
	}
}

void * threadFunc(void* arg)
{
	//每一个子线程要做的事儿
	printf("sub thread %ld is runing\n", pthread_self());
	thread_pool_t * pthreadpool = (thread_pool_t*)arg;
	
    while(1) {
        task_t task;
		taskDequeue(&pthreadpool->queue, &task);
        
        if(task.peerfd>0){
            handleTask(&task);
            
            // 任务完成, 让主线程恢复监听
            epollAddReadEvent(&epfd, task.peerfd); 
        }else{
            break;
        }
	}
	printf("sub thread %ld is ready to exit\n", pthread_self());
	pthread_exit(0);
}

void handleTask(task_t* ptask){
    //根据任务类型调用不同的处理函数
    if(ptask->taskType==TASK_REGISTER){
        handleRegisterTask(ptask);
    }else if(ptask->taskType==TASK_LOGIN){
        handleLoginTask(ptask);
    }else if(ptask->taskType==TASK_COMMAND){
        handleCommandTask(ptask);
    }
    
}

void handleRegisterTask(task_t *ptask){
    char recvbuf[NETBUFSIZE];
    char username[30];
    char salt[20];
    char cryptpasswd[100];
    char passwd[100];

    //从客户端获取用户名
    char temp[100] = "-------------------用户注册界面-------------------\n>> 请输入用户名:";
    send_message(ptask->peerfd, temp, COMMON);
    
    memset(recvbuf,'\0',sizeof(recvbuf));
    int ret = recv(ptask->peerfd, recvbuf, sizeof(recvbuf), 0);
    if(ret > 0){
        memset(username,'\0',sizeof(username));
        memcpy(username, recvbuf, getLength(recvbuf));
        username[getLength(username)-1] = '\0';
        printf("username: %s\n", username);
    }

    //从数据库查询用户名是否已经注册
    char sql[200];
    MYSQL_RES* result;
    MYSQL_ROW row;
    
    memset(sql,'\0',sizeof(sql));
    sprintf(sql, "select id  from User where username='%s'", username);
    printf("$$ sql: %s\n", sql);

    result = myQuery(sql);
    
    int rows = mysql_num_rows(result);
    if(rows == 0){
        //从客户端获取密码
        char temp[100] = ">> 请输入密码:";
        send_message(ptask->peerfd, temp, COMMON);
    
        memset(recvbuf, '\0', sizeof(recvbuf));
        ret = recv(ptask->peerfd, recvbuf, sizeof(recvbuf), 0);
        if(ret > 0){
            memset(passwd, '\0', sizeof(passwd));
            memcpy(passwd, recvbuf, getLength(recvbuf));
            passwd[getLength(passwd)-1] = '\0';
            printf("passwd: %s\n", passwd);
        }

        //生成盐值
        memset(salt,'\0',sizeof(salt));
        generate_str(salt);
        printf("salt: %s\n",salt);

        //密码和盐值加密
        strcpy(cryptpasswd, crypt(passwd, salt));
        printf("cryptpasswd: %s\n", cryptpasswd);
    
        //将用户名、盐值、加密密码、pwd存入数据库
        memset(sql, '\0', sizeof(sql));
        sprintf(sql, "insert into User(username, salt, cryptpasswd, pwd) value('%s', '%s', '%s', '/%s/')", username, salt, cryptpasswd, username);
        printf("$$ sql: %s\n", sql);
    
        result=myQuery(sql);
        if(ret > 0){
            printf("插入数据库成功!\n");
        }
    
        strcpy(temp, ">> 注册成功！\n");
        send_message(ptask->peerfd, temp , COMMON);

        //从数据库查询用户id
        memset(sql, '\0', sizeof(sql));
        sprintf(sql, "select id from User where username='%s'", username);
        printf("$$ sql: %s\n", sql);
    
        result=myQuery(sql);
        if(ret > 0){
            printf("查询数据库成功!\n");
        }
    
        row = mysql_fetch_row(result);
    
        int id = atoi(row[0]);
    
        //在虚拟文件表中给用户创建一个文件目录
        memset(sql, '\0', sizeof(sql));
        sprintf(sql, "insert into VirtualFile(parent_id, filename, owner_id, md5, filesize, type) value(0, '%s', %d, 0, 0, 'd')", username, id);
        printf("$$ sql: %s\n", sql);
    
        result = myQuery(sql);
        if(ret > 0){
            printf("插入数据库成功!\n");
        }
    
    }else{
        char temp[100] = "该用户名已被使用！\n";
        send_message(ptask->peerfd, temp, COMMON);

    }

    // 释放结果集
    mysql_free_result(result);

    //回到用户登录注册界面
    strcpy(temp, "-------------------登录注册界面-------------------\n1. 登录\n2. 注册\n请输入'1'或'2':");
    send_message(ptask->peerfd, temp, COMMON);

}

void handleLoginTask(task_t *ptask){
    char recvbuf[NETBUFSIZE];
    char username[30];
    char salt[20];
    char cryptpasswd[100];
    char passwd[100];
    
    //从客户端获取用户名
    char temp[200] = "-------------------用户登录界面-------------------\n>> 请输入用户名:";
    send_message(ptask->peerfd, temp, COMMON);
    
    memset(recvbuf,'\0',strlen(recvbuf));
    int ret = recv(ptask->peerfd, recvbuf, sizeof(recvbuf), 0);
    if(ret > 0){
        memset(username,'\0',sizeof(username));
        memcpy(username,recvbuf,getLength(recvbuf));
        username[getLength(username)-1] = '\0';
        printf("username: %s\n",username);
    }
    
    //从数据库查询salt
    char sql[200];
    MYSQL_RES* result;
    MYSQL_ROW row;
    
    memset(sql,'\0',sizeof(sql));
    sprintf(sql, "select salt from User where username='%s'", username);
    printf("$$ sql: %s\n", sql);

    result = myQuery(sql);
    
    int rows = mysql_num_rows(result);
    if(rows == 0){
        strcpy(temp, "该用户不存在!\n");
        send_message(ptask->peerfd, temp, COMMON);
    
        //回到用户登录注册界面
        strcpy(temp, "-------------------登录注册界面-------------------\n1. 登录\n2. 注册\n请输入'1'或'2':");
        send_message(ptask->peerfd, temp, COMMON);

    }else{
        row = mysql_fetch_row(result);
        memset(salt,'\0', sizeof(salt));
        strcpy(salt, row[0]);
        printf("salt: %s\n", salt);
    
        //将salt发送给客户端
        send_message(ptask->peerfd, salt, SALT);
    
        memset(recvbuf,'\0',strlen(recvbuf));
        ret = recv(ptask->peerfd, recvbuf, sizeof(recvbuf), 0);
        if(ret > 0){
            memset(cryptpasswd,'\0',sizeof(cryptpasswd));
            memcpy(cryptpasswd,recvbuf,getLength(recvbuf));
            printf("cryptpasswd: %s\n",cryptpasswd);
        }
    
        //从数据库查询id, cryptpasswd
        memset(sql,'\0',sizeof(sql));
        sprintf(sql,"select id, cryptpasswd from User where username='%s'", username);
        printf("$$ sql: %s\n", sql);
    
        result=myQuery(sql);
        if(ret > 0){
            printf("查询数据库成功!\n");
        }
    
        row=mysql_fetch_row(result);
    
        int user_id = atoi(row[0]);

        memset(passwd,'\0',sizeof(passwd));
        strcpy(passwd, row[1]);
        printf("用户id: %d, passwd: %s\n", user_id, passwd);
    
        //比较密码是否匹配
        if(strcmp(passwd, cryptpasswd) == 0){
            //将peerfd和用户id保存到已经登录的用户链表
            add_userlist_before_head(userList, ptask->peerfd, user_id);
            printf("userList size: %d\n", userList->size);
            
            printf("用户 %s 登录成功！\n",username);

            // 通知客户端登录成功
            strcpy(temp, "登录成功!\n");
            send_message(ptask->peerfd, temp, COMMON);
        
            // 生成jwt, 10分钟有效
            char* out;
            size_t out_length;
            struct l8w8jwt_encoding_params params;
            struct l8w8jwt_claim payload_claims[] = { {
                                                        .key = "uid",
                                                        .key_length = 3,
                                                        .value = row[0],
                                                        .value_length = strlen(row[0]),
                                                        .type = L8W8JWT_CLAIM_TYPE_INTEGER } };

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
            printf("login生成jwt: %s\n", out);
           
            // 将jwt发送给客户端
            send_message(ptask->peerfd, out, TASK_LOGIN);

#if 0
	//插入UserToken表
            char local[30];
            int year = 0, mon = 0, mday = 0;
            int hour = 0, min = 0, sec = 0;
            
            struct tm *p = localtime(&expire); //取得当地时间
            year = 1900 + p->tm_year;
            mon = 1 + p->tm_mon;
            mday = p->tm_mday;
            hour = p->tm_hour;
            min = p->tm_min;
            sec = p->tm_sec;

            memset(local, '\0', sizeof(local));
            sprintf(local, "%d-%d-%d %.2d:%.2d:%.2d", year, mon, mday, hour, min, sec);

            // UserToken表插入一条记录
            memset(sql, '\0', sizeof(sql));
            sprintf(sql, "insert into UserToken(user_id, token, expire) value(%d, '%s', '%s')", user_id, out, local);
            printf("$$ sql: %s\n", sql);
    
            result = myQuery(sql);
#endif
            
            //转到命令交互界面
            strcpy(temp, "-------------------命令交互界面-------------------\n>> 请输入命令\n支持的命令: cd、ls、pwd、puts、gets、rm、mkdir、quit");
            send_message(ptask->peerfd, temp, COMMON);
    
        }else{
            printf("用户 %s 密码错误！\n", username);
        
            strcpy(temp, "密码错误!\n");
            send_message(ptask->peerfd, temp, COMMON);
    
            //回到用户登录注册界面
            strcpy(temp, "-------------------登录注册界面-------------------\n1. 登录\n2. 注册\n请输入'1'或'2':");
            send_message(ptask->peerfd, temp, COMMON);
        }
    }
    
    // 释放结果集
    mysql_free_result(result);
}

void handleCommandTask(task_t *ptask){
    command_t command;
    
    printf("command: %s\n", ptask->command);
    
    //解析命令
    parse_cmd(ptask->command, &command);
    printf("command_t: %s, %s\n", command.cmd, command.parameter);
    
    int i = 0;
    int flag = 0;
    for(i = 0; i < rowNum && flag == 0; i++){
        if(strcmp(cmds[i], command.cmd) == 0){
            flag = 1;
            switch (i) {
            case 0:
                cmd_cd(ptask->peerfd, command.parameter);
                break;
            case 1:
                cmd_ls(ptask->peerfd);
                break;
            case 2:
                cmd_pwd(ptask->peerfd);
                break;
            case 3:
                cmd_puts(ptask->peerfd, command.parameter);
                break;
            case 4:
                cmd_gets(ptask->peerfd, command.parameter);
                break;
            case 5:
                cmd_rm(ptask->peerfd, command.parameter);
                break;
            case 6:
                cmd_mkdir(ptask->peerfd, command.parameter);
                break;
            case 7:
                cmd_quit(ptask->peerfd);
                break;
            default:
                ;
            }
        }
    }

    if(flag == 0){
        printf("不合法的命令!\n");
       
        char temp[100] = "不合法的命令，请重新输入!";
        send_message(ptask->peerfd, temp, COMMON);

    }
}

void cmd_cd(int peerfd, char* parameter)
{
    char sql[200];
    MYSQL_RES* result;
    
    // 从用户链表中查询用户id
    int user_id = search_userlist(userList, peerfd);

    //从数据库查询该用户的当前工作目录
    char pwd[100];
    memset(pwd,'\0', sizeof(pwd));
    get_pwd(user_id, pwd);
    printf("pwd: %s\n", pwd);

    // 解析 pwd 获取 dir_id
    int dir_id = parse_pwd(pwd);
    printf("dir_id: %d\n", dir_id);

    printf("parameter: %s, length: %ld\n", parameter, strlen(parameter));
    if(strcmp(parameter, "..") == 0){
        pwd_up(pwd);

        // 更新 User 表的 pwd
        memset(sql, '\0', sizeof(sql));
        sprintf(sql, "update User set pwd = '%s' where id = %d", pwd, user_id);
        printf("sql: %s\n", sql);
    
        result = myQuery(sql);
                
        send_message(peerfd, pwd, COMMON);
        
    }else{
        //从数据库查询该用户的当前工作目录下的文件夹
        memset(sql, '\0', sizeof(sql));
        sprintf(sql, "select filename from VirtualFile where filename = '%s' and owner_id = %d and parent_id = %d and type = 'd' ", parameter, user_id, dir_id);
        printf("$$ sql: %s\n", sql);

        result = myQuery(sql);
    
        int rows = mysql_num_rows(result);
        if( rows !=0 ){     // 是否存在该文件夹
            MYSQL_ROW row = mysql_fetch_row(result);
            if(strcmp(row[0], parameter) == 0){
                strcat(pwd, row[0]);
                strcat(pwd, "/");
                printf("pwd: %s\n", pwd);
    
                // 更新 User 表的 pwd
                memset(sql, '\0', sizeof(sql));
                sprintf(sql, "update User set pwd = '%s' where id = %d", pwd, user_id);
                printf("sql: %s\n", sql);
    
                result = myQuery(sql);
            
                send_message(peerfd, pwd, COMMON);
        
            }
        }else{
            char temp[100] = "该文件夹不存在！";
            send_message(peerfd, temp, COMMON);
        }
    }
    
    // 释放结果集
    mysql_free_result(result);

}

void cmd_ls(int peerfd){
    //从数据库查询当前目录下的文件
    char sql[200];
    MYSQL_RES* result;
    MYSQL_ROW row;
    
    // 从用户链表中查询用户id
    int user_id = search_userlist(userList, peerfd);

    //从数据库查询该用户的当前工作目录
    char pwd[100];
    memset(pwd,'\0', sizeof(pwd));
    get_pwd(user_id, pwd);
    printf("pwd: %s\n", pwd);

    // 解析 pwd 获取 dir_id
    int dir_id = parse_pwd(pwd);
    printf("dir_id: %d\n", dir_id);
    
    // 用parent_id、owner_id查询当前工作目录下的文件夹和文件
    memset(sql, '\0', sizeof(sql));
    sprintf(sql, "select filename, owner_id, filesize, type from VirtualFile where parent_id = %d and owner_id = %d", dir_id, user_id);
    printf("sql: %s\n", sql);

    result=myQuery(sql);

    int rows = mysql_num_rows(result);
	int cols = mysql_num_fields(result);
	printf("rows:%d, cols:%d\n", rows, cols);

    // 将查询结果放入发送缓冲区
    char lsBuf[NETBUFSIZE];
    memset(lsBuf, '\0', sizeof(lsBuf));
    int lsBuf_size = 0;
    int ret = 0;

    ret = sprintf(lsBuf, "filename          owner_id   filesize        type\n");
    lsBuf_size += ret;
    while((row = mysql_fetch_row(result)) != NULL) {
        ret = sprintf(lsBuf+lsBuf_size,"%-20s%-10s%-16s%-10s\n", row[0], row[1], row[2], row[3]);
        lsBuf_size += ret;
	}
    
	printf("lsBuf:\n%s\n", lsBuf);
    
    // 释放结果集
    mysql_free_result(result);
    
    // 将结果发送给客户端
    send_message(peerfd, lsBuf, COMMON);
}

void cmd_pwd(int peerfd)
{
    // 从用户链表中查询用户id
    int user_id = search_userlist(userList, peerfd);

    //从数据库查询该用户的当前工作目录
    char pwd[100];
    memset(pwd,'\0', sizeof(pwd));
    get_pwd(user_id, pwd);
    printf("pwd: %s\n", pwd);

    // 解析 pwd 获取 dir_id
    int dir_id = parse_pwd(pwd);
    printf("dir_id: %d\n", dir_id);
    
    // 将结果发送给客户端
    send_message(peerfd, pwd, COMMON);
}

void cmd_puts(int peerfd, char* parameter){
    char buff[NETBUFSIZE];

    //从数据库查询该用户的当前工作目录
    int user_id = search_userlist(userList, peerfd);
    
    char pwd[100];
    memset(pwd,'\0', sizeof(pwd));
    get_pwd(user_id, pwd);
    printf("pwd: %s\n", pwd);
    
    // 解析 pwd 获取 dir_id
    int dir_id = parse_pwd(pwd);
    printf("dir_id: %d\n", dir_id);
    
    // 查看当前工作目录是否存在该文件
    char sql[200];
    MYSQL_RES* result;
    MYSQL_ROW row;
    
    memset(sql,'\0',sizeof(sql));
    sprintf(sql, "select id, filesize from VirtualFile where filename = '%s' and parent_id = %d", parameter, dir_id);
    printf("$$ sql: %s\n", sql);

    result = myQuery(sql);

    int rows = mysql_num_rows(result);
    if(rows == 0){    // 当前目录下没有该文件，可以上传
        // 通知客户端传md5, 和文件大小filesize, 用json
        send_message(peerfd, parameter, UPLOAD);
    
        // 从客户端接收文件的md5 和 filesize
        int length;
        int msgType;
        char md5[100];

        recv(peerfd, &length, 4, MSG_WAITALL);
        recv(peerfd, &msgType, 4, MSG_WAITALL);
        memset(buff, '\0', sizeof(buff));
        recv(peerfd, buff, length, MSG_WAITALL);
        //printf("> buff: %s\n", buff);

        cJSON* root = cJSON_Parse(buff);
        
        memset(md5, '\0', sizeof(md5));
        strcpy(md5, cJSON_GetObjectItem(root, "md5")->valuestring);
        int filesize = cJSON_GetObjectItem(root, "filesize")->valueint;

        cJSON_Delete(root);
        
        printf("> %s md5: %s, filesize: %d\n", parameter, md5, filesize);

        // 查看服务器里是否存在该文件
        memset(sql,'\0',sizeof(sql));
        sprintf(sql, "select id, filesize from VirtualFile where md5 = '%s'", md5);
        printf("$$ sql: %s\n", sql);

        result = myQuery(sql);
        
        rows = mysql_num_rows(result);
        if(rows == 0){
            printf("here1\n");
            //服务器里没有该文件，通知客户端可以传文件,并用json携带server2的端口号9999
            char temp[100] = "ack";
            send_message(peerfd, temp, UPLOAD);
    
            // 等待客户端子线程与server2传输完文件  ???
        
        }else {
            row = mysql_fetch_row(result);
            filesize = atoi(row[1]);

            //通知客户端不用传文件, 实现文件秒传功能
            char temp[100] = "skip";
            send_message(peerfd, temp, UPLOAD);
        }
        
        //更新虚拟文件表, 插入文件记录
        memset(sql, '\0', sizeof(sql));
        sprintf(sql, "insert into VirtualFile(parent_id, filename, owner_id, md5, filesize, type) value(%d, '%s', %d, '%s', %d, 'f')", dir_id, parameter, user_id, md5, filesize);
        printf("$$ sql: %s\n", sql);
    
        result = myQuery(sql);
    
    }else{
        //通知客户端文件已存在
        char temp[100] = "当前文件夹下已有相同的文件";
        send_message(peerfd, temp, COMMON);

    }

    // 释放结果集
    mysql_free_result(result);
    
}

void cmd_gets(int peerfd, char* parameter){
    char sql[200];
    MYSQL_RES* result;
    
    // 从用户链表中查询用户id
    int user_id = search_userlist(userList, peerfd);

    //从数据库查询该用户的当前工作目录
    char pwd[100];
    memset(pwd,'\0', sizeof(pwd));
    get_pwd(user_id, pwd);
    printf("pwd: %s\n", pwd);

    // 解析 pwd 获取 dir_id
    int dir_id = parse_pwd(pwd);
    printf("dir_id: %d\n", dir_id);

    //查看该用户当前目录下的所有文件
    memset(sql, '\0', sizeof(sql));
    sprintf(sql, "select id from VirtualFile where parent_id = %d and filename = '%s' and owner_id = %d and type = 'f'", dir_id, parameter, user_id);
    printf("$$ sql: %s\n", sql);
    
    result = myQuery(sql);
    
    int rows = mysql_num_rows(result);
    if(rows != 0){
        //若存在，用json返回文件名parameter和端口9999，让客户端去连9999端口
        cJSON* root = NULL;
        char* out = NULL;
        root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "filename", parameter);
        cJSON_AddNumberToObject(root, "port", 9999);
        out = cJSON_Print(root);
        cJSON_Delete(root);

        send_message(peerfd, parameter, DOWNLOAD);

    }else{
        char temp[100] = "当前文件夹下没有该文件！";
        send_message(peerfd, temp, COMMON);
    }
    
    // 释放结果集
    mysql_free_result(result);
    
}

void cmd_rm(int peerfd, char* parameter){
    //从数据库查询当前目录下有没有这个文件
    char sql[200];
    MYSQL_RES* result;
    
    // 从用户链表中查询用户id
    int user_id = search_userlist(userList, peerfd);

    //从数据库查询该用户的当前工作目录
    char pwd[100];
    memset(pwd,'\0', sizeof(pwd));
    get_pwd(user_id, pwd);
    printf("pwd: %s\n", pwd);

    // 解析 pwd 获取 dir_id
    int dir_id = parse_pwd(pwd);
    printf("dir_id: %d\n", dir_id);

    //从数据库查询该用户的当前工作目录下的文件夹和文件
    memset(sql, '\0', sizeof(sql));
    sprintf(sql, "select id, filename, type from VirtualFile where filename = '%s' and owner_id = %d and parent_id = %d", parameter, user_id, dir_id);
    printf("$$ sql: %s\n", sql);

    result = myQuery(sql);
    
    MYSQL_ROW row;
    row = mysql_fetch_row(result);

    // 是否存在该文件或文件夹
    if(row != NULL){    
        if(strcmp(row[1], parameter) == 0){
            //判断是文件还是文件夹
            if(strcmp(row[2], "d") == 0){     // 删除文件夹
                //从虚拟文件表中删除该文件夹
                delete_dir(atoi(row[0]));

            }else if(strcmp(row[2], "f") == 0){   //删除文件
                //从虚拟文件表中删除该文件
                memset(sql, '\0', sizeof(sql));
                sprintf(sql, "delete from VirtualFile where owner_id = %d and parent_id = %d and filename = '%s'", user_id, dir_id, parameter);
                printf("$$ sql: %s\n", sql);

                result=myQuery(sql);
            }
            
            // 将结果发送给客户端
            char temp[100] = "删除成功!";
            send_message(peerfd, temp, COMMON);
        }
    
    }else{
        // 将结果发送给客户端
        char temp[100] = "该文件不存在！";
        send_message(peerfd, temp, COMMON);
    
    }
    
    // 释放结果集
    mysql_free_result(result);

}

void cmd_mkdir(int peerfd, char* parameter){
    //从数据库查询当前目录下有没有这个文件
    char sql[200];
    MYSQL_RES* result;
    
    // 从用户链表中查询用户id
    int user_id = search_userlist(userList, peerfd);

    //从数据库查询该用户的当前工作目录
    char pwd[100];
    memset(pwd,'\0', sizeof(pwd));
    get_pwd(user_id, pwd);
    printf("pwd: %s\n", pwd);

    // 解析 pwd 获取 dir_id
    int dir_id = parse_pwd(pwd);
    printf("dir_id: %d\n", dir_id);

    //向virualfile表插入一个文件夹
    memset(sql, '\0', sizeof(sql));
    sprintf(sql, "insert into VirtualFile(parent_id, filename, owner_id, md5, filesize, type) value(%d, '%s', %d, 0, 0, 'd')", dir_id, parameter, user_id);
    printf("$$ sql: %s\n", sql);
    
    result = myQuery(sql);
    
    // 释放结果集
    mysql_free_result(result);
    
    // 将结果发送给客户端
    char temp[100] = "新建文件夹成功!";
    send_message(peerfd, temp, COMMON);

}

void cmd_quit(int peerfd){
    // 从已登录用户链表中删除
    delete_userlist_node(userList, peerfd);
    printf("userList size: %d\n", userList->size);

    //回到用户登录注册界面
    char temp[100] = "-------------------登录注册界面-------------------\n1. 登录\n2. 注册\n请输入'1'或'2':";
    send_message(peerfd, temp, COMMON);

}

// 解析 pwd 获取当前工作目录的文件夹id
int parse_pwd(char* pwd){
    //从数据库查询当前目录下的文件
    char sql[200];
    MYSQL_RES* result;

    char dir_name[50];

    int dir_id;
    int parent_id = 0;

    for(int i = 0; i < getLength(pwd) - 1; i++){
        char ch = pwd[i];
        //printf("ch: %c\n", ch);

        if(ch == '/'){
            i++;
            ch = pwd[i];
            memset(dir_name, '\0', sizeof(dir_name));
            while(ch != '/' && ch != '\0' ){
                concat(getLength(dir_name), dir_name, ch);
                //printf("dir: %d, %s\n", getLength(dir_name), dir_name);
                i++;
                ch = pwd[i];
            }
            i--;
            
            //从数据库查询该文件夹的id
            memset(sql, '\0', sizeof(sql));
            sprintf(sql, "select id from VirtualFile where parent_id = %d and filename = '%s' and type = 'd'", parent_id, dir_name);
            printf("$$ sql: %s\n", sql);
 
            result=myQuery(sql);
 
            MYSQL_ROW row = mysql_fetch_row(result);
            
            dir_id = atoi(row[0]);
            
            parent_id = dir_id;

        }
    }
    
    // 释放结果集
    mysql_free_result(result);

    printf("解析pwd结束！\n");

    return dir_id;
}

void pwd_up(char* pwd){
    
    int cnt = 0;
    for(int i = getLength(pwd)-2; i > 0; i--){
        char ch = pwd[i];
        // printf("ch: %c\n", ch);
        if(ch != '/'){
            cnt++;
        }else{
            break;
        }
    }

    if(getLength(pwd) != (cnt+2)){
        // printf("执行memset\n");
        memset(pwd+getLength(pwd)-cnt-1, '\0', cnt+1);
        printf("pwd: %s\n", pwd);
    }

}

int delete_dir(int dir_id){
    printf("进入delete_dir\n");

    char sql[200];
    MYSQL_RES * result;

    // 查询该文件夹下面所有文件夹的id
    memset(sql, '\0', sizeof(sql));
    sprintf(sql, "select id from VirtualFile where parent_id = %d and type = 'd'", dir_id);
    printf("$$ sql: %s\n", sql);

    result = myQuery(sql);

    MYSQL_ROW row;
    while((row = mysql_fetch_row(result)) != NULL) {
        // 递归删除文件夹
        delete_dir(atoi(row[0]));
    }

    //从虚拟文件表中删除该文件夹下面的文件
    memset(sql, '\0', sizeof(sql));
    sprintf(sql, "delete from VirtualFile where parent_id = %d", dir_id);
    printf("$$ sql: %s\n", sql);

    result = myQuery(sql);

    //从虚拟文件表中删除该文件夹
    memset(sql, '\0', sizeof(sql));
    sprintf(sql, "delete from VirtualFile where id = %d", dir_id);
    printf("$$ sql: %s\n", sql);

    result = myQuery(sql);

    return 1;
}

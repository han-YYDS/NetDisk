#include "tools.h"

// 命令解析
int parse_cmd(char* com_str, command_t* pcom)
{
    //去掉client发过来字符串末尾的换行符；
    int len = strlen(com_str);
    len--;  //不计算换行符长度
    com_str[len] = '\0'; //字符串结束符

    //分离命令 和 [文件名或目录名]
    //存放到command结构体中
    char* p = com_str;
    int i = 0, j = 0;
    while (*p) {
        if (*p != ' ') {
            pcom->cmd[i] = *p;
            i++;
            p++;
        }
        else
            break;
    }
    pcom->cmd[i] = '\0';
    //    printf("com: %s\n", pcom->com);

    p++;
    while(*p) {
        pcom->parameter[j] = *p;
        j++;
        p++;
    }
    pcom->parameter[j] = '\0';
    
    return 0;
}

void error_quit(char *msg) {
	perror(msg);
	exit(-2);
}

//获取盐值
void get_salt(char *salt,char *passwd) {
	int i,j;
	//取出 salt,i 记录密码字符下标,j 记录$出现次数
	for(i=0,j=0;passwd[i] && j != 3;++i) {
	    if(passwd[i] == '$') ++j;
	}
	strncpy(salt,passwd,i-1);
}

//拼接字符串
void concat(int len, char* str, char ch) {
	str += len;
	*str = ch;
	*(++str) = '\0';
	return;
}

//获取字符串的长度
int getLength(char* str) {
	int len = 0, k;
	for (k = 0; k < 2000; k++) {
		if (str[k] != '\0') {
			len++;
		}
		else {
			break;
		}
	}
	return len;
}
 
void load_config_file(char* file,char* ip,uint16_t* pport,int* pprocessNum){
    //加载配置文件
    int filefd=open(file,O_RDONLY);
    char confBuff[1000];
    memset(confBuff,'\0',sizeof(confBuff));
    read(filefd,confBuff,sizeof(confBuff));
    close(filefd);
    //printf("%d\n%s\n",getLength(confBuff),confBuff);

    //获取配置项，和对应的值
    char item[16];
    char value[16];
    memset(item,'\0',sizeof(item));
    memset(value,'\0',sizeof(value));
    int length=getLength(confBuff);
    int cnt=0;
    while(cnt<length){
        char ch = confBuff[cnt];

        if (ch=='\n' || ch == ' ' || ch == '\0' || ch=='\t' || ch=='\r') {
                    ;
        }else if(ch==':'){
            cnt++;
            ch=confBuff[cnt];
            while (ch!='\n') {
                if(ch!=' ' && ch!='\0'){
				    concat(getLength(value),value, ch);      //将ch连接在value后面
                }
				cnt++;                //指针向后移一位
				ch =confBuff[cnt];    //ch变为下一个ch
			}
            cnt--;
            
            if(strcmp(item,"ip")==0){
                memcpy(ip,value,getLength(value));
            }else if(strcmp(item,"port")==0){
                *pport=atoi(value);
            }else if(strcmp(item,"num")==0){
                *pprocessNum=atoi(value);
            }else{
                printf("configuration error\n");
            }
            
            memset(item,'\0',sizeof(item));
            memset(value,'\0',sizeof(value));
        }else{
            while (ch!=':') {
				concat(getLength(item),item, ch);      //将ch连接在item后面
				cnt++;                //指针向后移一位
				ch =confBuff[cnt];    //ch变为下一个ch
			}
            cnt--;
        }

        cnt++;
    }
    
    return;
}

void generate_str(char* salt){
    char str[STR_LEN+1]={0};
    int i,flag;

    srand(time(NULL));
    for(i=0;i<STR_LEN;i++){
        flag=rand()%3;
        switch(flag){
        case 0:str[i]=rand()%26+'a'; break;
        case 1:str[i]=rand()%26+'A'; break;
        case 2:str[i]=rand()%10+'0'; break;
        }
    }

    strcpy(salt, str);
    
    return ;
}

char* GenRandomString(int length) {
    int flag, i;
    char* string;
    
    srand((unsigned) time(NULL ));
    
    if ((string = (char*) malloc(length)) == NULL ) {
        printf("malloc failed!flag:14\n");
        return NULL ;
    }
    
    for (i = 0; i < length+1; i++) {
        flag = rand() % 3;
        switch (flag) {
        case 0: string[i] = 'A' + rand() % 26; break;
        case 1: string[i] = 'a' + rand() % 26; break;
        case 2: string[i] = '0' + rand() % 10; break;
        default: string[i] = 'x'; break;
        }
    }
    string[length] = '\0';
    return string;
}

// 数据库操作
MYSQL_RES* myQuery(char* sql){
    
    MYSQL *conn = NULL;
	
    char *host = "localhost";
	char *user = "root";
	char *mysqlpasswd = "123456";
	char *db = "mynetdisk";

	// 初始化mysql的连接句柄
	conn = mysql_init(NULL);

	// 建立连接(TCP的连接，完成了三次握手的过程, 信息的校验)
	if(mysql_real_connect(conn, host, user, mysqlpasswd, db, 0, NULL, 0) == NULL) {
		printf("error:%s\n", mysql_error(conn));
	}

    // 设置字符集(utf8 支持中文)
    int ret = mysql_query(conn, "set names 'utf8'");
	if(ret != 0) {
		printf("error query1: %s\n", mysql_error(conn));
	}

    // 执行查询
	ret = mysql_query(conn, sql);
	if(ret != 0) {
		printf("error query1: %s\n", mysql_error(conn));
	}

    // 获取结果集, N行数据
	MYSQL_RES * result = mysql_store_result(conn);

	// 关闭连接
	mysql_close(conn);

    return result;
}


void get_pwd(int user_id, char* pwd){
    char sql[100];
    MYSQL_RES* result;

    //从数据库查询该用户的当前工作目录
    memset(sql, '\0', sizeof(sql));                                                       
    sprintf(sql, "select pwd from User where id = %d", user_id);
    printf("$$ sql: %s\n", sql);

    result = myQuery(sql);
    
    MYSQL_ROW row = mysql_fetch_row(result);
    
    //char pwd[100];
    //memset(pwd,'\0', sizeof(pwd));
    strcpy(pwd, row[0]);
    
    //printf("pwd: %s\n", pwd);

}

void log_client(char *msg){

    struct tm *p;

    //char buf[100]={0};
    char timebuf[32]={0};
    char logbuf[200]={0};
   // memset(buf,0,sizeof(buf));

    time_t t=time(NULL);
    p=localtime(&t);

    snprintf(timebuf,32,
             "%04d-%02d-%02d %02d:%02d:%02d",
             p->tm_year+1900,p->tm_mon+1,p->tm_mday,
             p->tm_hour,p->tm_min,p->tm_sec);

    snprintf(logbuf,sizeof(logbuf),"[%s] %s",
             timebuf,msg);

    FILE *fp=fopen("../log/clientlog.txt","a+");
    fprintf(fp,"%s\n",logbuf);

    //send(clientfd,logbuf,sizeof(logbuf),0);

    fclose(fp);
}


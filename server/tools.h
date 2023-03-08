#include <func.h>

#define STR_LEN 10

typedef struct command_s {
    char cmd[10];
    char parameter[100];
}command_t;

int parse_cmd(char* com_str, command_t* pcom);

void error_quit(char *msg);
void get_salt(char *salt,char *passwd);
void concat(int len, char* str, char ch);
int getLength(char* str);
void load_config_file(char* file,char* ip,uint16_t* pport,int* pprocessNum);
void generate_str(char* salt);
char* GenRandomString(int length);

MYSQL_RES* myQuery(char* sql);
void get_pwd(int user_id, char* pwd);

void log_client(char *msg);

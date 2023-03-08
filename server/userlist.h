#include <func.h>

typedef struct user_s {
	int peerfd;
    int user_id;
	struct user_s* prev;
	struct user_s* next;
} user_t;

typedef struct UserList_s {
	user_t* head;
	user_t* rear;
	int size;
    pthread_mutex_t mutex;
} UserList;


// UserList
// 构造函数
UserList* create_userlist();

// 析构函数
void destroy_userlist(UserList* list);

// 头插
void add_userlist_before_head(UserList* list, int peerfd, int user_id);

// 尾插
void add_userlist_behind_rear(UserList* list, int peerfd, int user_id);

// 按索引插入
void add_userlist_node(UserList* list, int index, int peerfd, int user_id);

// 删除第一个与peerfd相等的结点
void delete_userlist_node(UserList* list, int peerfd);

// 查找第一个与peerfd相等结点的user_id
int search_userlist(UserList* list, int peerfd);



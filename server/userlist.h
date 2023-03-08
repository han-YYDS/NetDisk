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
// ���캯��
UserList* create_userlist();

// ��������
void destroy_userlist(UserList* list);

// ͷ��
void add_userlist_before_head(UserList* list, int peerfd, int user_id);

// β��
void add_userlist_behind_rear(UserList* list, int peerfd, int user_id);

// ����������
void add_userlist_node(UserList* list, int index, int peerfd, int user_id);

// ɾ����һ����peerfd��ȵĽ��
void delete_userlist_node(UserList* list, int peerfd);

// ���ҵ�һ����peerfd��Ƚ���user_id
int search_userlist(UserList* list, int peerfd);



#include <func.h>

typedef struct slotnode_s{
    int peerfd;
    struct slotnode_s* prev;
    struct slotnode_s* next;
}slotnode_t;

typedef struct SlotList_s {
	slotnode_t* head;
	slotnode_t* rear;
	int size;
    pthread_mutex_t mutex;
} SlotList;


// SlotList
// 构造函数
SlotList* create_slotlist();

// 析构函数
void destroy_slotlist(SlotList* list);

// 头插
void add_slotlist_before_head(SlotList* list, int peerfd);

// 尾插
void add_slotlist_behind_rear(SlotList* list, int peerfd);

// 按索引插入
void add_slotlist_node(SlotList* list, int index, int peerfd);

// 删除第一个与peerfd相等的结点
void delete_slotlist_node(SlotList* list, int peerfd);

// 查找第一个与peerfd相等结点的user_id
int search_slotlist(SlotList* list, int peerfd);

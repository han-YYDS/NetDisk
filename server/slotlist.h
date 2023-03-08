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
// ���캯��
SlotList* create_slotlist();

// ��������
void destroy_slotlist(SlotList* list);

// ͷ��
void add_slotlist_before_head(SlotList* list, int peerfd);

// β��
void add_slotlist_behind_rear(SlotList* list, int peerfd);

// ����������
void add_slotlist_node(SlotList* list, int index, int peerfd);

// ɾ����һ����peerfd��ȵĽ��
void delete_slotlist_node(SlotList* list, int peerfd);

// ���ҵ�һ����peerfd��Ƚ���user_id
int search_slotlist(SlotList* list, int peerfd);

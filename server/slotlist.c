#include "slotlist.h"

SlotList* create_slotlist() {
	SlotList* list=(SlotList*)calloc(1, sizeof(SlotList));
    
    int ret = pthread_mutex_init(&list->mutex, NULL);
    THREAD_ERROR_CHECK(ret, "pthread_mutex_init");
    
    return list;
}

void add_slotlist_before_head(SlotList* list, int peerfd) {
    int ret = pthread_mutex_lock(&list->mutex);
    THREAD_ERROR_CHECK(ret, "pthread_mutex_lock");

	slotnode_t* newNode = (slotnode_t*)malloc(sizeof(slotnode_t));
	if (newNode == NULL) {
		printf("Error: malloc failed in add_before_head.\n");
		exit(1);
	}
	// ��ʼ��
	newNode->peerfd = peerfd;
	newNode->prev = NULL;
	newNode->next = list->head;
	// ����
	if (list->head != NULL) {
		list->head->prev = newNode;
	}
	// �޸�list������
	list->head = newNode;
	if (list->rear == NULL) {
		list->rear = newNode;
	}
	list->size++;
    
    ret = pthread_mutex_unlock(&list->mutex);
    THREAD_ERROR_CHECK(ret, "pthread_mutex_unlock");

}

void add_slotlist_behind_rear(SlotList* list, int peerfd) {
    int ret = pthread_mutex_lock(&list->mutex);
    THREAD_ERROR_CHECK(ret, "pthread_mutex_lock");
	
    slotnode_t* newNode = (slotnode_t*)malloc(sizeof(slotnode_t));
	if (newNode == NULL) {
		printf("Error: malloc failed in add_before_head.\n");
		exit(1);
	}
	// ��ʼ��
	newNode->peerfd = peerfd;
	newNode->next = NULL;
	newNode->prev = list->rear;
	// ����
	if (list->rear != NULL) {
		list->rear->next = newNode;
	}
	// ����list������
	list->rear = newNode;
	if (list->head == NULL) {
		list->head = newNode;
	}
	list->size++;
    
    ret = pthread_mutex_unlock(&list->mutex);
    THREAD_ERROR_CHECK(ret, "pthread_mutex_unlock");
}

void add_slotlist_node(SlotList* list, int index, int peerfd) {
    int ret = pthread_mutex_lock(&list->mutex);
    THREAD_ERROR_CHECK(ret, "pthread_mutex_lock");
	
    // ������
	if (index < 0 || index > list->size) {
		printf("Error: index must in [0, %d]\n", list->size);
		return;
	}
	// ������
	slotnode_t* newNode = (slotnode_t*)malloc(sizeof(slotnode_t));
	if (newNode == NULL) {
		printf("Error: malloc failed in add_before_head.\n");
		exit(1);
	}
	newNode->peerfd = peerfd;

	if (index == list->size) {
		newNode->next = NULL;
		newNode->prev = list->rear;
		// ����
		if (list->rear != NULL) {
			list->rear->next = newNode;
		}
		// ����list������
		list->rear = newNode;
		if (list->head == NULL) {
			list->head = newNode;
		}
	}
	else {
		// ѭ������ʽ
		slotnode_t* curr = list->head;
		for (int i = 0; i < index; i++) {
			curr = curr->next;
		}
		// ��currǰ�����
		newNode->prev = curr->prev;
		newNode->next = curr;

		if (curr->prev) {
			curr->prev->next = newNode;
		}
		curr->prev = newNode;
		// ���� list ������
		if (index == 0) {
			list->head = newNode;
		}
	}
	list->size++;

    ret = pthread_mutex_unlock(&list->mutex);
    THREAD_ERROR_CHECK(ret, "pthread_mutex_unlock");
}

void delete_slotlist_node(SlotList* list, int peerfd) {
    int ret = pthread_mutex_lock(&list->mutex);
    THREAD_ERROR_CHECK(ret, "pthread_mutex_lock");
	
    slotnode_t* curr = list->head;

	while (curr != NULL) {
		if (curr->peerfd == peerfd) {
			break;
		}
		curr = curr->next;
	}
	// curr == NULL || curr->val = val
	if (curr == NULL) return;

	// ɾ��curr���
	if (curr->prev == NULL) {
		list->head = curr->next;
	}
	else {
		curr->prev->next = curr->next;
		if (curr->next == NULL) {
			list->rear = curr->prev;
		}
		else {
			curr->next->prev = curr->prev;
		}
	}
	
    free(curr);

	// ��������
	list->size--;

    ret = pthread_mutex_unlock(&list->mutex);
    THREAD_ERROR_CHECK(ret, "pthread_mutex_unlock");
}


int search_slotlist(SlotList* list, int peerfd) {
    int index = 0;
	slotnode_t* curr = list->head;

	while (curr != NULL && curr->peerfd != peerfd) { // ��·ԭ��
		index++;
		curr = curr->next;
	}
	// curr == NULL || curr->val == val
	if (curr == NULL) return -1;
	return curr->peerfd;
}

void destroy_slotlist(SlotList* list) {
    int ret = pthread_mutex_destroy(&list->mutex);
    THREAD_ERROR_CHECK(ret, "pthread_mutex_destroy");

    slotnode_t* curr = list->head;
	while (curr != NULL) {
		slotnode_t* next = curr->next;
		free(curr);
		curr = next;
	}
	// �ͷ�list
	free(list);
}

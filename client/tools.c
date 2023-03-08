#include "tools.h"

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

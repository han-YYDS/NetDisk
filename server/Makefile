SRCS:=$(wildcard *.c)
OBJECTS:=$(patsubst %c, %o, $(SRCS))
CC:=gcc
LIBS:=-lpthread
SERVER:=server


$(SERVER):$(SRCS)
	$(CC) $^ -o $@  $(LIBS) -lcrypt -lmysqlclient -ll8w8jwt -lmbedcrypto -lmbedx509 -lpthread -lm -lrt -latomic -ldl -Wall -O0 -g

print:
	echo $(OBJECTS)
clean:
	rm -rf $(OBJECTS) $(SERVER)

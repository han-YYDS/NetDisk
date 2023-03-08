SRCS:=$(wildcard *.c)
OBJECTS:=$(patsubst %c, %o, $(SRCS))
CC:=gcc
LIBS:=-lpthread
CLIENT:=client

$(CLIENT):$(SRCS)
	$(CC) $^ -o $@  $(LIBS) -lcrypt -g

print:
	echo $(OBJECTS)
clean:
	rm -rf $(OBJECTS) $(CLIENT)

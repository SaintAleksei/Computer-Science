TARGET:=test-server-broadcast
INCDIRS:=..
SOURCES:=server.c test_server_broadcast.c
OBJECTS:=$(patsubst %.c,%.o,$(SOURCES))
CFLAGS:=-g -Wall -Wextra -I.. $(addprefix -I,$(INCDIRS))

$(TARGET): $(OBJECTS)
	$(CC) -o $@ $^ -lm

clean:
	rm -f *.o *.d *.log

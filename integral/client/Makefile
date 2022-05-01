TARGET:=test_client_broadcast
INCDIRS:=..
SOURCES:=client.c test_client_broadcast.c
OBJECTS:=$(patsubst %.c,%.o,$(SOURCES))
CFLAGS:=-Wall -Wextra -MMD $(addprefix -I,$(INCDIRS))

$(TARGET): $(OBJECTS)
	$(CC) -o $@ $^ -lm

clean:
	rm -f *.o *.d *.log


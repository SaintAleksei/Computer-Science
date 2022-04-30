TARGET:=client
INCDIRS:=..
SOURCES:=client.c main.c
OBJECTS:=$(patsubst %.c,%.o,$(SOURCES))
CFLAGS:=-Wall -Wextra -MMD $(addprefix -I,$(INCDIRS))

$(TARGET): $(OBJECTS)
	$(CC) -o $@ $^ -lm

clean:
	rm -f *.o *.d *.log


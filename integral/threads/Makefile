TARGET:=threads
INCDIRS:=..
SOURCES:=threads.c main.c
OBJECTS:=$(patsubst %.c,%.o,$(SOURCES))

CFLAGS:= -MMD $(addprefix -I,$(INCDIRS)) -g -O0 -Wall -Wextra -Werror

$(TARGET): $(OBJECTS)
	gcc -o $@ $^ -lpthread -lm

clean:
	rm -f *.o *.d *.log

include $(wildcard *.d)

DIRS := .
PROG := integral

CFLAGS := -MMD $(addprefix -I,$(DIRS)) -g -O0 -Wall -Wextra -Werror
VPATH  := $(DIRS)

$(PROG): $(notdir $(patsubst %.c,%.o,$(wildcard $(addsuffix /*.c,$(DIRS) ) ) ) )
	gcc -o $@ $^ -lpthread -lm

clean:
	rm -f *.log *.log.* *.o *.d $(PROG)

include $(wildcard *.d)

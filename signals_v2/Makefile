flags  :=-g -O0 -Wall -Wextra
dirs   := .
prog   := a.out

VPATH  := $(dirs)

$(prog): $(notdir $(patsubst %.c,%.o,$(wildcard $(addsuffix /*.c,$(dirs) ) ) ) )
	gcc $^ -o $@

%.o: %.c
	gcc -c -MMD $(addprefix -I,$(dirs) ) $(flags) $<

clean:
	rm -f *.o *.d $(prog)

include $(wildcard *.d)

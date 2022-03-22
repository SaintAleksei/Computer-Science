INCDIRS=inc
SRCDIRS=lib test

SRC=$(wildcard $(addsuffix /*.c,$(SRCDIRS)))
OBJ=$(notdir $(patsubst %.c,%.o,$(SRC)))

CFLAGS=--coverage -Wall -Wextra -MMD $(addprefix -I,$(INCDIRS))

VPATH=$(SRCDIRS)

.PHONY: clean test valgrind coverage

test: test-bin
	./test-bin

valgrind: test-bin
	valgrind ./test-bin

test-bin: $(OBJ)
	gcc -o test-bin --coverage $^

coverage: test
	gcov $(notdir $(patsubst %.gcda,%.c,$(SRC)))

clean:
	rm -f *.o *.d *.gcno *.gcda *.gcov test-bin
	
include $(wildcard *.d)

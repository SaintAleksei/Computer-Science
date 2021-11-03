all: a.out

reciever: test
	./test 2>>log

sender: test
	./test hamlet 2>>log

a.out: main.o	
	gcc main.o

main.o: main.c
	gcc -c main.c -g -Wall -Wextra

clean:
	rm -f *.o .file_* log 
	clear
	

all: prog1
prog1: prog1.c	
	gcc -Wall -fsanitize=address,undefined -o prog1 prog1.c
.PHONY: clean all
clean:
	rm prog1
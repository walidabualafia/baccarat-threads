CFLAGS=-g -Wall -Werror -Wunused -pthread

lab4: lab4.o
	clang -o $@ $^

clean:
	rm -f *.o lab4

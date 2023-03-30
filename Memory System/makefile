clean: OS
	rm *.o
OS: a.o
	gcc -o OS a.o -lpthread
a.o: main.c
	gcc -c main.c -o a.o -lpthread

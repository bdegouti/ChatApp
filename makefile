PROGRAM_NAME = lets-talk
LocalPort = 5050
IP = localhost
RemotePort = 5051

all: lets-talk.c lets-talk.h list.c list.h
	gcc -g -Wall -o $(PROGRAM_NAME) lets-talk.c list.c -lpthread -lm -lrt
Valgrind:
	valgrind --leak-check=yes ./$(PROGRAM_NAME) $(LocalPort) $(IP) $(RemotePort)
clean:
	rm $(PROGRAM_NAME)
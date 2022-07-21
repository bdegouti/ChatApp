#include<stdio.h>
#include<time.h>
#include<math.h>
#include<stdlib.h>
#include<unistd.h>
#include<assert.h>
#include<float.h>
#include<string.h>
#include<ctype.h>
#include<unistd.h>
#include<sys/syscall.h>
#include<errno.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h> 
#include <netdb.h>
#include <pthread.h>
#include <semaphore.h>
#include "list.h"

struct dataArgs{
    int sockfd;
    struct addrinfo* resLoc;
    struct addrinfo* resRem;
};

int setupConnection(char* LocalPort, struct sockaddr_in* sockaddr);
void initializeThreads();
void PrintError();
char* decrypt(char* message, int maxLength);
char* encrypt(char* message, int maxLength);

void checkStatus(struct dataArgs* data);

void* sendOutgoing(void* p);
void* consoleManager();
void* checkForIncoming(void* p);
void* checkIO(void* p);

void initiateExit(struct dataArgs* data, int callingThread);
void freeMessage(void* message);
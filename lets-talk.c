#include "lets-talk.h"

List* incomingList;
List* outgoingList;

const static int NUM_OF_THREADS = 4;
const static int KEY = 1;
const static int MAX_MESSAGE_LENGTH = 4000;

pthread_t* threads;
int Running = 1;
int checkFailed = 1;
int messageReceived;

char* incomingBuff;
pthread_cond_t IncomingListNotEmpty;
pthread_cond_t OutgoingListNotEmpty;
pthread_cond_t CanExit;
pthread_mutex_t OutLock, InLock, ExitLock;

int main(int argc, char* argv[]){
    threads = (pthread_t*) malloc(NUM_OF_THREADS*sizeof(pthread_t));
    pthread_mutex_init(&OutLock, NULL);
    pthread_mutex_init(&InLock, NULL);
    pthread_mutex_init(&ExitLock, NULL);

    pthread_cond_init(&IncomingListNotEmpty, NULL);
    pthread_cond_init(&OutgoingListNotEmpty, NULL);
    pthread_cond_init(&CanExit, NULL);  

    incomingList = List_create();
    outgoingList = List_create();

    incomingBuff = malloc(sizeof(char)*MAX_MESSAGE_LENGTH);
    messageReceived = 0;
    char* IP;
    char* LocalPort;
    char* RemotePort;
    if(argc == 4){
        IP = argv[2];
        LocalPort = argv[1];
        RemotePort = argv[3];

        //adress info for local
        struct addrinfo hints, *res;
        int sockfd;
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_flags = AI_PASSIVE;

        getaddrinfo(NULL, LocalPort, &hints, &res);

        //address info for remote
        struct addrinfo hints2, *resRem;
        memset(&hints2, 0, sizeof hints);
        hints2.ai_family = AF_INET;
        hints2.ai_socktype = SOCK_DGRAM;
        if(strcmp(IP, "Localhost")==0 || strcmp(IP, "localhost")==0){
            hints2.ai_flags = AI_PASSIVE;
        }
        getaddrinfo(IP, RemotePort, &hints2, &resRem);

        //create socket
        sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if(sockfd == -1){
            PrintError();
        }
        if(bind(sockfd, res->ai_addr, res->ai_addrlen)==-1){
            PrintError();
        }
        struct dataArgs* args = (struct dataArgs*) malloc(sizeof(struct dataArgs));
        args->resLoc = res;
        args->resRem = resRem;
        args->sockfd = sockfd;

        printf("Welcome to Lets-Talk! Please type your messages now!\n");
        initializeThreads(args);
    }
    else{
        PrintError();
    }
    return 0;
}

void PrintError(){
    printf("Usage:\n    ./lets-talk <local port> <remote host> <remote port>\nExamples:\n   ./lets-talk 3000 192.168.0.513 3001\n   ./lets-talk 3000 some-computer-name 3001\n");
    exit(0);
}

void* checkIO(void* p){
    struct dataArgs* data = p;
    while(Running){
        char* inputBuff = (char*)calloc(MAX_MESSAGE_LENGTH, sizeof(char));
        if(inputBuff == NULL){
            printf("Error: malloc failed at checkIO\n");
        }
        scanf("%[^\n]%*c", inputBuff);
        if(inputBuff[0] == '!'){
            if(strcmp(inputBuff, "!status")==0){
                checkStatus(data);
            }
            else if(strcmp(inputBuff, "!exit")==0){
                if(sendto(data->sockfd, "!exit\n", 8, 0, data->resRem->ai_addr, data->resRem->ai_addrlen)==-1){
                    printf("Exit command failed to send!\n");
                }

                pthread_mutex_lock(&OutLock);
                pthread_cond_signal(&OutgoingListNotEmpty);
                pthread_mutex_unlock(&OutLock);

                pthread_mutex_lock(&ExitLock);
                pthread_cond_wait(&CanExit, &ExitLock);
                pthread_mutex_unlock(&ExitLock);

                initiateExit(data, 0);
                break;
            }
            else{
                printf("unrecognized command\n");
            }
        }
        else{
            int new_length = strlen(inputBuff);
            inputBuff = realloc(inputBuff, new_length+1);
            List_prepend(outgoingList, inputBuff);

            pthread_mutex_lock(&OutLock);
            pthread_cond_signal(&OutgoingListNotEmpty);
            pthread_mutex_unlock(&OutLock);
        }
    }
    exit(0);
}

void* checkForIncoming(void* p){
    struct dataArgs* data = p;
    while(Running){
        ssize_t mSize = recvfrom(data->sockfd, incomingBuff, MAX_MESSAGE_LENGTH, MSG_CONFIRM, data->resRem->ai_addr, &data->resRem->ai_addrlen);
        //printf("Incoming:\n");
        if(mSize != -1){
            if(strcmp("Online", incomingBuff)){
                checkFailed = 0;
            }
            if(strcmp("!status\n", incomingBuff)==0){
                char* message = (char*)malloc(7*sizeof(char));
                strcpy(message, "Online");
                encrypt(message, strlen(message));
                //exit(0);
                if(sendto(data->sockfd, message, 6, MSG_CONFIRM, data->resRem->ai_addr, data->resRem->ai_addrlen) == -1){
                    printf("failed to send status message.\n");
                }
            }
            else if(strcmp("!exit\n", incomingBuff)==0){
                initiateExit(data, 1);
                break;
            }
            else{
                decrypt(incomingBuff, mSize);
                //printf("    received: %s\n", incomingBuff);
                char* message = (char*) malloc(sizeof(char)*strlen(incomingBuff));
                strcpy(message, incomingBuff);
                //printf("length of incoming: %d\n", (int)strlen(message));
                memset(incomingBuff, 0, MAX_MESSAGE_LENGTH);
                List_prepend(incomingList, message);

                pthread_mutex_lock(&InLock);
                pthread_cond_signal(&IncomingListNotEmpty);
                pthread_mutex_unlock(&InLock);
            }
        }
    }
    pthread_exit(NULL);
}

void* consoleManager(){
    while(Running){
        pthread_mutex_lock(&InLock);
        pthread_cond_wait(&IncomingListNotEmpty, &InLock);
        pthread_mutex_unlock(&InLock);
        //printf("consoleManager:\n");
        while(List_curr(incomingList) != NULL){
            char* curr = List_trim(incomingList);
            //printf("    printing recieved: ");
            printf("%s\n", curr);
            free(curr);
        }
    }
    pthread_exit(NULL);
}

void* sendOutgoing(void* p){
    struct dataArgs* data = p;
    while(Running){
        pthread_mutex_lock(&OutLock);
        pthread_cond_wait(&OutgoingListNotEmpty, &OutLock);
        pthread_mutex_unlock(&OutLock);
        //printf("sendOutgoing:\n");
        while(List_curr(outgoingList) != NULL){
            //printf("    sending\n");
            char* curr = List_trim(outgoingList);
            size_t s = strlen(curr);
            encrypt(curr, s);
            if(sendto(data->sockfd, curr, s, MSG_CONFIRM, data->resRem->ai_addr, data->resRem->ai_addrlen)==-1){
                printf("send failed!\n");
            }
            free(curr);
        }

        pthread_mutex_lock(&ExitLock);
        pthread_cond_signal(&CanExit);
        pthread_mutex_unlock(&ExitLock);
    }
    
    pthread_exit(NULL);
}

void initializeThreads(struct dataArgs* args){
    threads[0] = pthread_self();

    if(pthread_create(&threads[1], NULL, &checkForIncoming, args)){
        printf("ERROR: creating datagram checker thread\n");
        exit(0);
    }

    if(pthread_create(&threads[2], NULL, &consoleManager, NULL)){
        printf("ERROR: creating console manager thread\n");
        exit(0);
    }

    if(pthread_create(&threads[3], NULL, &sendOutgoing, args)){
        printf("ERROR: creating outgoing message thread\n");
        exit(0);
    }

    checkIO(args);
}

char* decrypt(char* message, int maxLength){
    for(int i = 0; i < maxLength && message[i]+KEY != '\0' && message[i]+KEY != 0; i++){
        if(message[i]-KEY <= 0){
            message[i] = (message[i]-KEY)+255;
        }
        message[i] -= KEY;

        if(message[i] == '_'){
            message[i] = ' ';
        }
    }
    return message;
}

char* encrypt(char* message, int maxLength){
    for(int i = 0; i < maxLength; i++){
        if(message[i] == ' '){
            message[i] = '_';
        }
        if((message[i]+KEY > 255)){
            message[i] = (message[i]+KEY)-255;
        }
        else{
            message[i] += KEY;
        }
    }
    return message;
}

void checkStatus(struct dataArgs* data){
    if(sendto(data->sockfd, "!status\n", 9, 0, data->resRem->ai_addr, data->resRem->ai_addrlen)==-1){
        printf("failed to send status check\n");
        return;
    }

    sleep(3);
    if(checkFailed){
        printf("Offline\n");
    }
    checkFailed = 1;
}

void initiateExit(struct dataArgs* data, int callingThread){
    for(int i = 0; i < NUM_OF_THREADS; i++){
        if(i != callingThread){
            pthread_cancel(threads[i]);
        }
    }
    Running = 0;
    List_free(incomingList, (FREE_FN)&freeMessage);
    List_free(outgoingList, (FREE_FN)&freeMessage);
    free(threads);
    free(incomingBuff);
    close(data->sockfd);
    free(data);
}

void freeMessage(void* message){
    free(message);
}
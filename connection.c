#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdbool.h>

#include <stdio.h>

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* Functions that use Connection data type are the ones suposed to be used in the program. */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef and
#define and &&
#endif // and

#ifndef or
#define or ||
#endif // or

// TODO rename input thread to connection thread

// TODO modify thread to run continously and connect to any avalible devices

// TODO add errors

// TODO fix AF_LOCAL sockets if they crash (or make the program not crash)

// TODO create python suport

// TODO stop ading TODOs

typedef void* Connection;

struct ConnectionThread {
    pthread_t threadID;
    pthread_cond_t cond;
    pthread_mutex_t mutex;

    // read only variables
    bool input;
    bool server;
    bool network;
    void *socketData;
    size_t dataSize;
    
    // shered variables
    bool running;
    bool connected;
    bool newData;
    void *data;
};

uint64_t createConnectionThreadSocket(const void* socketData, bool server, bool network);

void *inputThreadMain(void *arg) {
    struct ConnectionThread *inputThread = (struct ConnectionThread*)arg;

    // allocate memory
    void *data = malloc(inputThread->dataSize);
    if (!data) {
        exit(1);
    }
    
    long long sockets = createConnectionThreadSocket(inputThread->socketData, inputThread->server, inputThread->network);
    pthread_mutex_lock(&inputThread->mutex);
    inputThread->connected = true;
    pthread_mutex_unlock(&inputThread->mutex);

    int socketfd, serverfd;
    memcpy(&socketfd, &sockets, sizeof(int));
    memcpy(&serverfd, &sockets + sizeof(int), sizeof(int));

    // TODO inprove performance of main loop or create 2 types of thread main for input and output

    // main loop
    pthread_mutex_lock(&inputThread->mutex);
    while(inputThread->running) {
        pthread_mutex_unlock(&inputThread->mutex);
        if (inputThread->input) {
            recv(socketfd, data, inputThread->dataSize, 0);
            pthread_mutex_lock(&inputThread->mutex);
            memcpy(inputThread->data, data, inputThread->dataSize);
            inputThread->newData = true;
        } else {
            pthread_mutex_lock(&inputThread->mutex);
            if (inputThread->newData) {
                memcpy(data, inputThread->data, inputThread->dataSize);
                pthread_mutex_unlock(&inputThread->mutex);
                send(socketfd, data, inputThread->dataSize, 0);
                inputThread->newData = false;
                pthread_mutex_lock(&inputThread->mutex);
            }
        }
    }
    pthread_mutex_unlock(&inputThread->mutex);

    free(data);
    close(socketfd);

    if (inputThread->server) {
        close(serverfd);
    }
    
    if (!inputThread->network) {
        unlink(inputThread->socketData);
    }
    
    pthread_mutex_lock(&inputThread->mutex);
    inputThread->connected = 0;
    pthread_mutex_unlock(&inputThread->mutex);

    return NULL;
}

// returns the file descriptor for the client
// function return a 8 byte number where the first 4 bytes is the file descriptor of the client
// the last 4 bytes of the number reprezent the server file descriptor
uint64_t createConnectionThreadSocket(const void* socketData, bool server, bool network) {
    uint64_t socketfd = socket((network ? AF_INET : AF_LOCAL), SOCK_STREAM, 0);

    // convert socketData to sockaddr
    struct sockaddr *socketaddr;
    if (network) {
        if (!(socketaddr = malloc(sizeof(struct sockaddr_in)))) {
            exit(1);
        }
        ((struct sockaddr_in*)socketaddr)->sin_family = AF_INET;
        if (!strlen(socketData + sizeof(in_port_t)) and server) {
            ((struct sockaddr_in*)socketaddr)->sin_addr.s_addr = INADDR_ANY;
        } else {
            ((struct sockaddr_in*)socketaddr)->sin_addr.s_addr = inet_addr(socketData + sizeof(in_port_t));
        }
        ((struct sockaddr_in*)socketaddr)->sin_port = htons(*((in_port_t*)socketData));
    } else {
        if (!(socketaddr = malloc(sizeof(struct sockaddr_un)))) {
            exit(1);
        }
        ((struct sockaddr_un*)socketaddr)->sun_family = AF_LOCAL;
        strcpy(((struct sockaddr_un*)socketaddr)->sun_path, socketData);
    }

    if (server) {
        if (network) {
            unlink(socketData);
        }
        bind((int)socketfd, socketaddr, sizeof(*socketaddr));
        listen((int)socketfd, 0);
        void *socketMemoryAddress = &socketfd;
        memcpy(socketMemoryAddress + sizeof(int), &socketfd, sizeof(int));
        *((int*)&socketfd) = accept((int)socketfd, NULL, NULL);
    } else {
        connect((int)socketfd, socketaddr, sizeof(*socketaddr));
    }

    free(socketaddr);

    return socketfd;
}

// creates and starts the connection thread and socket
// dataSize reprezents the maximum size of the data it can revice
// suports AF_LOCAL/AF_UNIX (is network is false) and AF_INET (if network is true)
// const void* socket points to file name or host name depending on the type of socket
// if host name os used for the soket the first 2 bytes reprezent the port (in_port_t)
// for server the host is the adress on witch the server listens for connesction
// to lisne to any connection the host should be set to a empty string
struct ConnectionThread *createConnectionThread(const void *socketData, size_t dataSize, bool input, bool network, bool server) {
    struct ConnectionThread *inputThread = (struct ConnectionThread*)malloc(sizeof(struct ConnectionThread));
    if (!inputThread) {
        exit(1);
    }
    
    inputThread->running = true;
    inputThread->connected = false;
    inputThread->dataSize = dataSize;
    inputThread->server = server;
    inputThread->network = network;
    inputThread->input = input;
    inputThread->newData = false;

    if (pthread_mutex_init(&inputThread->mutex, NULL)) {
        exit(1);
    }
    if (pthread_cond_init(&inputThread->cond, NULL)) {
        exit(1);
    }

    size_t socketSize;
    if (network) {
        socketSize = sizeof(in_port_t);
        socketSize += strlen(socketData + socketSize);
    } else {
        socketSize = strlen(socketData);
    }

    if (!(inputThread->socketData = malloc(socketSize))) {
        exit(1);
    }

    memcpy(inputThread->socketData, socketData, socketSize);

    if (!(inputThread->data = malloc(dataSize))) {
        exit(1);
    }

    if (pthread_create(&inputThread->threadID, NULL, inputThreadMain, inputThread)) {
        exit(1);
    }

    return inputThread;
}

/* User functions. */

// creates a AF_LOCAL/AF_UNIX connection where socket parameter is the path to the socket file
// size of data reprezents the size of the structure, variable or buffer send and recived
Connection createLocalConnection(const char* socket, bool input, bool server, size_t sizeOfData) {
    return createConnectionThread(socket, sizeOfData, input, false, server);
}

// creates a AF_INET connection whith a ip and a port
// if connection is server ip can be left as a empty string ("" or "\0") to listen for connections from any ip
// alternativly the ip can be set to 0.0.0.0
// size of data reprezents the size of the structure, variable or buffer send and recived
Connection createNetworkConnection(const char* ip, unsigned short port, bool input, bool server, size_t sizeOfData) {
    void* socket = malloc(strlen(ip) + sizeof(port));
    if (!socket) {
        exit(1);
    }
    memcpy(socket, &port, sizeof(port));
    strcpy(socket + sizeof(port), ip);
    return createConnectionThread(socket, sizeOfData, input, true, server);
}

// sends stop comand to thread and waits for thread to finish
// dealocates all thread allocated memory
// dealocates connection variable and set its value to NULL
void destroyConnection(Connection *connection) {
    struct ConnectionThread *connectionThread = (struct ConnectionThread*)(*connection);
    pthread_mutex_lock(&connectionThread->mutex);
    connectionThread->running = 0;
    pthread_mutex_unlock(&connectionThread->mutex);

    pthread_join(connectionThread->threadID, NULL);
    pthread_mutex_destroy(&connectionThread->mutex);
    pthread_cond_destroy(&connectionThread->cond);
    
    free(connectionThread->socketData);
    connectionThread->socketData = NULL;
    free(connectionThread->data);
    connectionThread->data = NULL;

    free(*connection);
    *connection = NULL;
}

// TODO make better system for reciving data

// if data was recived function return true else false
// when new data is recived the old one is overwriten
// copies the data from the thread memory into caller thread memory
// local copy is crated to alow data to be used while the thread receves new data
// dest should be the same type as the data set when creating the thread
bool getConnectionData(const Connection connection, void *dest) {
    struct ConnectionThread *connectionThread = (struct ConnectionThread*)connection;
    pthread_mutex_lock(&connectionThread->mutex);
    if (!connectionThread->newData) {
        pthread_mutex_unlock(&connectionThread->mutex);
        return false;
    }
    memcpy(dest, connectionThread->data, connectionThread->dataSize);
    pthread_mutex_unlock(&connectionThread->mutex);
    return true;
}

// if data was set and not send the fucntion waits for data to be send
// copies the data from the caller thread memory into thread memory
// local copy is crated to alow caller thread to continue execution while the thread sends data
// src should be the same type as the data set when creating the thread
void setConnectionData(const Connection connection, void *src) {
    struct ConnectionThread *connectionThread = (struct ConnectionThread*)connection;
    pthread_mutex_lock(&connectionThread->mutex);
    memcpy(connectionThread->data, src, connectionThread->dataSize);
    connectionThread->newData = true;
    pthread_mutex_unlock(&connectionThread->mutex);
}

// check if connection exists and if it is connected or not
// can be used to check if the thread is still waiting to connect or the other end is closed
// checking if device has connected only works for network connection
// checking if connection is closed works for any connection
bool isConnected(const Connection connection) {
    if (connection) {
        pthread_mutex_lock(&((struct ConnectionThread*)connection)->mutex);
        if (((struct ConnectionThread*)connection)->connected) {
            pthread_mutex_unlock(&((struct ConnectionThread*)connection)->mutex);
            return true;
        }
        pthread_mutex_unlock(&((struct ConnectionThread*)connection)->mutex);
    }
    return false;
}

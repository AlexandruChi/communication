#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdbool.h>

#include <stdio.h>

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Functions that use Connection data type are the ones supposed to be used in the program *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef and
#define and &&
#endif // and

#ifndef or
#define or ||
#endif // or

#define GET_BIT(X, Y) ((X) & (Y)) 

#define INPUT_BIT(X) GET_BIT((X), 0b00000001)
#define SERVER_BIT(X) GET_BIT((X), 0b00000010)

// TODO modify thread to run continuously and connect to any available devices

// TODO add error handling

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
    
    // shared variables
    bool running;
    bool connected;
    bool newData;
    void *data;
};

uint64_t createConnectionThreadSocket(const void* socketData, bool server, bool network);

void *connectionThreadMain(void *arg) {
    struct ConnectionThread *connectionThread = (struct ConnectionThread*)arg;

    // allocate memory
    void *data = malloc(connectionThread->dataSize);
    if (!data) {
        exit(1);
    }
    
    long long sockets = createConnectionThreadSocket(connectionThread->socketData, connectionThread->server, connectionThread->network);
    pthread_mutex_lock(&connectionThread->mutex);
    connectionThread->connected = true;
    pthread_mutex_unlock(&connectionThread->mutex);

    int socketfd, serverfd;
    memcpy(&socketfd, &sockets, sizeof(int));
    memcpy(&serverfd, &sockets + sizeof(int), sizeof(int));

    // TODO create 2 types of thread main for input and output

    // main loop
    pthread_mutex_lock(&connectionThread->mutex);
    while(connectionThread->running) {
        pthread_mutex_unlock(&connectionThread->mutex);
        if (connectionThread->input) {
            recv(socketfd, data, connectionThread->dataSize, 0);
            pthread_mutex_lock(&connectionThread->mutex);
            memcpy(connectionThread->data, data, connectionThread->dataSize);
            connectionThread->newData = true;
            pthread_mutex_unlock(&connectionThread->mutex);
        } else {
            pthread_mutex_lock(&connectionThread->mutex);
            if (connectionThread->newData) {
                memcpy(data, connectionThread->data, connectionThread->dataSize);
                pthread_mutex_unlock(&connectionThread->mutex);
                send(socketfd, data, connectionThread->dataSize, 0);
                pthread_mutex_lock(&connectionThread->mutex);
                connectionThread->newData = false;
                pthread_mutex_unlock(&connectionThread->mutex);
            }
        }
        pthread_mutex_lock(&connectionThread->mutex);
    }
    pthread_mutex_unlock(&connectionThread->mutex);

    free(data);
    close(socketfd);

    if (connectionThread->server) {
        close(serverfd);
    }
    
    if (!connectionThread->network) {
        unlink(connectionThread->socketData);
    }
    
    pthread_mutex_lock(&connectionThread->mutex);
    connectionThread->connected = 0;
    pthread_mutex_unlock(&connectionThread->mutex);

    return NULL;
}

/* returns the file descriptor for the client
 * function return a 8 byte number where the first 4 bytes is the file descriptor of the client
 * the last 4 bytes of the number represent the server file descriptor */
uint64_t createConnectionThreadSocket(const void* socketData, bool server, bool network) {
    uint64_t socketfd = socket((network ? AF_INET : AF_LOCAL), SOCK_STREAM, 0);
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
        if (!network) {
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

/* creates and starts the connection thread and socket
 * dataSize represents the maximum size of the data it can receive
 * supports AF_LOCAL/AF_UNIX (is network is false) and AF_INET (if network is true)
 * const void* socket points to file name or host name depending on the type of socket
 * if host name os used for the socket the first 2 bytes represent the port (in_port_t)
 * for server the host is the address on witch the server listens for connection
 * to listen to any connection the host should be set to a empty string */
struct ConnectionThread *createConnectionThread(const void *socketData, size_t dataSize, bool input, bool network, bool server) {
    struct ConnectionThread *connectionThread = (struct ConnectionThread*)malloc(sizeof(struct ConnectionThread));
    if (!connectionThread) {
        exit(1);
    }
    
    connectionThread->running = true;
    connectionThread->connected = false;
    connectionThread->dataSize = dataSize;
    connectionThread->server = server;
    connectionThread->network = network;
    connectionThread->input = input;
    connectionThread->newData = false;

    if (pthread_mutex_init(&connectionThread->mutex, NULL)) {
        exit(1);
    }
    if (pthread_cond_init(&connectionThread->cond, NULL)) {
        exit(1);
    }

    size_t socketSize;
    if (network) {
        socketSize = sizeof(in_port_t);
        socketSize += strlen(socketData + socketSize);
    } else {
        socketSize = strlen(socketData);
    }

    if (!(connectionThread->socketData = malloc(socketSize))) {
        exit(1);
    }

    memcpy(connectionThread->socketData, socketData, socketSize);

    if (!(connectionThread->data = malloc(dataSize))) {
        exit(1);
    }

    if (pthread_create(&connectionThread->threadID, NULL, connectionThreadMain, connectionThread)) {
        exit(1);
    }

    return connectionThread;
}

/* User functions */

/* creates a AF_LOCAL/AF_UNIX connection where socket parameter is the path to the socket file
 * size of data represents the size of the structure, variable or buffer send and received */
Connection createLocalConnection(const char* socket, size_t sizeOfData, uint8_t flags) {
    return createConnectionThread(socket, sizeOfData, INPUT_BIT(flags), false, SERVER_BIT(flags));
}

/* creates a AF_INET connection whit a ip and a port
 * if connection is server ip can be left as a empty string ("" or "\0") to listen for connections from any ip
 * alternatively the ip can be set to 0.0.0.0
 * size of data represents the size of the structure, variable or buffer send and received */
Connection createNetworkConnection(const char* ip, unsigned short port, size_t sizeOfData, uint8_t flags) {
    void* socket = malloc(strlen(ip) + sizeof(port));
    if (!socket) {
        exit(1);
    }
    memcpy(socket, &port, sizeof(port));
    strcpy(socket + sizeof(port), ip);
    return createConnectionThread(socket, sizeOfData, INPUT_BIT(flags), true, SERVER_BIT(flags));
}

/* sends stop comand to thread and waits for thread to finish
 * deallocates all thread allocated memory
 * deallocates connection variable and set its value to NULL */
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

/* if data was received function return true else false
 * when new data is received the old one is overwritten
 * copies the data from the thread memory into caller thread memory
 * local copy is crated to allow data to be used while the thread receives new data
 * dest should be the same type as the data set when creating the thread */
bool getConnectionData(const Connection connection, void *dest) {
    struct ConnectionThread *connectionThread = (struct ConnectionThread*)connection;
    pthread_mutex_lock(&connectionThread->mutex);
    if (!connectionThread->newData) {
        pthread_mutex_unlock(&connectionThread->mutex);
        return false;
    }
    memcpy(dest, connectionThread->data, connectionThread->dataSize);
    connectionThread->newData = false;
    pthread_mutex_unlock(&connectionThread->mutex);
    return true;
}

/* if data was set and not send the function waits for data to be send
 * copies the data from the caller thread memory into thread memory
 * local copy is crated to allow caller thread to continue execution while the thread sends data
 * src should be the same type as the data set when creating the thread */
void setConnectionData(const Connection connection, void *src) {
    struct ConnectionThread *connectionThread = (struct ConnectionThread*)connection;
    pthread_mutex_lock(&connectionThread->mutex);
    memcpy(connectionThread->data, src, connectionThread->dataSize);
    connectionThread->newData = true;
    pthread_mutex_unlock(&connectionThread->mutex);
}

/* check if connection exists and if it is connected or not
 * can be used to check if the thread is still waiting to connect or the other end is closed
 * checking if device has connected only works for network connection
 * checking if connection is closed works for any connection */
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

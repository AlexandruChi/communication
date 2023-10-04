/* C/C++ */

/* all libraries inlcuded in this header
 * are used only for bool and size_t
 * 
 * check source code for documentation and 
 * implementation for the threads and sockets
 * 
 * library does not implement a queue system
 * for messeges so only the most recent data is 
 * continuously recived or send when is avalible 
 * in the threads memory
 */

#ifndef __connection_h
#define __connection_h

#ifdef __cplusplus
#include <cstddef>
#else // __cplusplus
#include <stddef.h>
#include <stdbool.h>
#endif // __cplusplus

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

typedef void* Connection;

// creates a AF_LOCAL/AF_UNIX connection where socket parameter is the path to the socket file
// size of data reprezents the size of the structure, variable or buffer send and recived
Connection createLocalConnection(const char* socket, bool input, bool server, size_t sizeOfData);

// creates a AF_INETconnection whith a ip and a port
// if connection is server ip can be left as a empty string ("" or "\0") to listen for connections from any ip
// alternativly the ip can be set to 0.0.0.0
// size of data reprezents the size of the structure, variable or buffer send and recived
Connection createNetworkConnection(const char* ip, unsigned short port, bool input, bool server, size_t sizeOfData);

// sends stop comand to thread and waits for thread to finish
// dealocates all thread allocated memory
// dealocates connection variable and set its value to NULL
void destroyConnection(Connection *connection);

// if data was recived function return true else false
// when new data is recived the old one is overwriten
// copies the data from the thread memory into caller thread memory
// local copy is crated to alow data to be used while the thread receves new data
// dest should be the same type as the data set when creating the thread
bool getConnectionData(const Connection connection, void *dest);

// if data was set and not send the fucntion waits for data to be send
// copies the data from the caller thread memory into thread memory
// local copy is crated to alow caller thread to continue execution while the thread sends data
// src should be the same type as the data set when creating the thread
void setConnectionData(const Connection connection, void *src);

// check if connection exists and if it is connected or not
// can be used to check if the thread is still waiting to connect or the other end is closed
bool isConnected(const Connection connection);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __connection_h
/* C/C++ */

/* all libraries included in this header
 * are used only for bool and size_t
 * 
 * check source code for documentation and 
 * implementation for the threads and sockets
 * 
 * library does not implement a queue system
 * for messages so only the most recent data is 
 * continuously received or send when is available 
 * in the threads memory
 */

#ifndef __connection_h
#define __connection_h

#ifdef __cplusplus
#include <cstddef>
#include <cstdint>
#else // __cplusplus
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#endif // __cplusplus

#define CONNECTION_OUTPUT 0b00000000
#define CONNECTION_INPUT 0b00000001
#define CONNECTION_CLIENT 0b00000000
#define CONNECTION_SERVER 0b00000010

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

typedef void* Connection;

/* creates a AF_LOCAL/AF_UNIX connection where socket parameter is the path to the socket file
 * size of data represents the size of the structure, variable or buffer send and received */
Connection createLocalConnection(const char* socket, size_t sizeOfData, uint8_t flags);

/* creates a AF_INET connection whit a ip and a port
 * if connection is server ip can be left as a empty string ("" or "\0") to listen for connections from any ip
 * alternatively the ip can be set to 0.0.0.0
 * size of data represents the size of the structure, variable or buffer send and received */
Connection createNetworkConnection(const char* ip, unsigned short port, size_t sizeOfData, uint8_t flags);

/* sends stop comand to thread and waits for thread to finish
 * deallocates all thread allocated memory
 * deallocates connection variable and set its value to NULL */
void destroyConnection(Connection *connection);

/* if data was received function return true else false
 * when new data is received the old one is overwritten
 * copies the data from the thread memory into caller thread memory
 * local copy is crated to allow data to be used while the thread receives new data
 * dest should be the same type as the data set when creating the thread */
bool getConnectionData(const Connection connection, void *dest);

/* if data was set and not send the function waits for data to be send
 * copies the data from the caller thread memory into thread memory
 * local copy is crated to allow caller thread to continue execution while the thread sends data
 * src should be the same type as the data set when creating the thread */
void setConnectionData(const Connection connection, void *src);

/* check if connection exists and if it is connected or not
 * can be used to check if the thread is still waiting to connect or the other end is closed
 * checking if device has connected only works for network connection
 * checking if connection is closed works for any connection */
bool isConnected(const Connection connection);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __connection_h
#ifndef NETWORKING
#define NETWORKING

#include <stdbool.h>
/*
	server_create_socket

	creates server.

	parameters: port number

	returns: socket file descriptor
*/
int server_create_socket(int port_number);

/*
	sendall

	sends buffer across the network.

	parameters: the socket, buffer to be sent, and length of the message

	returns: the socket file descriptor
*/
int sendall(int socket, char *buf, int length);

/*
	recvall



	parameters: the buffer being written to, the socket, length of how much it
	should be receive.

	returns: numbers of characters received
*/
int recvall(char *buffer, int socket, int length);



/*
	server_accept_connection

	parameters: the socket file descriptor for listening for new players

	returns: the socket file descriptor for the new player

*/
int server_accept_connection(int fd);

bool check_beginning_chars(char *buffer);

#endif

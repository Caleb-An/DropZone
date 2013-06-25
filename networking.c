#include <gtk/gtk.h>
#include <chipmunk/chipmunk.h>
#include "specs/networking.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "specs/protocols.h"

#define LISTENQ 8

/*
	server_create_socket

	creates server

	parameters: port number

	returns: socket file descriptor
*/
int server_create_socket(int port_number) {
    // Step 1:  Create a socket with socket()
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (socket_fd < 0) {
	perror("Problem in creating the socket");
	exit(2);
    }

    // Step 2: bind the socket to an address
    struct sockaddr_in serveraddr;
    serveraddr.sin_family = AF_INET;  // INET family of protocols

    // Use default address of this machine.
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons(port_number);

    bind(socket_fd, (struct sockaddr*) &serveraddr, sizeof(serveraddr));

    return socket_fd;
}

/*
  	Source: Beej's Guide to Network Programming

	sendall

	sends buffer across the network

	parameters: the socket, buffer to be sent, and length of the message

	returns: the socket file descriptor
*/

int sendall(int socket, char *buf, int length) {
    int total = 0;        // how many bytes we've sent
    int bytesleft = MAXLINE; // how many we have left to send
    int n;

    while(total < MAXLINE) {
        n = send(socket, buf+total, bytesleft, 0);
        if (n == -1) { break; }
        total += n;
        bytesleft -= n;
	//printf("n: %d\n", n);
    }
    //printf("SEND: %s, length: %d\n", buf, total);
    //length = total; // return number actually sent here

    return n==-1?-1:0; // return -1 on failure, 0 on success
}

/*




*/
int recvall(char *buffer, int socket, int length) {
    int done_receiving = NOT_DONE;
    int index = 0;
    int n = 0;
    while(done_receiving == NOT_DONE && index < length) {
	n = recv(socket, buffer + index, length, 0);
		//printf("Receive: %s\n", buffer);

	if(n < 0) {
	    //perror("recv");
	    return -1;
	}

	if(!check_beginning_chars(buffer)) {
	    //printf("first chars not met\n");
		//printf("BOGUS: %s\n", buffer);
	    buffer = NULL;
	    return MAXLINE + 2;
	}

	done_receiving = protocol_recv_full_message(buffer);
	index += n;
    }

    if (done_receiving == BOGUS)
        return MAXLINE + 2;
    else {
		//printf("Recvall buffer: %s\n", buffer);
		if (buffer[4] != '2') {
			//printf("Recvall buffer: %s\n", buffer);
		}
	return index;
    }
}


bool check_beginning_chars(char *buffer) {
    for(int i = 0; i < ID_INDEX-1; i++)
	if(buffer[i] != BEGIN_CHARACTER[i])
	    return false;
    return true;
}


int server_accept_connection(int fd) {
    // step 3:  convert listenfd to a passive "listening" socket
    listen(fd, LISTENQ);

    // structure to store client address
    struct sockaddr_in cliaddr;
    socklen_t clilen = sizeof(cliaddr);

    printf("Waiting to accept a socket connection request.\n");
    int connfd = accept (fd, (struct sockaddr *) &cliaddr, &clilen);

    printf("Received socket connection request.\n");
    return connfd;
}

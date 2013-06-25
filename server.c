#include <gtk/gtk.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <chipmunk/chipmunk.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include "specs/common.h"
#include "specs/physics.h"
#include "specs/networking.h"
#include "specs/protocols.h"

#define NEW_PLAYER 30001 // Port used to listen for new players
#define TIMESTEP 1.0/60.0
#define EXTRA_TIME 10000

/*
	server.c

	the central file for the server. Has the main function. Contains a world_struct 
    and a chipmunk space; it will accept new body messages and level change messages
	from a client, update accordingly its world_struct and chipmunk space accordingly,
	all while updating all clients of updates to body positions. 
 */



/*
	broadcast_info struct
		
	contains all the necessary info of a current game status
 */
typedef struct {
    bool skip_sender_fd;
    int sender_fd;
    char *message;
    fd_set *master_readfds;
    int nbytes;
    int	new_player_listener;
    int fdmax;
    int game_started;
    int num_players;
    int try_number;
    int level;
    world_status *world;

} broadcast_info;

//function prototypes 
static broadcast_info *new_broadcast_info(fd_set *master_readfds, int new_player_listener, int fdmax);
static void server_select(struct timeval *tv, broadcast_info *info);
static void server_broadcast_message(broadcast_info *info);
static void server_world_step(broadcast_info *info);
static void server_add_body(broadcast_info *info);
static void server_send_initial_bodies(cpBody *body, broadcast_info *info);
static void free_message(broadcast_info *info);
static void server_send_world_info(broadcast_info *info);
static void server_switch_level(broadcast_info *info, int level, bool win);

/*
	initializes a new broadcast_info 

	parameters: file descriptor, player listener, max file descriptor

	returns: pointer to the broadcast_info struct
 */
static broadcast_info *
new_broadcast_info(fd_set *master_readfds, int new_player_listener, int fdmax) {

    broadcast_info *info = (broadcast_info *) malloc(sizeof(broadcast_info));
    assert(info);

    info -> skip_sender_fd = false;
    info -> message = NULL;
    info -> master_readfds = master_readfds;
    info -> nbytes = 0;
    info -> game_started = false;
    info -> new_player_listener = new_player_listener;
    info -> fdmax = fdmax;
    info -> num_players = 0;
    info -> try_number = 0;
    info -> sender_fd = 0;
    info -> level = 1;
    info -> world = NULL;

    return info;
}

/*
	checks and processes messages from clients

	parameters: timeval struct, broadcast info structs

	returns: nothing
 */
static void
server_select(struct timeval *tv, broadcast_info *info) {

    fd_set readfds;
    FD_ZERO(&readfds);
    readfds = *(info -> master_readfds);
    char buf[MAXLINE] = {0};

    // Look for sockets ready to be read
	if (select(info -> fdmax + 1, &readfds, NULL, NULL, tv) == -1) {
		
		perror("select");
		exit(4);
    }

    // Check through the file descriptors for data
	for (int i = 0; i <= info -> fdmax; i++) {

        if (FD_ISSET(i, &readfds)) {

            // Check if a new player is going to be added.
	    	if (i == info -> new_player_listener) {

				// If so, accept the player's connection
				int new_player_fd = server_accept_connection(info -> new_player_listener);
				info -> game_started = true;

				if(new_player_fd != -1) {
		    		// Update fdmax if necessary
		    		if(new_player_fd > info -> fdmax)
					info -> fdmax = new_player_fd;

		    		// Add fd to the set of fds to check from next loop on
		    		FD_SET(new_player_fd, info -> master_readfds);
		    		info -> num_players++;
		    		server_send_world_info(info);
				}
		
				// Problem in accepting player's connection
				else
		    		perror("accept");
			}

            // There is either a chat or a shape to process.
	    	else {
				
				info -> nbytes = recvall(buf, i, MAXLINE);

				if(info -> nbytes != MAXLINE + 2) {

		    		// Message from client to add new body
		    		if(buf[ID_INDEX] == NEW_BODY_MESSAGE + '0') {
						
						free_message(info);
						info -> message = buf;
						info -> skip_sender_fd = false;
						server_add_body(info);
						info -> try_number++;
		    		}

		    		// Message from client that is a chat to be sent out to other players
		    		else if(buf[ID_INDEX] == CHAT_MESSAGE + '0') {
			
						free_message(info);
						info -> skip_sender_fd = true;
						info -> sender_fd = i;
						info -> message = buf;
						server_broadcast_message(info);
						usleep(EXTRA_TIME);
		    		}

		    		// Message from client to change the level
		    		else if(buf[ID_INDEX] == LEVEL_MESSAGE + '0') {
						free_message(info);
						info -> message = buf;
						info -> skip_sender_fd = false;
						level_info *level_switch_info = protocol_decode_level(buf);
						server_switch_level(info, level_switch_info -> level, false);
		    		}
				}
		
				// Errors or client dropped
				else {
		    		
					// Connection closed
		    		if (info -> nbytes == 0) {
						printf("selectserver: socket %d hung up\n", i);
		    		}
		    		else
						perror("recv");

		    		info -> num_players--;
		    		FD_CLR(i, info -> master_readfds); // remove from master set
		    		close(i); // bye!
				}
	    	}
		}
    }
}

/*
	broadcasts the broadcast info
	
	parameters: broadcast info

	returns: nothing
 */
static void 
server_broadcast_message(broadcast_info *info) {

    for(int j = 0; j <= info -> fdmax + 1; j++) {
	
		// Loop through file descriptors to send info to each one
		if (FD_ISSET(j, info -> master_readfds)) {
	    	
			// Except the new player listener and the sender if a chat
	    	if (j != info -> new_player_listener && !(info -> skip_sender_fd && info -> sender_fd == j)) {
				
				if (sendall(j, info -> message, MAXLINE) == -1)
		    		perror("send");

	    	}
		}
    }
}

/*
	calls physics functions to timestep the world

	parameters: broadcast_info struct pointer

	returns: nothing
 */
static void
server_world_step(broadcast_info *info) {

    world_update(info -> world);
    info -> message = protocol_send_coords(info -> world -> space);
    info -> skip_sender_fd = false;
    server_broadcast_message(info);

	//if a new level is desired, switches level
    if (info -> world->status == 1) {
	
		usleep(EXTRA_TIME*2);
		server_switch_level(info, info->level, true);
		usleep(EXTRA_TIME*2);
    }
}

/*
	adds the body received from client 

	parameters: broadcast_info struct pointer

	returns: nothing
 */
static void 
server_add_body(broadcast_info *info) {

	//if the player is out of tries, restart the level
    if(info -> try_number >= 3) {

		server_switch_level(info, info -> level, false);
		info -> try_number = -1;
		return;
    }

    polygon_struct *body_info = protocol_extract_body(info -> message);

    body_info -> body_id = create_user_object((cpVect *)body_info -> vectors ->
		data, body_info -> vector_count, conv_color(body_info -> color),
		info -> world, PLAYER_BOX_COLLISION_NUMBER, 1, body_info->mass);

    cpVect average = cpv(0,0);
    cpVect *vect = (cpVect *) (body_info->vectors->data);

	// sum up the x and y values
    for (int i = 0; i < body_info->vector_count; i++) {
		average.x += vect[i].x;
		average.y += vect[i].y;
    }
	

	// divide by the vector count to get the average
    average.x /= body_info->vector_count;
    average.y /= body_info->vector_count;

    cpVect vertices[body_info->vector_count];
    
	for (int i = 0; i < body_info->vector_count; i++) {
		vertices[i] = cpvsub (vect[i], average);
    }

    info -> message = protocol_new_body(body_info -> color, vertices, body_info -> vector_count,
					body_info -> body_id, 1);

    server_broadcast_message(info);
    usleep(EXTRA_TIME * 5);
}

/*
	send the body of the level to the clients

	parameters: body, broadcast_info pointer

	returns: nothing
*/
static void 
server_send_initial_bodies(cpBody *body, broadcast_info *info) {

    polygon_struct *body_info = polygon_from_body(body);

    info -> message = protocol_new_body(body_info -> color, (cpVect *)body_info -> vectors-> data,
		body_info -> vector_count, body_info -> body_id, 1);

    server_broadcast_message(info);

	free_message(info);
	usleep(EXTRA_TIME * 5);
}

/*
	frees the message in broadcast info

	parameters: broadcast_info pointer

	returns: nothing
*/
static void 
free_message(broadcast_info *info) {

    if(info -> message) {
		free(info -> message);
		info -> message = NULL;
    }
}

/*
	sends the initial world info, using server_send_initial_bodies
	on each body within the initial level cpSpace

	parameters: broadcast_info pointer

	returns: nothing
*/
static void 
server_send_world_info(broadcast_info *info) {
    
	server_send_initial_bodies(world_get_ground(info -> world -> space), info);
    cpSpaceEachBody(info -> world -> space, (cpSpaceBodyIteratorFunc)
		    server_send_initial_bodies, info);

    if (info -> world->drawing_box) {
		
		free_message(info);
		info->message = protocol_send_zone(info -> world->drawing_box_x1,
			info -> world->drawing_box_y1, info -> world->drawing_box_x2,
			info -> world->drawing_box_y2);

	server_broadcast_message(info);
    }
}

/*
	switches the level depending on if won or not

	parameters: broadcast_info pointer, current level, win boolean

	returns: nothing
*/
static void 
server_switch_level(broadcast_info *info, int level, bool win) {

    if(info->world) {
		world_free(info -> world);
		info->world = NULL;
	}

    // Check which level should be loaded
    if(win)
		info->level = info->level % 10 + 1;
    
	else if (level >= 1) {
		info -> level = level;
    }

	info->try_number = 0;

    // Create new world
    info -> world = world_new(info -> level, TIMESTEP);

    // Send instruction to kill current world and send info to populate new world
    info -> message = protocol_send_level(info -> level, info -> try_number);
    server_broadcast_message(info);
    usleep(EXTRA_TIME * 2);
    server_send_world_info(info);
}

/*
	main function, starts up the server and sends
	updates to any connected clients. 
 */
int 
main(int argc, char **argv) {
    
	fd_set master_readfds;
    FD_ZERO(&master_readfds);
    int fdmax = 0; // Keeps track of highest file descriptor

    // Socket to figure out if people are joining the game.
    int new_player_listener_fd = server_create_socket(NEW_PLAYER);
    FD_SET(new_player_listener_fd, &master_readfds);
    fdmax = new_player_listener_fd + 1;

    // Start game: create world_new and send level information to all clients
    broadcast_info *info = new_broadcast_info(&master_readfds,
					      new_player_listener_fd, fdmax);

    server_switch_level(info, 1, false);
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 500000;

    while(!info -> game_started || info -> num_players > 0) {

		server_world_step(info);
		server_select(&tv, info);
		usleep(5000);
    }

    // Close connections
    for(int i = 0; i < fdmax + 1; i++) {
	
		if(FD_ISSET(i, &master_readfds))
	    	close(i);
    }

    // Freeing the world
    world_free(info -> world);
    free(info);

    return EXIT_SUCCESS;
}

#include <gtk/gtk.h>
#include <chipmunk/chipmunk.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "specs/common.h"
#include "specs/graphics.h"
#include <unistd.h>
#include "specs/protocols.h"
#include "specs/networking.h"
#include <math.h>
#include "specs/physics.h"

#define LINE_RADIUS 0.5
#define DESIRED_NUMBER_VERTICES 10
#define SERV_PORT 30001 /*port*/
#define PLAYER_BOX_COLLISION_NUMBER 2
#define TEXT_BOX_BUFFER_LIMIT 141
#define MINIMUM_NUMBER_POINTS 40

/*
	gui_world struct

	contains all data needed for one client

	notable fields: 
		mutex locks space_lock, etc.
		cpSpace pointer space
		graphics world struct pointer graphics
 */
typedef struct {
    graphics_world *graphics;
    cpSpace *space;
    int press_x;
    int press_y;
    int release_x;
    int release_y;
    int try_number;
    float mass;
    int level;
    int socket;
    float *angles;
    cpVect *positions;
    int num_bodies;
    bool terminate_thread;
    pthread_mutex_t *space_lock;
    pthread_mutex_t *socket_lock;
    pthread_mutex_t *initial_lock;
    pthread_mutex_t *text_lock;
    polygon_struct *polygon;
    bool draw_success;
    GtkTextBuffer *textbox_buffer;
    GtkWidget *label;
} gui_world;

/*
	string_info struct

	contains a call from recv and the size
	of the message
 */
typedef struct {
    char buffer[MAXLINE];
    ssize_t n; // Number of bytes read (last index in buffer that is filled)
} string_info;

//function prototypes
static string_info *client_select(int socket);
static void initialize_array (gui_world *world);

/*
	starts a new game

	parameters: gui world pointer

	returns: false
 */
static gboolean
new_game (gui_world *world) {

    pthread_mutex_lock(world -> space_lock);

	//null checking
    if (world->graphics->message != NULL) {
		
		free (world->graphics->message);
		world->graphics->message = NULL;
    }

    if(world -> try_number >= 3) {

		world->graphics->message = (char *)malloc(20 * sizeof(char));
		
		if (world->graphics->message == NULL) {

	    	printf("Memory allocation error: in function new_game\n");
	    	exit(-1);
		}
		strcpy(world->graphics->message, "You Lose!");
    }

    world->try_number = 0;
	world->num_bodies = 0;

    g_array_free (world->graphics->user_points, TRUE);
    initialize_array(world);

	world_free_space (world->space);
    world->space = cpSpaceNew();
    world->graphics->space = world->space;

    pthread_mutex_unlock(world -> space_lock);
    return false;
}

/*
	quits the gtk window

	parameters: gtkwindow pointer, gui world struct

	returns: nothing
 */
static void 
pre_quit (GtkWindow *window, gpointer data) {

	gui_world *world = (gui_world *)data;
	world->terminate_thread = true;
	gtk_main_quit();
}

/*
	adds a cpVect to gui world

	parameters: floats x and y, gui world pointer

	returns: nothing
 */
static void 
add_user_point (float x, float y, gui_world *world) {

    cpVect point = cpv(x,y);

    if ((world->graphics->display) && (x < world->graphics->x1 || y <
		world->graphics->y1 || x > world->graphics->x2 
		|| y > world->graphics->y2)) {
		
		world->draw_success = false;
    }

    world->graphics->user_points = g_array_append_vals(world->graphics->user_points, &point, 1);
}

/*
	updates the body bosition

	parameters: cpSpace , cpBody, gui world

	returns: nothing
 */
static void 
update_body_post (cpSpace *space, cpBody *body, gui_world *world) {

    body_information *info = cpBodyGetUserData (body);
    int index = info->body_id;

    if ((index < world->num_bodies) && (fabs(world->angles[index]) < 1000)) {
		
		cpBodySetPos (body, world->positions[index]);
		cpBodySetAngle (body, world->angles[index]);
    }
}

/*
	updates the bodies in the world

	parameters: cpBody and gui_world

	returns: nothing
 */
static void 
update_body (cpBody *body, gui_world *world) {

    cpSpaceAddPostStepCallback(world->space, (cpPostStepFunc) update_body_post, body, world);
    cpSpaceStep(world -> space, 0);
}

/*
	updates the space

	parameters: gui world, array of float angles, array of cpVects

	returns: nothing
 */
static void 
update_space (gui_world *world, float *angles, cpVect *positions) {

    world->angles = angles;
    world->positions = positions;
    cpSpaceEachBody(world->space, (cpSpaceBodyIteratorFunc) update_body, world);
}

/*
	adds body to the gui world

	parameters: gui world, string message with body info
	
	returns: nothing
 */
static void 
add_body (gui_world *world, char *message) {

    polygon_struct *polygon = protocol_extract_body (message);
    cpVect *vectors = (cpVect *)polygon->vectors->data;

    cpBody *body = cpSpaceAddBody (world->space, cpBodyNew(1, 1));
    body_information *info = (body_information *)malloc(sizeof(body_information));
    
	if (info == NULL) {

		printf("Memory allocation error: in function add_body\n");
		exit(-1);
    }

    info->color = conv_color (polygon->color);
    info->body_id = polygon->body_id;
	(world->num_bodies)++;
    cpBodySetUserData (body, info);

    cpSpaceAddShape (world->space, cpSegmentShapeNew
		     (body, vectors[polygon->vector_count-1],
		      vectors[0], LINE_RADIUS));

    for (int i = polygon->vector_count - 2; i >= 0; i--) {
		cpSpaceAddShape (world->space, cpSegmentShapeNew
			(body, vectors[i], vectors[i+1], LINE_RADIUS));
    }

}

/*
	listening thread that checks for new data from server

	parameter: void pointer data (irrelevant)

	returns: nothing 
 */
static void *
listener_thread(void *data) {
    gui_world *world = (gui_world *) data;
    unsigned int m_secs = 1;

    while(!world -> terminate_thread) {
		
		string_info *info = client_select (world->socket);
		char string[MAXLINE];

		if (info != NULL && info->buffer != NULL /*&& info -> n == 0*/) {
			
			if(info->buffer)
				memcpy(string, info->buffer, MAXLINE);

			if (string[ID_INDEX] == NEW_BODY_MESSAGE + '0') {
			
				pthread_mutex_lock(world -> space_lock);
				add_body (world, string);
				pthread_mutex_unlock(world -> space_lock);
			}

			else if (string[ID_INDEX] == UPDATE_POSITIONS_MESSAGE + '0') {
				
				coord_update *coords = protocol_extract_coords (string);
				pthread_mutex_lock(world -> space_lock);
				update_space(world, coords->angle_array, coords->vector_array);
				pthread_mutex_unlock(world -> space_lock);
				
				free(coords->angle_array);
				free(coords->vector_array);
				free(coords);
			}

			else if (string[ID_INDEX] == CHAT_MESSAGE + '0') {
			
				char *label_string = protocol_decode_chat(string);
				gtk_label_set_text ((GtkLabel *)world->label, label_string);
			}

			else if (string[ID_INDEX] == ZONE_MESSAGE + '0') {
			
				drawing_zone *zone = protocol_decode_zone(string);
				world->graphics->x1 = zone->x1;
				world->graphics->x2 = zone->x2;
				world->graphics->y1 = zone->y1;
				world->graphics->y2 = zone->y2;
				world->graphics->display = true;
			}

			else if (string[ID_INDEX] == LEVEL_MESSAGE + '0') {
			
				level_info *level_switch_info = protocol_decode_level(string);
				world -> level = level_switch_info -> level;
				world -> try_number = level_switch_info -> try_number;
				new_game(world);
			}
		}

		gtk_widget_queue_draw(world -> graphics -> window); 
		usleep(m_secs);
		free(info);
    }

    return NULL;
}

/*
	gets a message from the socket

	parameters: integer socket number

	returns: string info struct
 */
static string_info *
client_select(int socket) {
    
	fd_set fds;
    struct timeval timeout;
    int sel;
    string_info *result = (string_info *) malloc(sizeof(string_info));
	
	if (result == NULL) {
	    
		printf("Memory allocation error: in function client_select\n");
	    exit(-1);
	}

    // Set time limit.
    timeout.tv_sec = 0;
    timeout.tv_usec = 1;

    // Create a descriptor set containing two sockets.
    FD_ZERO(&fds);
    FD_SET(socket, &fds);
    sel = select(socket + 1, &fds, NULL, NULL, &timeout);

    if (sel == -1) {
		perror("select failed");
    }

    if (sel >= 0) {
		if (FD_ISSET(socket, &fds)) {

			result -> n = recvall(result -> buffer, socket, MAXLINE);

			// Got error or connection closed by client
			if (result -> n == 0) {
			
			// connection closed
				printf("selectserver: socket %d hung up\n", socket);
				return NULL;
			}
			else if (result -> n < 0 || result -> n > MAXLINE) {
				
				return NULL;
			}
		}
    }

    return result; // number of socket descriptors ready to be read
}

/*
	callback function for color changing

	parameters: gtk widget and gpointer data, as gtk desires

	returns: gboolean
 */
static gboolean 
cb_color_change (GtkWidget *widget, gpointer data) {

    gui_world *world = (gui_world *)data;

    COLOR new_color = conv_color(gtk_widget_get_name(widget));
    world->graphics->color = new_color;

    return FALSE;
}

/*
	callback function for mass changing

	parameters: gtk widget and gpointer data, as gtk desires

	returns: gboolean
 */
static gboolean 
cb_mass_change (GtkWidget *widget, gpointer data) {
	gui_world *world = (gui_world *)data;
	world->mass = atof(gtk_widget_get_name(widget));
	return FALSE;
}

/*
	callback function for level changing

	parameters: gtk widget and gpointer data, as gtk desires

	returns: gboolean
 */
static gboolean cb_change_level (GtkWidget *widget, gpointer data) {
    gui_world *world = (gui_world *)data;
    char *send_message = protocol_send_level(atoi(gtk_widget_get_name(widget)), 1);
    int length = strlen (send_message);

    sendall(world->socket, send_message, length);

    return FALSE;
}

/*
	callback function for chat sending

	parameters: gtk widget and gpointer data, as gtk desires

	returns: gboolean
 */
static gboolean 
chatbox_button_pressed (GtkWidget *widget, gpointer data) {

	gui_world *world = (gui_world *)data;

	GtkTextIter start, end;

	gtk_text_buffer_get_bounds(world->textbox_buffer, &start, &end);

	char *buffer_contents = gtk_text_buffer_get_text((GtkTextBuffer *)world->textbox_buffer, &start, &end, false);

	char truncated_string[TEXT_BOX_BUFFER_LIMIT + 1];

	int length = strlen(buffer_contents);

	if (length < TEXT_BOX_BUFFER_LIMIT) {
		strcpy(truncated_string, buffer_contents);
	}
	else {
		length = TEXT_BOX_BUFFER_LIMIT;
		strncpy(truncated_string, buffer_contents, length);
	}

	gtk_text_buffer_delete (world->textbox_buffer, &start, &end);

	char *send_string = protocol_send_chat (truncated_string);

	length = strlen(send_string);

    sendall(world->socket, send_string, length);

	free (send_string);
	send_string = NULL;
	return FALSE;
}

/*
	callback function for getting what the user is drawing

	parameters: gtk widget and gpointer data, as gtk desires

	returns: gboolean
 */
static gboolean 
cb_button_press (GtkWidget *widget, GdkEventButton *event, gpointer data) {

    gui_world *world = (gui_world *)data;
    int space_height = 50, space_width = 50;

    /* convert into cp values */
    int window_height = gtk_widget_get_allocated_height(world -> graphics -> drawing_screen);
    int window_width = gtk_widget_get_allocated_width(world -> graphics -> drawing_screen);

    float screen_height_ratio = (float) window_height / space_height;
    float screen_width_ratio = (float) window_width / space_width;

	world->draw_success = true;

    cpFloat x = (event->x - window_width / 2) / screen_width_ratio;
    cpFloat y = (-event->y + window_height / 2) / screen_height_ratio;


	if (isnan(x) || isnan(y)) {
		printf("NAN added\n");
		return TRUE;
	}


    if (event->button == 1) {
		add_user_point (x, y, world);
    }

    return TRUE;
}

/* 
	initializes the g_array 

	parameters: gui world

	returns: nothing
 */
static void 
initialize_array (gui_world *world) {

    world->graphics->user_points = g_array_new (FALSE, FALSE, sizeof(cpVect));
}

/*
	callback function for drawing

	parameters: gtk widget and gpointer data, as gtk desires

	returns: gboolean
 */
static gboolean 
draw_cb (GtkWidget *widget, cairo_t *cr, gpointer data) {

    gui_world *world = (gui_world *) data;
    world->graphics->drawing_screen = widget;
    world -> graphics -> cr = cr;
    pthread_mutex_lock(world -> space_lock);
    graphics_space_iterate (world -> graphics);
    pthread_mutex_unlock(world -> space_lock);

    return FALSE;
}

/*
	callback function for releasing the mouse click

	parameters: gtk widget and gpointer data, as gtk desires

	returns: gboolean
 */
static gboolean 
cb_button_release (GtkWidget *widget, GdkEventMotion *event, gpointer data) {

    gui_world *world = (gui_world *)data;

    if (world->graphics->user_points->len < MINIMUM_NUMBER_POINTS) {

		world->graphics->message = (char *)malloc(20 * sizeof(char));
		
		if (world->graphics->message == NULL) {
			
			printf("Memory allocation error: in function cb_button_release\n");
			exit(-1);
		}

		strcpy(world->graphics->message, "Drew Too Quickly");
		g_array_free (world->graphics->user_points, TRUE);
		initialize_array(world);

		return TRUE;
    }

	else if (!world->draw_success) {

		world->graphics->message = (char *)malloc(20 * sizeof(char));

		if (world->graphics->message == NULL) {

			printf("Memory allocation error: in function cb_button_release\n");
			exit(-1);
		}

		strcpy(world->graphics->message, "Drew Outside Box");

		g_array_free (world->graphics->user_points, TRUE);
		initialize_array(world);

		return TRUE;
	}

    else {

		free(world->graphics->message);
		world->graphics->message = NULL;
    }

    int skip_interval = world->graphics->user_points->len / DESIRED_NUMBER_VERTICES;
    int counter = 0;

    for (int i = 0; i < world->graphics->user_points->len; i++) {
		
		counter++;
		
		if (counter % skip_interval != 0) {
			
			g_array_remove_index (world->graphics->user_points, i);
			i--;
		}
    }


    //SEND INFO TO SERVER
    char *send_string = protocol_new_body (color_to_string(world->graphics->color),
					   (cpVect *)world->graphics->user_points->data,
					   world->graphics->user_points->len, -1, world->mass);

    //-1 is arbitrary since server decides IDs for bodies
    int length = strlen(send_string);

    sendall(world->socket, send_string, length);

    g_array_free (world->graphics->user_points, TRUE);
    initialize_array(world);

    return TRUE;
}

/*
	callback function for mouse dragging

	parameters: gtk widget and gpointer data, as gtk desires

	returns: gboolean
 */
static gboolean 
motion_notify_event_cb (GtkWidget *widget, GdkEventMotion *event, gpointer data) {

    gui_world *world = (gui_world *)data;

    int x, y;
    GdkModifierType state;

    gdk_window_get_pointer (event->window, &x, &y, &state);

    int space_height = 50, space_width = 50;

    /* convert into cp values */
    int window_height = gtk_widget_get_allocated_height(world -> graphics -> drawing_screen);
    int window_width = gtk_widget_get_allocated_width(world -> graphics -> drawing_screen);

    float screen_height_ratio = (float) window_height / space_height;
    float screen_width_ratio = (float) window_width / space_width;

    cpFloat x1 = (x - window_width / 2) / screen_width_ratio;
    cpFloat y1 = (-y + window_height / 2) / screen_height_ratio;


    if (state & GDK_BUTTON1_MASK) 
		add_user_point (x1, y1, world);

    return TRUE;
}

/*
	main function. initializes gtk, starts the threads

 */
int 
main(int argc, char **argv) {

    //basic check of the arguments
    if (argc !=2) {
	
		perror("Usage: need to input the ip address");
		exit(1);
    }

    //begin socket creation
    int sockfd = server_create_socket(SERV_PORT);

    struct sockaddr_in servaddr;

    //Creation of the socket
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr= inet_addr(argv[1]);
    servaddr.sin_port =  htons(SERV_PORT); //convert to big-endian order

    //Connection of the client to the socket
    if (connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr))<0) 	{

		perror("Problem in connecting to the server");
		exit(3);
    }

    gui_world world;
    world.graphics = (graphics_world *) malloc(sizeof(graphics_world));
   
	if (world.graphics == NULL) {

		printf("Memory allocation error: in function main\n");
		exit(-1);
	}

	world.graphics->display = false;
    world.graphics->color = conv_color("red");
	world.mass = 1;
    world.space = world.graphics->space = cpSpaceNew();
    world.socket = sockfd;
    world.try_number = 0;
    pthread_mutex_t mutex_lock = PTHREAD_MUTEX_INITIALIZER;
    world.space_lock = &mutex_lock;
    pthread_mutex_t network_lock = PTHREAD_MUTEX_INITIALIZER;
    world.socket_lock = &network_lock;
    pthread_mutex_t start_lock = PTHREAD_MUTEX_INITIALIZER;
    world.initial_lock = &start_lock;
	pthread_mutex_t text_lock = PTHREAD_MUTEX_INITIALIZER;
    world.text_lock = &text_lock;
    world.terminate_thread = false;
	world.graphics->message = NULL;
	world.angles = NULL;
	world.positions = NULL;
	world.num_bodies = 0;

    initialize_array(&world);

    GtkWidget *frame;
    GtkWidget *da;
    GtkWidget *vbox;
    GtkWidget *menubar;
    GtkWidget *color_menu;
    GtkWidget *color;
    GtkWidget *red;
    GtkWidget *yellow;
    GtkWidget *green;
    GtkWidget *blue;

    gtk_init (&argc, &argv);
    world.graphics -> window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size (GTK_WINDOW(world.graphics -> window), 400, 400);
    gtk_window_move(GTK_WINDOW(world.graphics -> window), 100, 100);

    gtk_window_set_title(GTK_WINDOW(world.graphics -> window), "DropZone");

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    // Make the color menu
    menubar = gtk_menu_bar_new();
    color_menu = gtk_menu_new();
	GtkWidget *mass_menu = gtk_menu_new();
	GtkWidget *level_menu = gtk_menu_new();

    GtkWidget *level_name = gtk_menu_item_new_with_label ("Level Select");

    gtk_menu_item_set_submenu(GTK_MENU_ITEM(level_name), level_menu);

	for (int i = 1; i <= 10; i++) {

		char name[10];
		sprintf(name, "Level %d", i);
		GtkWidget *level = gtk_menu_item_new_with_label(name);
		sprintf(name, "%d", i);
		gtk_widget_set_name (level, name);
		g_signal_connect (level, "activate", G_CALLBACK (cb_change_level), &world);
		gtk_menu_shell_append(GTK_MENU_SHELL(level_menu), level);
	}

    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), level_name);


    // Attach the color changing callbacks
    color = gtk_menu_item_new_with_label("Color");
    red = gtk_menu_item_new_with_label("red");
    g_signal_connect (red, "activate", G_CALLBACK (cb_color_change), &world);
    gtk_widget_set_name (red, "red");
    yellow = gtk_menu_item_new_with_label("yellow");
    g_signal_connect (yellow, "activate", G_CALLBACK (cb_color_change), &world);
    gtk_widget_set_name (yellow, "yellow");
    green = gtk_menu_item_new_with_label("green");
    gtk_widget_set_name (green, "green");
    g_signal_connect (green, "activate", G_CALLBACK (cb_color_change), &world);
    blue = gtk_menu_item_new_with_label("blue");
    gtk_widget_set_name (blue, "blue");
    g_signal_connect (blue, "activate", G_CALLBACK (cb_color_change), &world);

    gtk_menu_item_set_submenu(GTK_MENU_ITEM(color), color_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(color_menu), red);
    gtk_menu_shell_append(GTK_MENU_SHELL(color_menu), yellow);
    gtk_menu_shell_append(GTK_MENU_SHELL(color_menu), green);
    gtk_menu_shell_append(GTK_MENU_SHELL(color_menu), blue);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), color);

	GtkWidget *mass = gtk_menu_item_new_with_label("Mass");
	GtkWidget *half = gtk_menu_item_new_with_label("0.5 kg");
	g_signal_connect (half, "activate", G_CALLBACK(cb_mass_change), &world);
	gtk_widget_set_name (half, "0.5");
	GtkWidget *one = gtk_menu_item_new_with_label("1 kg");
	g_signal_connect (one, "activate", G_CALLBACK(cb_mass_change), &world);
	gtk_widget_set_name (one, "1");
	GtkWidget *five = gtk_menu_item_new_with_label("5 kg");
	g_signal_connect (five, "activate", G_CALLBACK(cb_mass_change), &world);
	gtk_widget_set_name (five, "5");
	GtkWidget *ten = gtk_menu_item_new_with_label("10 kg");
	g_signal_connect (ten, "activate", G_CALLBACK(cb_mass_change), &world);
	gtk_widget_set_name (ten, "10");
	GtkWidget *twenty = gtk_menu_item_new_with_label("20 kg");
	g_signal_connect (twenty, "activate", G_CALLBACK(cb_mass_change), &world);
	gtk_widget_set_name (twenty, "20");

	gtk_menu_item_set_submenu(GTK_MENU_ITEM(mass), mass_menu);
	gtk_menu_shell_append(GTK_MENU_SHELL(mass_menu), half);
	gtk_menu_shell_append(GTK_MENU_SHELL(mass_menu), one);
	gtk_menu_shell_append(GTK_MENU_SHELL(mass_menu), five);
	gtk_menu_shell_append(GTK_MENU_SHELL(mass_menu), ten);
	gtk_menu_shell_append(GTK_MENU_SHELL(mass_menu), twenty);
	gtk_menu_shell_append(GTK_MENU_SHELL(menubar), mass);

	gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, FALSE, 3);

    // Add menu to grid
    GtkWidget *grid = gtk_grid_new();
    gtk_container_add (GTK_CONTAINER(world.graphics->window), grid);
    gtk_grid_attach ((GtkGrid *)grid, vbox, 0, 0, 1, 1);

    g_signal_connect (GTK_WINDOW(world.graphics -> window), "destroy",
		      G_CALLBACK (pre_quit), &world);

    frame = gtk_frame_new (NULL);
    gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);

    gtk_grid_attach ((GtkGrid *)grid, frame, 0, 1, 400, 400);
    gtk_widget_set_size_request (frame, 400, 400);

    da = gtk_drawing_area_new ();
    gtk_container_add (GTK_CONTAINER (frame), da);

	world.graphics->drawing_screen = da;

    gtk_widget_add_events(da, GDK_BUTTON_PRESS_MASK);
    gtk_widget_add_events(da, GDK_BUTTON_RELEASE_MASK);
    gtk_widget_add_events(da, GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK);

//---------------------------------

	world.label = gtk_label_new("");
	GtkWidget *chatbox_text = gtk_text_view_new();
	GtkWidget *chatbox_button = gtk_button_new_with_label("send");
	gtk_label_set_line_wrap ( (GtkLabel *) world.label, TRUE );


	gtk_grid_attach ((GtkGrid *)grid, world.label, 0, 401, 200, 100);
	gtk_grid_attach ((GtkGrid *)grid, chatbox_text, 201, 401, 200, 90);
	gtk_grid_attach ((GtkGrid *)grid, chatbox_button, 201, 491, 200, 10);
	gtk_widget_set_size_request (world.label, 200, 100);
	gtk_widget_set_size_request (chatbox_text, 200, 90);
	gtk_widget_set_size_request (chatbox_button, 200, 10);
	gtk_text_view_set_wrap_mode ((GtkTextView *)chatbox_text, GTK_WRAP_CHAR);

	world.textbox_buffer = gtk_text_view_get_buffer ((GtkTextView *) chatbox_text);

	g_signal_connect (chatbox_button, "clicked", G_CALLBACK (chatbox_button_pressed), &world);

//---------------------------------

    /* Signals used to handle the backing surface */
    g_signal_connect (da, "draw", G_CALLBACK (draw_cb), &world);

    g_signal_connect (da, "motion-notify-event", G_CALLBACK (motion_notify_event_cb), &world);
    g_signal_connect (da, "button-press-event", G_CALLBACK (cb_button_press), &world);
    g_signal_connect (da, "button-release-event", G_CALLBACK (cb_button_release), &world);

    gtk_widget_show_all (world.graphics -> window);

    pthread_t world_updater_thread;
    pthread_create(&world_updater_thread, NULL, listener_thread, &world);
    gtk_main();

	world_free_space (world.space);

    return 0;
}


#include <chipmunk/chipmunk.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include "specs/common.h"
#include "specs/graphics.h"
#include "specs/physics.h"

#define DESIRED_NUMBER_VERTICES 10
#define TEXT_BOX_BUFFER_LIMIT 141

/*
  gui_world struct
  contains the data needed to represent
  and run the world. *graphics holds the graphics pointer
  shared with graphics.c, *physics holds the pointer of data
  shared with physics.c, and the press and release integers
  stores the coordinates of mouse clicks for box creation.
 */
typedef struct {
    graphics_world *graphics;
    world_status *physics;
    int press_x;
    int press_y;
    int release_x;
    int release_y;
    int try_number;
    COLOR color;
	int level;
	bool draw_success;
	int mass;
	bool stop; //so my reset button works, but it doesn't
	GtkTextBuffer *textbox_buffer;
	GtkWidget *label;
	bool delete_text;
} gui_world;

/*
  function prototypes for callback functions passed
  to gtk connect functions
 */
static gboolean draw_cb (GtkWidget *widget, cairo_t *cr, gpointer data);
static gboolean time_handler(gpointer data);
//static gboolean configure_event_cb (GtkWidget *widget, GdkEventConfigure *event, gpointer data);
static gboolean cb_color_change (GtkWidget *widget, gpointer data);
static void initialize_array (gui_world *world);
static gboolean cb_button_press (GtkWidget *widget, GdkEventButton *event, gpointer data);
static void add_user_point (float x, float y, gui_world *world);
static void delete_text (gui_world *world);

static gboolean
new_game (gui_world *world) {

	cpFloat time_step = 1.0/60.0;

	world_free (world->physics);

	world->mass = 1;
	
	//world->physics = (world_status *)malloc(sizeof(world_status));
    world->physics = world_new(world->level, time_step);
	
    world->graphics -> space = world->physics -> space;

	if (world->physics->drawing_box) {
		world->graphics->x1 = world->physics->drawing_box_x1;
		world->graphics->y1 = world->physics->drawing_box_y1;
		world->graphics->x2 = world->physics->drawing_box_x2;
		world->graphics->y2 = world->physics->drawing_box_y2;
		world->graphics->display = true;
	}
	else {
		world->graphics->display = false;
	}

    world->try_number = 1;

	return false;
}

gboolean reset_game (gui_world *world) {
	world->stop = true;
	new_game (world);
	world->stop = false;
	
	return false;
}


gboolean 
cb_open_file (GtkWidget *widget, gpointer data) {

	GtkWindow *parent_window = (GtkWindow *)data;

	GtkWidget *dialog;
     
    dialog = gtk_file_chooser_dialog_new ("Open File",
    				      parent_window,
     				      GTK_FILE_CHOOSER_ACTION_OPEN,
     				      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
     				      GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
     				      NULL);
     
    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
    {
    	char *filename;
    
        filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
        //open_file (filename);
        g_free (filename);
    }
     
    gtk_widget_destroy (dialog);

	return FALSE;

}

/*
  draw_cb

  iterates over the objects in the world struct to paint them.

  parameters: gtk widget pointer, cairo pointer and gui_world pointer
 */
static gboolean 
draw_cb (GtkWidget *widget, cairo_t *cr, gpointer data) {
    gui_world *world = (gui_world *) data;
	//world->graphics->drawing_screen = widget;
    world -> graphics -> cr = cr;
    graphics_space_iterate (world -> graphics);
    return FALSE;
}


/*
  time_handler

  basic timestep, updates and draws the world

  parameters: gui_world pointer
 */
static gboolean 
time_handler(gpointer data) {

    gui_world *world = (gui_world *) data;
    //cpSpaceStep(world -> space, 1.0/50);

	if (!world->stop) {
    	world_update(world -> physics);
    	//graphics_space_iterate(world -> graphics);
    	gtk_widget_queue_draw(world -> graphics -> window);

		if (world->delete_text) {
			delete_text(world);
		}

		if (world->physics->status == 1) {
			
			if (world->graphics->message != NULL) {
				free (world->graphics->message);
			}	
			world->graphics->message = (char *)malloc(20 * sizeof(char));
			strcpy(world->graphics->message, "You Win!");
			world->level = world->level % 10 + 1;
			new_game (world);
			//gtk_main_quit();
		}

	}
    return 1;
}

/*
  cb_press

  sets the callback to update x,y coordinates for the initial mouse click

  parameters: gtk widget pointer, cairo pointer and gui_world pointer

static gboolean
cb_press (GtkWidget *widget, GdkEventButton *event, gpointer data) {
    gui_world *world = (gui_world *) data;
	

    if (event -> button == 1) {
	world -> press_x = event -> x;
	world -> press_y = event -> y;
    }

    return FALSE;
}
*/

/*
	cb_color_change

*/
static gboolean 
cb_color_change (GtkWidget *widget, gpointer data) {

	gui_world *world = (gui_world *)data;

	COLOR new_color = conv_color(gtk_widget_get_name(widget));
	world->color = new_color;

	return FALSE;

}

static gboolean 
cb_mass_change (GtkWidget *widget, gpointer data) {

	gui_world *world = (gui_world *)data;

	world->mass = atoi(gtk_widget_get_name(widget));

	return FALSE;

}


//Checks whether the max number of characters has been reached
static gboolean 
max_length_detect (GtkWidget *widget, gpointer data) {
	gui_world *world = (gui_world *)data;

	GtkTextIter start, end;

	//gtk_text_buffer_set_text (world->textbox_buffer, "TEXT", -1);
	
	gtk_text_buffer_get_bounds(world->textbox_buffer, &start, &end);

	//gtk_text_buffer_delete (world->textbox_buffer, &start, &end);

	//gtk_text_buffer_get_start_iter(world->textbox_buffer, &start);
	//gtk_text_buffer_get_iter_at_offset(world->textbox_buffer, &start, TEXT_BOX_BUFFER_LIMIT);
	//gtk_text_buffer_get_end_iter(world->textbox_buffer, TEXT_BOX_BUFFER_LIMIT + 1);

	char *buffer_contents = gtk_text_buffer_get_text((GtkTextBuffer *)world->textbox_buffer, &start, &end, false);	
	
	if (strlen(buffer_contents) > TEXT_BOX_BUFFER_LIMIT) {
		
		world->delete_text = true;
	
		//char substring[TEXT_BOX_BUFFER_LIMIT + 1];
		//strncpy(substring, buffer_contents, TEXT_BOX_BUFFER_LIMIT);

		//test(world);
		//gtk_text_buffer_get_iter_at_offset(world->textbox_buffer, &start, TEXT_BOX_BUFFER_LIMIT);
		
		//gtk_text_buffer_delete (world->textbox_buffer, &start, &end);
		//gtk_text_buffer_set_text (world->textbox_buffer, "TEXT", -1);
		//gtk_text_buffer_insert(world->textbox_buffer, &start, substring, strlen(substring));

		//gtk_text_buffer_get_bounds(world->textbox_buffer, &start, &end);
	}

	return TRUE;
}

static void 
delete_text (gui_world *world) {

	GtkTextIter start, end;

	gtk_text_buffer_get_iter_at_offset(world->textbox_buffer, &start, TEXT_BOX_BUFFER_LIMIT);
	gtk_text_buffer_get_end_iter(world->textbox_buffer, &end);
	gtk_text_buffer_delete (world->textbox_buffer, &start, &end);
	
	world->delete_text = false;

}

static gboolean 
chatbox_button_pressed (GtkWidget *widget, gpointer data) {

	gui_world *world = (gui_world *)data;

	GtkTextIter start, end;
	
	gtk_text_buffer_get_bounds(world->textbox_buffer, &start, &end);

	char *buffer_contents = gtk_text_buffer_get_text((GtkTextBuffer *)world->textbox_buffer, &start, &end, false);

	gtk_label_set_text ((GtkLabel *)world->label, buffer_contents);

	gtk_text_buffer_delete (world->textbox_buffer, &start, &end);

	//gtk_text_buffer_set_text (world->textbox_buffer, "TEXT", -1);

	return FALSE;
}

/*
  cb_box_add

  Handle motion events by continuing to draw if button 1 is
  still held down. The ::motion-notify signal handler receives
  a GdkEventMotion struct which contains this information.

  parameters: gtk widget pointer, cairo pointer and gui_world pointer
 
static gboolean 
cb_box_add (GtkWidget *widget, GdkEventMotion *event, gpointer data) {

    int space_height = 50, space_width = 50;
    gui_world *world = (gui_world *) data;

    //convert into cp values 
    int window_height = gtk_widget_get_allocated_height(world -> graphics -> window);
    int window_width = gtk_widget_get_allocated_width(world -> graphics -> window);

    float screen_height_ratio = (float) window_height / space_height;
    float screen_width_ratio = (float) window_width / space_width;


    //cpFloat cp_press_x = (world -> press_x - window_width / 2) / screen_width_ratio;
    //cpFloat cp_press_y = (world -> press_y - window_height / 2) / screen_height_ratio;
    //cpFloat cp_release_x = (event -> x - window_width / 2) / screen_width_ratio;
    //cpFloat cp_release_y = (event -> y - window_height / 2) / screen_height_ratio;


    cpFloat cp_press_x = (world -> press_x - window_width / 2) / screen_width_ratio;
    cpFloat cp_press_y = (-world -> press_y + window_height / 2) / screen_height_ratio;
    cpFloat cp_release_x = (event -> x - window_width / 2) / screen_width_ratio;
    cpFloat cp_release_y = (-event -> y + window_height / 2) / screen_height_ratio;

    //checks if the player has anymore smites,
     //quits if the player is out of smites 
    if (world -> try_number > 3) {
		new_game (world);
		return FALSE;
		//gtk_main_quit();
    }

    cpFloat centroid_x = (cp_press_x + cp_release_x) / 2;
    cpFloat centroid_y = (cp_press_y + cp_release_y) / 2;

    cpFloat width, height;
    if (cp_press_x > cp_release_x)
	width = (cp_press_x - cp_release_x) / 2;
    else
	width = (cp_release_x - cp_press_x) / 2;

    if (cp_press_y > cp_release_y)
	height = cp_press_y - cp_release_y;
    else
	height = cp_release_y - cp_press_y;

    if (width > 0 && height > 0) {
	world_add_object (world -> physics, cpv(centroid_x, centroid_y),
        width, height, world->color, PLAYER_BOX_COLLISION_NUMBER);
	(world -> try_number)++;
    }

    return FALSE;
}
*/


static gboolean 
cb_button_press (GtkWidget *widget, GdkEventButton *event, gpointer data) {

	gui_world *world = (gui_world *)data;

	world->draw_success = true;

	int space_height = 50, space_width = 50;

    /* convert into cp values */
    int window_height = gtk_widget_get_allocated_height(world -> graphics -> drawing_screen);
    int window_width = gtk_widget_get_allocated_width(world -> graphics -> drawing_screen);

    float screen_height_ratio = (float) window_height / space_height;
    float screen_width_ratio = (float) window_width / space_width;

	cpFloat x = (event->x - window_width / 2) / screen_width_ratio;
    cpFloat y = (-event->y + window_height / 2) / screen_height_ratio;

	if (event->button == 1) 
		add_user_point (x, y, world);
	
	return TRUE;
}

static void 
initialize_array (gui_world *world) {

	/*
	if (world->user_points != NULL) {
		g_array_free (world->user_points, TRUE);
	}
	*/
	world->graphics->user_points = g_array_new (FALSE, FALSE, sizeof(cpVect));
}

static void
add_user_point (float x, float y, gui_world *world) {
	cpVect point = cpv(x,y);

	if ((world->physics->drawing_box) && (x < world->graphics->x1 || y < world->graphics->y1 || x > world->graphics->x2 || y > world->graphics->y2)) {
		world->draw_success = false;
	}
	
	world->graphics->user_points = g_array_append_vals(world->graphics->user_points, &point, 1);
}

static gboolean
cb_button_release (GtkWidget *widget, GdkEventMotion *event, gpointer data) {
	
	gui_world *world = (gui_world *)data;

	if (world->graphics->user_points->len < 50) {
		world->graphics->message = (char *)malloc(20 * sizeof(char));
		strcpy(world->graphics->message, "Size Too Small");

		g_array_free (world->graphics->user_points, TRUE);
		initialize_array(world);

		return TRUE;
	}
	
	else if (!world->draw_success) {
		world->graphics->message = (char *)malloc(20 * sizeof(char));
		strcpy(world->graphics->message, "Drew Outside Box");

		g_array_free (world->graphics->user_points, TRUE);
		initialize_array(world);
		
		return TRUE;
	} 

	else {
		free(world->graphics->message);
		world->graphics->message = NULL;
	}

	if (world -> try_number > 3) {
		if (world->graphics->message != NULL) {
			free (world->graphics->message);
		}	
		world->graphics->message = (char *)malloc(20 * sizeof(char));
		strcpy(world->graphics->message, "You Lose!");

		g_array_free (world->graphics->user_points, TRUE);

		initialize_array(world);

		new_game (world);
		return FALSE;
		//gtk_main_quit();
    }

	(world -> try_number)++;
	
	int skip_interval = world->graphics->user_points->len / DESIRED_NUMBER_VERTICES;

	int counter = 0;

	for (int i = 0; i < world->graphics->user_points->len; i++) {

		counter++;

		if (counter % skip_interval != 0) {

			g_array_remove_index (world->graphics->user_points, i);
			i--;
		}
	}

	create_user_object ((cpVect *)world->graphics->user_points->data, world->graphics->user_points->len, world->color, world->physics, PLAYER_BOX_COLLISION_NUMBER, 0.7, world->mass); 

	g_array_free (world->graphics->user_points, TRUE);

	initialize_array(world);

	return TRUE;

}

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


	if (state & GDK_BUTTON1_MASK) {
		add_user_point (x1, y1, world);
	}

	return TRUE;

}

/*
  main

  main function, sets up the gtk GUI, the graphics and physics worlds
  and then calls gtk_main().

  parameters: argc and *argv[] from the command line
*/
int
main ( int argc, char *argv[] ) {

	if (argc < 2) {
		printf("Must include an integer for level\n");
		exit(-1);
	}

    int level = atoi(argv[1]);
	if (level < 1 || level > 10) {
		printf("Must select level between 1 and 10\n");
		exit(-1);
	}

    //cpVect gravity = cpv(1, -100);
    cpFloat time_step = 1.0/60.0;

    gui_world world;
    world.graphics = (graphics_world *) malloc(sizeof(graphics_world));
    assert(world.graphics);
	world.color = conv_color("red");
    //world.physics = (world_status *)malloc(sizeof(world_status));
	//world.physics->drawing_box = false;
    world.physics = world_new(level, time_step);
    world.graphics -> space = world.physics -> space;
	world.graphics -> image = cairo_image_surface_create_from_png("balkcom2000.png");
	
	if (world.physics->drawing_box) {

		world.graphics->x1 = world.physics->drawing_box_x1;
		world.graphics->y1 = world.physics->drawing_box_y1;
		world.graphics->x2 = world.physics->drawing_box_x2;
		world.graphics->y2 = world.physics->drawing_box_y2;
		world.graphics->display = true;
	}
	else {
		world.graphics->display = false;
	}

	world.mass = 1;
    world.try_number = 1;
	world.level = level;
	world.stop = false;
	initialize_array (&world);


    GtkWidget *frame;
    GtkWidget *da;
    GtkWidget *vbox;
    GtkWidget *menubar;
	GtkWidget *filemenu;
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

    gtk_window_set_title( GTK_WINDOW(world.graphics -> window), "DropZone" );


	//Menu Begin-----------------------------------------------

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    //gtk_container_add(GTK_CONTAINER(world.graphics -> window), vbox);

    // Make the color menu
    menubar = gtk_menu_bar_new();
	filemenu = gtk_menu_new();
    color_menu = gtk_menu_new();
	GtkWidget *mass_menu = gtk_menu_new();
	GtkWidget *open_menu = gtk_image_menu_item_new_from_stock(GTK_STOCK_OPEN, NULL);
	//GtkWidget *reset = gtk_menu_item_new_with_label("Reset");

	GtkWidget *file_name = gtk_menu_item_new_with_label ("File");
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(file_name), filemenu);
	gtk_menu_shell_append(GTK_MENU_SHELL(filemenu), open_menu);
	//gtk_menu_shell_append(GTK_MENU_SHELL(filemenu), reset);
	gtk_menu_shell_append(GTK_MENU_SHELL(menubar), file_name);

	g_signal_connect (open_menu, "activate", G_CALLBACK (cb_open_file), world.graphics->window);

	//g_signal_connect (reset, "activate", G_CALLBACK (reset_game), &world);

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
	gtk_widget_set_name (half, "1");
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

	//Menu End-----------------------------------------

    // Add menu to grid
    GtkWidget *grid = gtk_grid_new();
    gtk_container_add (GTK_CONTAINER(world.graphics->window), grid);
    gtk_grid_attach ((GtkGrid *)grid, vbox, 0, 0, 1, 1);

    g_signal_connect (GTK_WINDOW(world.graphics -> window), "destroy", G_CALLBACK (gtk_main_quit), NULL);

    frame = gtk_frame_new (NULL);
    gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
    //gtk_container_add (GTK_CONTAINER (world.graphics -> window), frame);

    gtk_grid_attach ((GtkGrid *)grid, frame, 0, 1, 400, 400);
    gtk_widget_set_size_request (frame, 400, 400);

    da = gtk_drawing_area_new ();
    gtk_container_add (GTK_CONTAINER (frame), da);

	world.graphics->drawing_screen = da;

	//Set up chat box stuff---------------------------
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

	world.delete_text = false;

	world.textbox_buffer = gtk_text_view_get_buffer ((GtkTextView *) chatbox_text);

	g_signal_connect (world.textbox_buffer, "changed", G_CALLBACK (max_length_detect), &world);
	g_signal_connect (chatbox_button, "clicked", G_CALLBACK (chatbox_button_pressed), &world);

	//End chat box stuff---------------------------------

    gtk_widget_add_events(da, GDK_BUTTON_PRESS_MASK);
    gtk_widget_add_events(da, GDK_BUTTON_RELEASE_MASK);
	gtk_widget_add_events(da, GDK_POINTER_MOTION_MASK
							| GDK_POINTER_MOTION_HINT_MASK);

    /* Signals used to handle the backing surface */
    g_signal_connect (da, "draw", G_CALLBACK (draw_cb), &world);
    //g_signal_connect (da,"configure-event", G_CALLBACK (configure_event_cb), &world);

	g_signal_connect (da, "motion-notify-event", G_CALLBACK (motion_notify_event_cb), &world);
    g_signal_connect (da, "button-press-event", G_CALLBACK (cb_button_press), &world);
    g_signal_connect (da, "button-release-event", G_CALLBACK (cb_button_release), &world);

    g_timeout_add(40, (GSourceFunc) time_handler, (gpointer) &world);

    gtk_widget_show_all (world.graphics -> window);
    gtk_main ();

    //Free stuff
    
/*
    
	world_free (world.physics);
	   if (world.graphics -> box){
		   free(world.graphics -> box);
	   if (world.graphics -> window)
		   free(world.graphics -> window);
	   free(world.graphics);	
       if(world.graphics) 
		   if (world.graphics -> cr)
		   	  cairo_destroy(world.graphics -> cr);
    	}

*/
    return 0;
}


/*
  Creates a new surface if the window is resized.  Source for designing this
  method: draw.c
*//*
static gboolean configure_event_cb (GtkWidget *widget, GdkEventConfigure *event,
				    gpointer data) {


    graphics_world *world = (graphics_world *) data;

    printf("here configure BEFORE\n");

    if (world -> surface) // Destroys surface before making new surface
	cairo_surface_destroy (world -> surface);


    world -> surface = gdk_window_create_similar_surface(gtk_widget_get_window(widget),
							CAIRO_CONTENT_COLOR,
							gtk_widget_get_allocated_width(widget),
							gtk_widget_get_allocated_height(widget));

    printf("here configure AFTER\n");

    // Redraws the board
    graphics_space_iterate(world);
    return TRUE;
}

*/




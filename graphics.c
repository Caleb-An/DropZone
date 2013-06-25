#include <stdio.h>
#include <gtk/gtk.h>
#include <chipmunk/chipmunk.h>
#include <stdlib.h>
#include <stdbool.h>
#include "specs/common.h"
#include "specs/graphics.h"
#include "specs/physics.h"
/*
	graphics.c

	contains the functions for drawing everything to
	the GUI. The functions here are called by client
	and used in conjuction with data passed to client
	by the server. 

 */ 





/*
	drawing_coordinates struct

	contains data used by graphics_get_shape 
	and graphics_draw_body for iteration with 
	Chipmunk

 */
typedef struct {
	int capacity;
	int size;
	cpVect *positions;
} drawing_coordinates;


//function prototypes
static void graphics_set_color_from_body (cairo_t **cr, cpBody *body);
static void graphics_set_rgb_from_color (cairo_t **cr, COLOR color);
static void graphics_draw_body (cpBody *body, graphics_world *world);
static void graphics_get_shape (cpBody *body, cpShape *shape, drawing_coordinates *coords);
static void graphics_write_message (graphics_world *world);
static void graphics_draw_zone (graphics_world *world);
static void graphics_partial_shape (graphics_world *world);

/*
	iterates over the bodies of the cpSpace inside the world
	and calls graphics_draw_body on each body	

	Parameters:
		*world = graphics world 

	Returns: nothing
 */
void 
graphics_space_iterate (graphics_world *world) {

	//draw the drawing zone
	if (world->display)
		graphics_draw_zone (world);

	//draw the outline of the user's object
	if (world->user_points->len > 0)
		graphics_partial_shape (world);

	//chipmunk iterator for each body
	cpSpaceEachBody(world->space, (cpSpaceBodyIteratorFunc) graphics_draw_body, world);
	
	//draw the ground
	graphics_draw_body (world->space->staticBody, world);
	
	
	graphics_write_message (world);
}

/*
	draws the line tracing the user's drawing in
	the currently set color

	Parameters:
		*world = graphics world 

	Returns: nothing
 */
static void
graphics_partial_shape (graphics_world *world) {

	int space_height = 50, space_width = 50;
    int window_height = gtk_widget_get_allocated_height(world -> drawing_screen);
    int window_width = gtk_widget_get_allocated_width(world -> drawing_screen);

	float screen_height_ratio = (float)window_height / space_height;
	float screen_width_ratio = (float)window_width / space_width;
	
	static const double dashed[] = {14.0, 6.0};
  	static int len  = sizeof(dashed) / sizeof(dashed[0]);

	cairo_t *cr = gdk_cairo_create (gtk_widget_get_window(world->drawing_screen));

	cairo_set_dash(cr, dashed, len, 1);

	graphics_set_rgb_from_color (&cr, world->color);

	cairo_translate (cr, window_width / 2 , window_height / 2);

	cpVect *vect = (cpVect *)world->user_points->data;

	cairo_move_to (cr, screen_width_ratio * vect[0].x, -screen_height_ratio * vect[0].y);

	//connect each user input mouse point with dashes
	for (int i = 1; i < world->user_points->len; i++) {
		cairo_line_to (cr, screen_width_ratio * vect[i].x, -screen_height_ratio * vect[i].y);
	}
	
	cairo_stroke(cr);

	cairo_destroy(cr);
}

/*
	writes the message to the screen

	Parameters:
		*world = graphics world 

	Returns: nothing
 */
static void
graphics_write_message (graphics_world *world) {

	int window_height = gtk_widget_get_allocated_height(world -> drawing_screen);
    int window_width = gtk_widget_get_allocated_width(world -> drawing_screen);
	cairo_text_extents_t extents;

	cairo_t *cr = gdk_cairo_create (gtk_widget_get_window(world->drawing_screen));

	cairo_set_source_rgb (cr, 1, 0, 0);

	cairo_select_font_face(cr, "Purisa", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);

  	cairo_set_font_size(cr, 15);

	// null checking
	if (world->message != NULL) {

		cairo_text_extents(cr, world->message, &extents);
		cairo_move_to (cr, window_width / 2 - extents.width / 2, window_height / 10);
		cairo_show_text (cr, world->message);
	}

	cairo_destroy (cr);
}

/*
	draws the drawing zone in which
	the player is allowed to draw

	Parameters:
		*world = graphics world 

	Returns: nothing
 */
static void
graphics_draw_zone (graphics_world *world) {
	
	int space_height = 50, space_width = 50;
    int window_height = gtk_widget_get_allocated_height(world -> drawing_screen);
    int window_width = gtk_widget_get_allocated_width(world -> drawing_screen);

	float screen_height_ratio = (float)window_height / space_height;
	float screen_width_ratio = (float)window_width / space_width;

	cairo_t *cr = gdk_cairo_create (gtk_widget_get_window(world->drawing_screen));

	cairo_set_source_rgba(cr, 0, 0, 0, 1);
	
	cairo_translate (cr, window_width / 2 , window_height / 2);

	//Dash influenced from zetcode.com
	static const double dashed[] = {14.0, 6.0};
  	static int len  = sizeof(dashed) / sizeof(dashed[0]);

	cairo_set_dash(cr, dashed, len, 1);

  	cairo_move_to(cr, screen_width_ratio * world->x1, -screen_height_ratio * world->y1);  
  	cairo_line_to(cr, screen_width_ratio * world->x1, -screen_height_ratio * world->y2);
	cairo_line_to(cr, screen_width_ratio * world->x2, -screen_height_ratio * world->y2);
	cairo_line_to(cr, screen_width_ratio * world->x2, -screen_height_ratio * world->y1);
	cairo_line_to(cr, screen_width_ratio * world->x1, -screen_height_ratio * world->y1);
  	cairo_stroke(cr);

	cairo_destroy(cr);

}

/*
	draws the body to the screen

	Parameters:
		*body = body to be drawn
		*world = graphics world 

	Returns: nothing
 */
static void
graphics_draw_body ( cpBody *body, graphics_world *world ) {

	cpVect point_position = cpBodyGetPos (body);
	cpFloat angle = cpBodyGetAngle (body);

	drawing_coordinates *coords = (drawing_coordinates *) malloc(sizeof(drawing_coordinates));

	//null checking
	if (coords == NULL) {

		printf("Memory allocation error: in graphics_draw_body\n");
		exit(-1);
	}
	coords->capacity = 1;
	coords->size = 0;
	coords->positions = (cpVect *) malloc(coords->capacity * sizeof(cpVect));

	//null checking
	if (coords->positions == NULL) {

		printf("Memory allocation error: in graphics_draw_body\n");
		exit(-1);
	}

	//iterate over each shape for new coordinates
	cpBodyEachShape(body, (cpBodyShapeIteratorFunc) graphics_get_shape, coords);

	if (coords->size > 0) {

		int space_height = 50, space_width = 50;
    	int window_height = gtk_widget_get_allocated_height(world -> drawing_screen);
    	int window_width = gtk_widget_get_allocated_width(world -> drawing_screen);

		float screen_height_ratio = (float)window_height / space_height;
		float screen_width_ratio = (float)window_width / space_width;

		cairo_t *cr = gdk_cairo_create (gtk_widget_get_window(world->drawing_screen));

		graphics_set_color_from_body (&cr, body);

		cairo_matrix_t saved_transform;
		cairo_get_matrix(cr, &saved_transform);

		cairo_set_line_width (cr, 6);

		cairo_translate (cr, window_width / 2 + screen_width_ratio * point_position.x, window_height - window_height / 2 - screen_height_ratio * point_position.y);

		cairo_rotate(cr, -angle);

		cairo_move_to (cr, screen_width_ratio * coords->positions[0].x, -screen_height_ratio * coords->positions[0].y);

		// creates the outline of the shape
		for (int i = 1; i < coords->size; i++) {
		
			cairo_line_to (cr, screen_width_ratio * coords->positions[i].x, -screen_height_ratio * coords->positions[i].y);

			cairo_stroke_preserve (cr);
		}
		
		// close and fill
		cairo_close_path(cr);
		cairo_fill (cr);
		
		cairo_destroy(cr);

	}

	free (coords->positions);
	free (coords);
}

/*
	updates the coordinates of the shape being drawn

	Parameters:
		*body = body being drawn
		*shape = shape representation of it
		*coords = coordinates of the last draw callback 

	Returns: nothing
 */
static void
graphics_get_shape (cpBody *body, cpShape *shape, drawing_coordinates *coords) {
	
	coords->size += 1;
	
	if (coords->size >= coords->capacity) {
		coords->capacity *= 2;
		coords->positions = (cpVect *)realloc(coords->positions, coords->capacity * sizeof(cpVect));
	}

	coords->positions[coords->size - 1] = cpSegmentShapeGetA (shape);
}

/*
	Sets the color according of the cairo pointer according to the shape color

	Parameters:
    	**cr = cairo pointer that is used to draw to drawing area
    	shape = shape that must be drawn on screen

	Returns: nothing
 */
static void
graphics_set_color_from_body (cairo_t **cr, cpBody *body) {

	body_information *info = cpBodyGetUserData(body);	

	if ((info == NULL) || (&(info->color) == NULL)) {
		cairo_set_source_rgb (*cr, 0, 0, 0);
	}
	else {
		graphics_set_rgb_from_color (cr, info->color);
	}
}

/*
	sets the rgb of the shapes based off the color
	type passed to it. 

	parameters: cairo context, the color

	returns: nothing
 */
static void
graphics_set_rgb_from_color (cairo_t **cr, COLOR color) {
	
	if (color == GREEN) {
		cairo_set_source_rgb (*cr, 0, 255, 0);
	}

	else if (color == RED) {
		cairo_set_source_rgb (*cr, 255, 0, 0);
	}

	else if (color == BLUE) {
		cairo_set_source_rgb (*cr, 0, 0, 255);
	}

	else if (color == YELLOW) {
		cairo_set_source_rgb (*cr, 200, 200, 0);
	}
}

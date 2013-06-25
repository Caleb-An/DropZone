#ifndef GRAPHICS_H
#define GRAPHICS_H



/*
  This struct contains the two pieces of information: the cpSpace and the drawing
  box.  It is passed from gui.c to graphics.c.  This allows gui.c to let graphics
  know the objects and positions to draw the state of the game to the window and
  whether the graphics window should display the drawing box for player input.
 */


typedef struct  {
    cpSpace *space;
    bool display; // whether the drawing box should be displayed
    float x1; // the x-coordinate of the upper left corner of the drawing box
    float y1; // the y-coordinate of the upper right corner of the drawing box
    float x2; // the width of the box
    float y2; // the height of the box
    GtkWidget *window;
	GtkWidget *drawing_screen;
    cairo_t *cr;
	char *message;
	GArray *user_points;
	COLOR color;
	cairo_surface_t *image;
} graphics_world;


/*
  The iterate_space function will be used to look through all the items in world
  and display them one at a time (by using other functions). This will be called
  directly by the GUI callback within the GUI file.

  Parameters:
      cairo_t *cr - pointer to the cairo context
      graphics_world *world - pointer to the graphics_world struct

  Returns: nothing
 */
void graphics_space_iterate (graphics_world *world);


#endif

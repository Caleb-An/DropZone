//#include "chipmunk.h"

#define TARGET_COLLISION_NUMBER 1
#define PLAYER_BOX_COLLISION_NUMBER 2
#define OBSTACLE_COLLISION_NUMBER 3

#ifndef PHYSICS_H
#define PHYSICS_H



/*
  The reason for this struct is so that the GUI can have both a pointer to the
  cpSpace (to give to graphics) and know the status of the game (whether the
  player has won, lost, or the game is still being played).  The drawing_box_*
  variables are passed to GUI so that the GUI can create the drawing box and send
  that information to graphics.
 */

typedef struct {
    cpSpace *space;
    int status;
	bool drawing_box;
    float drawing_box_x1;
    float drawing_box_y1;
    float drawing_box_x2;
    float drawing_box_y2;
    float timestep;
	int body_count;
} world_status;

/*
  Sets up the cpSpace by adding gravity and creating the structures read from the
  file. Sets up gravity, creates bodies and their collision shapes, and adds them
  to the cpSpace. Adds masses and chooses a point within the rectangle to be the
  point of the mass.  Defines a cpSpaceAddCollisionHandler for collisions in
  layers 2, 3, or 4 that contains the target to ensure that the game know when
  the target is hit.  Called by gui.c.

  Parameters:
      level: the number level (1, 2, or 3) that is being set up.
      timestep: the time to step the world, determined by the GUI
      width: width of the window
      height: height of the window

  Returns: world_status struct containing world and the status of the game
 */
world_status *world_new(int level, float timestep);


/*
  Takes two points from the GUI and adds the box defined by that diagonal as a
  dynamic object to the world.  Adds this object to layer 1 with all other
  objects, and also a layer that corresponds to the number of boxes the player
  has drawn (+1).  This enable collisions between the target (which is the only
  other body in layers 2, 3 and 4) and the player’s drawn box to be detected to
  figure out when the game is over by examining only collisions between the
  second layer.  This function is called by gui.c.

  Parameters:
      x1: coordinates of the first point
      y1: y coordinate of the first point
      x2: x coordinate of the second point
      y2: y coordinate of the second point
      color: the rgb color that user chose to draw the box (in size 3 array)
      try_number: the # attempt, used calculate the layer of box

  Returns: nothing
 */
void world_add_object(world_status *world, cpVect vector, cpFloat width, cpFloat
		      height, COLOR color, cpCollisionType collision);

int
create_user_object (cpVect *vectors, int vector_size, COLOR color, world_status *world, cpCollisionType collision, cpFloat mu, cpFloat mass);

/*
  Called by the GUI at regular time intervals to advance time in the cpSpace.
  This function checks whether all bodies have stopped moving, and if so alters
  the status in world_status.  It returns the world_status so that the GUI and
  graphics can know when the game is over and where all the bodies in the space
  are.  If all boxes have stopped moving, then the status of the game is updated.
  If a box has moved off the screen, it is deleted from the space.  Called by
  gui.c

  Parameters: world- the latest status of the world so that a new status can be calculated

  Returns: nothing
 */
void *world_update(world_status *world);

cpBody *world_get_ground (cpSpace *space);


/*
  Frees all the shapes, bodies, and static bodies to prevent memory leaks.
  Called by gui.c

  Parameters: none

  Returns: nothing
 */
bool world_free (world_status *world );

bool world_free_space (cpSpace *space);

#endif

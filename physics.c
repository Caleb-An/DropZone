#include <stdio.h>
#include <stdlib.h>
#include <chipmunk/chipmunk.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include "specs/common.h"
#include "specs/physics.h"

#define LINE_RADIUS 0.5

//Prototypes for static functions
static void world_custom_read (FILE *ifile, world_status *world, cpCollisionType collision);
static void world_box_read (FILE *ifile, world_status *world, cpCollisionType collision);
static void world_line_read (FILE *ifile, world_status *world, cpCollisionType collision);
static void world_level_read (FILE *ifile, world_status *world);
static void world_ground_read (FILE *ifile, world_status *world);
static cpBool target_collision (cpArbiter *arbiter, cpSpace *space, void *data);

// Prototypes for functions that destroy the space and free memory
static void world_shape_post(cpSpace *space, cpShape *shape, void *unused);
static void world_shape_free(cpShape *shape, cpSpace *space);
static void world_constrain_post(cpSpace *space, cpConstraint *constraint, void
				 *unused);
static void world_constrain_free(cpConstraint *constraint, cpSpace *space);
static void world_body_post(cpSpace *space, cpBody *body, void *unused);
static void world_body_free(cpBody *body, cpSpace *space);

//Prototypes for testing functions
static cpVect center_calculation (cpVect *vectors, int vector_size);
static cpFloat moment_calculation (cpVect *vectors, int vector_size, cpVect center, cpFloat mass);
static void world_zone_read (FILE *ifile, world_status *world);

/*
	returns the ground from the cpSpace.

	Parameters:
		*space = cpSpace

	Returns:
		cpBody of the ground
 */
cpBody *world_get_ground (cpSpace *space) {
	return space->staticBody;
}

/*
	Checks for whether there is a collision between the target and the players's
	boxes.

	Parameters:
    	*arbiter = cpArbiter that detects collision with targets
    	*space = the cpSpace that simulates the game
    	*world = world_status that represents the status of the game

	Returns:
    	cpBool that represents whether the function has finished handling the
     	collision
 */
static cpBool
target_collision (cpArbiter *arbiter, cpSpace *space, void *data) {

	world_status *world = (world_status *)data;
	world->status = 1;

	return true;
}


/*
	Creates a new world_status

	Parameters:
    	level = int representing the level the user has chosen
    	timestep = the amount of time to step the world

	Returns:
		a new world_status
 */
world_status
*world_new(int level, float timestep) {

	world_status *world= malloc(sizeof(world_status));
	if (world == NULL) {
		printf("Memeory allocation error in function world_new\n");
		exit(-1);
	}

	world->status = 2;
	world->drawing_box_x1 = world->drawing_box_y1 = 0;
	world->drawing_box_x2 = world->drawing_box_y2 = 0;
	world->drawing_box = false;
	world->timestep = timestep;
	world->body_count = 0;
	world->space = cpSpaceNew();
	cpEnableSegmentToSegmentCollisions(); //muy importante

	char filename[40];

	sprintf( filename, "level%i.lvl", level );

	FILE *ifile;

	if ((ifile = fopen (filename, "r")) == NULL)
		exit(-1);
	
	cpSpaceSetGravity (world->space, cpv(0, -75)); //Set gravity

	world_level_read (ifile, world);

	cpSpaceAddCollisionHandler (world->space, TARGET_COLLISION_NUMBER,
				    PLAYER_BOX_COLLISION_NUMBER,
				    target_collision, NULL, NULL, NULL, world);

	return world;
}

/*
	Creates a body_information struct

	Parameters:
    	*vectors = cpVectors of the shape
		vector_size = int number of vectors
		color = COLOR struct 
		*world = world_status struct
		collision = CollisionType
		mu = cpFloat of the frictional coefficient
		mass = cpFloat of the mass	

	Returns:
		returns the body id number
 */
int
create_user_object (cpVect *vectors, int vector_size, COLOR color, world_status *world, 
cpCollisionType collision, cpFloat mu, cpFloat mass) {

	cpVect center = center_calculation (vectors, vector_size);

	cpVect vertices[vector_size];

	for (int i = 0; i < vector_size; i++)
		vertices[i] = cpvsub (vectors[i], center);

	cpFloat moment = moment_calculation (vertices, vector_size, center, mass);

	cpBody *body_new = cpSpaceAddBody(world->space, cpBodyNew(mass, moment));
	cpBodySetPos(body_new, center);
	body_information *info = (body_information *)malloc(sizeof(body_information));
	if (info == NULL) {
		printf("Memory allocation error: in function create_user_object\n");
		exit(-1);
	}

	info->color = color;
	info->body_id = world->body_count;
	world->body_count++;
	cpBodySetUserData(body_new, info);

	cpShape *line = cpSpaceAddShape (world->space, cpSegmentShapeNew (body_new, vertices[vector_size - 1], vertices[0], LINE_RADIUS));
	cpShapeSetFriction(line, mu);
	cpShapeSetCollisionType (line, collision);

	for (int i = vector_size - 2; i >= 0; i--) {
		cpShape *line = cpSpaceAddShape(world->space, cpSegmentShapeNew(body_new, vertices[i], vertices[i + 1], LINE_RADIUS));
		cpShapeSetFriction(line, mu);
		cpShapeSetCollisionType (line, collision);
	}

	return info->body_id;
}

/*
	calculates the center of the body

	Parameters:
    	*vectors = cpVectors of the shape
		vector_size = int number of vectors

	Returns:
		returns the center of the shape
 */
static cpVect
center_calculation (cpVect *vectors, int vector_size) {

	cpVect average = cpv(0,0);

	for (int i = 0; i < vector_size; i++) {
		average.x += vectors[i].x;
		average.y += vectors[i].y;
	}

	average.x /= vector_size;
	average.y /= vector_size;

	return average;
}

/*
	calcuates the moment of the body

	Parameters:
		*vectors = cpVectors of the edges
		vector_size = int number of vectors
		center = cpVector of the center
		mass = cpFloat mass of the object

	Returns:
		float of the moment
 */
static cpFloat
moment_calculation (cpVect *vectors, int vector_size, cpVect center, cpFloat mass) {

	cpFloat radius = 0;

	for (int i = 0; i < vector_size; i++) 
		radius += cpvdist (cpv(0,0), vectors[i]);

	radius /= vector_size;

	return cpMomentForCircle (mass, 0, radius, cpv(0,0));
}

/*
	Reads the level from the file.

	Parameters:
    	*ifile = FILE to read level from
    	*world = pointer to the world_status to fill up
 */
static void
world_level_read (FILE *ifile, world_status *world) {
	char input[100];
	int collision_type = OBSTACLE_COLLISION_NUMBER;

	while (fscanf(ifile, "%s", input) != EOF) {

		if (strcmp (input, "TARGET") == 0) {
			collision_type = TARGET_COLLISION_NUMBER;
		}
		else if (strcmp (input, "OBSTACLE") == 0) {
			collision_type = OBSTACLE_COLLISION_NUMBER;
		}
		else if (strcmp (input, "GROUND") == 0) {
			world_ground_read (ifile, world);
		}
		else if (strcmp (input, "CUSTOM") == 0) {
			world_custom_read (ifile, world, collision_type);
		}
		else if (strcmp (input, "BOX") == 0) {
			world_box_read (ifile, world, collision_type);
		}
		else if (strcmp (input, "LINE") == 0) {
			world_line_read (ifile, world, collision_type);
		}
		else if (strcmp (input, "ZONE") == 0) {
			world_zone_read (ifile, world);
		}
	}
}

/*
	Reads the zone from the file.

	Parameters:
    	*ifile = FILE to read level from
    	*world = pointer to the world_status to fill up
 */
static void
world_zone_read (FILE *ifile, world_status *world) {

	float x1, y1, x2, y2;
	fscanf(ifile, "%f%f%f%f", &x1, &y1, &x2, &y2);

	world->drawing_box_x1 = x1;
    world->drawing_box_y1 = y1;
    world->drawing_box_x2 = x2;
    world->drawing_box_y2 = y2;

	world->drawing_box = true;

}


/*
	reads a custom body from the .lvl file

	Parameters:
		*ifile = file pointer to the level
		*world = world status to be updated
		collision = type of collision
 */
static void
world_custom_read (FILE *ifile, world_status *world, cpCollisionType collision) {

	char input[10], input2[10];
	cpVect *vectors = (cpVect *)malloc(sizeof(cpVect));
	if (vectors == NULL) {
		printf("Memeory allocation error in function world_custom_read\n");
		exit(-1);
	}

	int capacity = 1;
	int size = 0;
	strcpy(input, "1");
	strcpy(input2, "1");

	while (strcmp(input, "z") != 0) {

		fscanf(ifile, "%s%s", input, input2);

		if (strcmp(input, "z") != 0) {
			size++;
			if (size == capacity) {
				capacity *= 2;
				vectors = (cpVect *)realloc(vectors, capacity * sizeof(cpVect));
				if (vectors == NULL) {
					printf("Memory allocation error: in function world_custom_read\n");
					exit(-1);
				}
			}
			int x = atoi(input);
			int y = atoi(input2);
			vectors[size-1] = cpv( x, y );
		}
	}

	char color[10];
	fscanf( ifile, "%s", color );

	float mu, mass;
	fscanf( ifile, "%f%f", &mu, &mass );

	create_user_object ( vectors, size, conv_color(color), world, collision, mu, mass );
}

/*
	reads the drawing box from the .lvl file

	Parameters:
		*ifile = file pointer to the level
		*world = world status to be updated
		collision = type of collision
 */
static void
world_box_read ( FILE *ifile, world_status *world, cpCollisionType collision ) {

	float x1, y1, width, height;
	fscanf( ifile, "%f%f%f%f", &x1, &y1, &width, &height );

	cpVect center = cpv( x1, y1 );

	char color[10];
	fscanf( ifile, "%s", color );

	float mu, mass;
	fscanf( ifile, "%f%f", &mu, &mass );

	body_information *info = (body_information *) malloc( sizeof( body_information ) );
	if (info == NULL) {
		printf("Memory allocation error: in function world_box_read\n");
		exit(-1);
	}

	info->color = conv_color ( color );
	info->body_id = world->body_count;
	world->body_count++;

	cpFloat moment = cpMomentForBox(mass, width, height);

	cpBody *body_new = cpSpaceAddBody(world->space, cpBodyNew(mass, moment));
	cpBodySetPos(body_new, center);
	cpBodySetUserData(body_new, info);

	cpVect corners[4] = { cpv(width/2, height/2), cpv(width/2, -height/2),
						cpv(-width/2, -height/2), cpv(-width/2, height/2)};

	cpShape *line = cpSpaceAddShape (world->space, cpSegmentShapeNew (body_new, corners[3], corners[0], LINE_RADIUS));
	cpShapeSetFriction(line, mu);
	cpShapeSetCollisionType (line, collision);

	for (int i = 2; i >= 0; i--) {
		cpShape *line = cpSpaceAddShape( world->space, cpSegmentShapeNew( body_new, corners[i], corners[i + 1], LINE_RADIUS ));
		cpShapeSetFriction( line, mu );
		cpShapeSetCollisionType ( line, collision );
	}
}

/*
	reads a line body from the .lvl file

	Parameters:
		*ifile = file pointer to the level
		*world = world status to be updated
		collision = type of collision
 */
static void
world_line_read ( FILE *ifile, world_status *world, cpCollisionType collision ) {

	float x1, y1, x2, y2;
	fscanf( ifile, "%f%f%f%f", &x1, &y1, &x2, &y2 );
	cpVect center = cpv((x1 + x2) / 2, (y1 + y2) / 2);

	//These two vects (a and b) are relative to center
	cpVect vectA = cpv(x1 - center.x, y1 - center.y);
	cpVect vectB = cpv(x2 - center.x, y2 - center.y);

	char color[10];
	fscanf(ifile, "%s", color);

	float mu;
	fscanf(ifile, "%f", &mu);

	float mass;
	fscanf(ifile, "%f", &mass);

	body_information *info = (body_information *)malloc(sizeof(body_information));
	if (info == NULL) {
		printf("Memory allocation error: in function world_line_read\n");
		exit(-1);
	}

	info->color = conv_color (color);
	info->body_id = world->body_count;
	world->body_count++;

	// The moment of inertia
  	cpFloat moment = cpMomentForSegment(mass, vectA, vectB);

  	// The cpSpaceAdd*() functions return the thing that you are adding.
  	// It's convenient to create and add an object in one line.
  	cpBody *body_new = cpSpaceAddBody(world->space, cpBodyNew(mass, moment));
	cpBodySetPos(body_new, center);
	cpBodySetUserData(body_new, info);

	// Now we create the collision shape for the ball.
 	// You can create multiple collision shapes that point to the same body.
	// They will all be attached to the body and move around to follow it.
	cpShape *line = cpSpaceAddShape (world->space, cpSegmentShapeNew(body_new, vectA, vectB, LINE_RADIUS));
	cpShapeSetFriction(line, mu);
	cpShapeSetCollisionType (line, collision);

	line = cpSpaceAddShape (world->space, cpSegmentShapeNew(body_new, vectB, vectA, LINE_RADIUS));
	cpShapeSetFriction(line, mu);
	cpShapeSetCollisionType (line, collision);

}

/*
  Reads the ground from the input file.

  Parameters:
      *ifile = FILE pointer to input file
      *world = pointer to world_status to insert target into world
 */
static void
world_ground_read (FILE *ifile, world_status *world) {
	
	char input[20];

	float x1, y1, x2, y2, mu;

	fscanf(ifile, "%f%f%f%f", &x1, &y1, &x2, &y2);

	fscanf(ifile, "%s", input);

	fscanf(ifile, "%f", &mu);

	body_information *info = (body_information *)malloc(sizeof(body_information));
	if (info == NULL) {
		printf("Memory allocation error: in function world_ground_read\n");
		exit(-1);
	}

	info->color = conv_color( input );
	info->body_id = world->body_count;
	world->body_count++;

	cpBodySetUserData (world->space->staticBody, info);


	cpShape *ground = cpSegmentShapeNew (world->space->staticBody,
					     cpv(x1,y1), cpv(x2,y2), LINE_RADIUS);

	cpShapeSetFriction (ground, mu);
	cpSpaceAddShape (world->space, ground);

	ground = cpSegmentShapeNew (world->space->staticBody,
					     cpv(x2,y2), cpv(x1,y1), LINE_RADIUS);

	cpShapeSetFriction (ground, mu);
	cpSpaceAddShape (world->space, ground);
}

/*
	
	updates the world

	Parameters:
		*world = world status struct pointer to be updated

	Returns:
		world update struct
 */
void
*world_update (world_status *world) {
	cpSpaceStep (world->space, world->timestep);

	return world;
}


/*
	Removes and frees the shape.

	Parameters:
    	*space = cpSpace that simulates game
    	*shape = cpShape for which this function is called
    	*unused = unusued data pointer

	Returns: nothing
 */
static void
world_shape_post(cpSpace *space, cpShape *shape, void *unused){

	cpSpaceRemoveShape(space, shape);

	if (cpShapeGetUserData(shape) != NULL) 
		free(cpShapeGetUserData(shape));
	
	cpShapeFree(shape);
}


/*
	Calls method to frees the shape safely.

	Parameters:
    	*space = cpSpace that simulates game
    	*shape = cpShape for which this function is called

	Returns: nothing
 */
static void
world_shape_free(cpShape *shape, cpSpace *space){

	cpSpaceAddPostStepCallback(space, (cpPostStepFunc)world_shape_post, shape, NULL);
}


/*
	Removes and frees the constraint.

	Parameters:
    	*space = cpSpace that simulates game
    	*shape = cpShape for which this function is called
    	*unused = unusued data pointer

	Returns: nothing
 */
static void
world_constrain_post(cpSpace *space, cpConstraint *constraint, void *unused){
	cpSpaceRemoveConstraint(space, constraint);
	cpConstraintFree(constraint);
}

/*
	Calls function to safely remove and free the constraint

	Parameters:
    	*space = cpSpace that simulates game
    	*shape = cpShape for which this function is called

	Returns: nothing
 */
static void
world_constrain_free(cpConstraint *constraint, cpSpace *space){
	cpSpaceAddPostStepCallback(space, (cpPostStepFunc)world_constrain_post,
				   constraint, NULL);
}


/*
	Removes and frees the body safely.

	Parameters:
    	*space = cpSpace that simulates game
    	*shape = cpShape for which this function is called
    	*unused = unusued data pointer

	Returns: nothing
 */
static void
world_body_post(cpSpace *space, cpBody *body, void *unused){
	cpSpaceRemoveBody(space, body);
	cpBodyFree(body);
}


/*
	Removes and frees the body safely.

	Parameters:
    	*space = cpSpace that simulates game
    	*shape = cpShape for which this function is called

	Returns: nothing
 */
static void
world_body_free(cpBody *body, cpSpace *space){
	cpSpaceAddPostStepCallback(space, (cpPostStepFunc)world_body_post, body,
				   NULL);
}


/*
	Removes and frees the world status safely.

	Parameters:
    	*world = world status to be freed

	Returns: true if free was successful
 */
bool
world_free (world_status *world ){

	//iterate over the objects and shapes in the space
	world_free_space (world->space);

	free (world);

	return true;
}

/*
	Removes and frees the space safely.

	Parameters: cpSpace pointer

	Returns: true if free was successful
 */
bool
world_free_space (cpSpace *space) {
	cpSpaceEachShape(space, (cpSpaceShapeIteratorFunc)
			 world_shape_free, space);
	cpSpaceEachConstraint(space, (cpSpaceConstraintIteratorFunc)
			      world_constrain_free, space);
	cpSpaceEachBody(space, (cpSpaceBodyIteratorFunc)
			world_body_free, space);

	cpSpaceFree(space);

	return true;
}

/*
	Removes and frees the body safely.

	Parameters:
    	*space = cpSpace that simulates game
    	*shape = cpShape for which this function is called
    	*unused = unusued data pointer

	Returns: nothing
 */
/*
void
world_add_object(world_status *world, cpVect vector, cpFloat width, cpFloat
		 height, COLOR color, cpCollisionType collision) {

	// the default mass
	cpFloat mass = 1;

	// The moment of inertia
  	cpFloat moment = cpMomentForBox(mass, width, height);

  	// The cpSpaceAdd*() functions return the thing that you are adding.
  	// It's convenient to create and add an object in one line.
  	cpBody *body_new = cpSpaceAddBody(world->space, cpBodyNew(mass, moment));
	cpBodySetPos(body_new, vector);

	COLOR *color_check = (COLOR *)malloc(sizeof(COLOR));
	
	*color_check = color;


	// Now we create the collision shape for the ball.
 	// You can create multiple collision shapes that point to the same body.
	// They will all be attached to the body and move around to follow it.
	cpShape *boxShape = cpSpaceAddShape(world->space, cpBoxShapeNew(body_new,
									width,
									height));
	//printf("width: %d, height: %d: \n", width, height);
	cpShapeSetFriction(boxShape, 1);
	cpShapeSetCollisionType (boxShape, collision);
	cpShapeSetUserData(boxShape, color_check);
}
*/


/*-----------------------------------------------------------------
* Testing functions
*

static void
world_object_read (FILE *ifile, world_status *world) {

	char input[50];

	float x1, y1, width, height, mu, mass;
	fscanf(ifile, "%f%f%f%f%f", &x1, &y1, &width, &height, &mu);

	bool is_static;
	fscanf(ifile, "%s", input);
	is_static = (strcmp("y", input) == 0) ? true : false;

	fscanf(ifile, "%s", input);
	COLOR *enum_color = (COLOR *)malloc(sizeof(COLOR));
	*enum_color = conv_color(input);

	world_add_object(world, cpv(x1, y1), width, height, *enum_color, OBSTACLE_COLLISION_NUMBER);

	//printf("added object\n");

}

static void
world_new_check () {

	world_status *world = world_new (1, 10);

	printf("x: %d, y: %d\n", world->drawing_box_x, world->drawing_box_y);
	printf("height: %d, width: %d\n", world->drawing_box_height, world->drawing_box_width);

}

static void
world_hello_chipmunk_test () {
	int level = 1;
	cpFloat time_step = 1.0 / 60.0;
	world_status *world = world_new (level, time_step);

	for (cpFloat time = 0; time < 2; time += time_step) {
		//printf("In loop: %f\n", time);
		cpSpaceEachBody (world->space, (cpSpaceBodyIteratorFunc)world_print_every_body, NULL);
		cpSpaceStep (world->space, time_step);
	}

	world_free(world);
}

static void
world_print_every_body (cpBody *body, cpSpace *space) {


	cpVect pos = cpBodyGetPos (body);
	cpVect vel = cpBodyGetVel (body);

	printf("ballBody is at (%5.2f, %5.2f). It's velocity is (%5.2f, %5.2f)\n", pos.x, pos.y, vel.x, vel.y);
}

static void
world_print_every_shape (cpShape *shape, cpSpace *space) {

	printf("This exists\n");

}

*/

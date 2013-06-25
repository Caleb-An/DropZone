#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <gtk/gtk.h>
#include <chipmunk/chipmunk.h>
#include <assert.h>
#include "specs/common.h"
#include "specs/physics.h"
#include "specs/protocols.h"


/*

-- TESTING LOCATED HERE --

int main(int argc, char *argv[]) {

	//test protocol_new_body
	world_status *world = world_new( 1 , 0.1 );

	cpBody *ground = world_get_ground( world->space );

	polygon_struct *polygon = polygon_from_body( ground );

	char *result;

	result = protocol_new_body ( polygon->color, (cpVect *) polygon->vectors->data, polygon->vector_count, polygon->body_id );

	//printf("%s\n", result);
	//test protocol_extract_body

	polygon = protocol_extract_body( result );

	result = protocol_new_body ( polygon->color, (cpVect *) polygon->vectors->data, polygon->vector_count, polygon->body_id );

	//printf("%s\n", result);

	//protocol_send_coords

	result = protocol_send_coords( world->space );
	printf("%s\n", result);
	//protocol_extract_coords
	coord_update *update = protocol_extract_coords( result );

	printf("----------\n");
	printf("%f\n", update->angle_array[0]);
	printf("(%f,%f)\n", update->vector_array[1].x, update->vector_array[1].y);

	char *message = "hello world";
	char *coded_message = protocol_send_chat(message);
	char *decoded_message = protocol_decode_chat(coded_message);

	printf("coded_message: %s\n", coded_message);
	printf("decoded_message: %s\n", decoded_message);

	float *array = malloc(sizeof(int) * 6);

	free( array );


	result = protocol_send_zone(1.01, 2.02, 3.03, 4.04);
	printf("sending drawing zone: %s\n", result);
	drawing_zone *test = protocol_decode_zone(result);
	printf("Coords in drawing_zone struct: %f, %f, %f, %f\n", test->x1, test->y1, test->x2, test->x2);


	result = protocol_send_chat("Chat testing\n");
	printf("result: %s\n", result);

	printf("decoded: %s", protocol_decode_chat(result));


	result = protocol_send_level(13);
	printf("result: %s\n", result);
	printf("level: %i\n", protocol_decode_level(result));

	world_free( world );

	return(0);
}
*/

/*
	checks if the string has a full message
	
	parameters: string to check

	returns: BOGUS if unuseable partial, NOT_DONE
	if useable partial, and DONE if useable full
 */
int
protocol_recv_full_message(char *message) {
   
	//null checking
	if(message == NULL)
		return BOGUS;

    bool found_beginning = false;
    int i = 0;

 	// Check beginning characters
    for(; !found_beginning && i < MAXLINE; i++) {
	
		if(message[i] == BEGIN_CHARACTER[0]) {
	    
			if(message[i + 1] == BEGIN_CHARACTER[1] && message[i + 2] == BEGIN_CHARACTER[2])
				found_beginning = true;
		}
    }

    int new_pointer_index = i;

    if(found_beginning == false)
		return BOGUS;
    
	else
		new_pointer_index += i;

    // Check for ending characters
    for(; i < MAXLINE; i++)
		if(message[i] == END_CHARACTER[0]) {

	    	for(int j = i; j <= i + ID_INDEX - 2; j++)
				if(message[j] != END_CHARACTER[j - i])
		    		return NOT_DONE;
	    	
			return DONE;
		}

    message = message + new_pointer_index;

    return NOT_DONE;
}

/*
	returns the length of the message

	parameters: string message

	returns: integer length, with 0
	if the string is invalid
*/
int 
protocol_message_length(char *message) {

	//null checking
    if(message == NULL)
		return 0;

    for(int i = 0; i < MAXLINE; i++)
		if(message[i] == END_CHARACTER[0])
	    	return i + ID_INDEX - 1;

    return 0;
}

/*
	converts a message into a server
	acceptable string

	parameters: string message

	returns: converted string
*/
char *
protocol_send_chat (char *message) {

    if( message == NULL )
		return ";?";

    char *result = calloc  (MAXLINE, sizeof(char));
    
	assert(result);

    sprintf(result, "%s;%i;%s;%s" , BEGIN_CHARACTER, CHAT_MESSAGE, message, END_CHARACTER);

    return result;
}

/*
	interprets a message from a protocol wrapped string
	sister function to protocol_send_chat

	parameters: string from protocol_send_chat

	returns: message
*/
char *
protocol_decode_chat(char *message) {

    if( message == NULL )
		return "";

    char *result = calloc (MAXLINE, sizeof(char));
    assert(result);
    strtok(message, ";"); // Take away beginning 5 characters
    strtok(NULL, ";"); // Take away message ID
    char *updated_message = strtok(NULL, ";"); //

    return updated_message;
}

/*
	converts level information into a string

	parameters: integer level, integer try number

	returns: string with the info
 */
char *
protocol_send_level(int level, int try_number) {

    char *result = (char *) calloc (MAXLINE, sizeof(char));
    
	//checking if invalid	
	if ( level <= 0 || try_number <= 0) {
		level = 1;
		try_number = 1;
    }

    sprintf(result, "%s;%i;%i,%i;%s", BEGIN_CHARACTER, LEVEL_MESSAGE, level, try_number, END_CHARACTER);
    assert(result);

    return result;
}

/*
	converts the string into a level info struct
	sister function to protocol_send_level

	parameters: string message

	returns: level info struct
 */
level_info *
protocol_decode_level(char *message) {

    strtok(message, ";"); // Take away beginning characters
    strtok(NULL, ";"); // Take away ID
    level_info *level_switch_info = (level_info *) malloc(sizeof(level_info));
    sscanf(strtok(NULL, ";"), "%i,%i", &(level_switch_info -> level), &(level_switch_info -> try_number));

    return level_switch_info;
}

/*
	creates a string describing the draw zone

	parameters: 4 floats for the (x,y) of the bottom left and top right corners

	returns: string with the message
 */
char *
protocol_send_zone(float x1, float y1, float x2, float y2) {

    char *result = (char *) calloc(MAXLINE, sizeof(char));

    sprintf( result, "%s;%i;%3.3f,%3.3f,%3.3f,%3.3f;%s" , BEGIN_CHARACTER, ZONE_MESSAGE, x1, y1, x2, y2, END_CHARACTER);

    return result;
}

/*
	decodes the string into a drawing zone struct
	sister function of protocol_send_zone

	parameters: string 

	returns: drawing zone struct 
 */
drawing_zone *
protocol_decode_zone(char *string) {

    if ( strcmp(string, "error") == 0 ) {
	return NULL;
    }

    drawing_zone *zone = (drawing_zone *) malloc(sizeof(drawing_zone));

    float x1, y1, x2, y2;
    char *result;

    strtok(string, ";"); // Take care of beginning characters
    strtok(NULL, ";"); // Take care of id
    result = strtok(NULL, ";");
    sscanf(result, "%f,%f,%f,%f", &x1, &y1, &x2, &y2);
    zone->x1 = x1;
    zone->y1 = y1;
    zone->x2 = x2;
    zone->y2 = y2;
    return zone;
}


/*
  	tells all the clients about a new body.

 	Parameters: color, array of the coordinates in [x1,y1,x2,y2,x3,y3] etc. form,
 	number of numbers passed, id

	returns: string of form "id;color;num_count;x1,y1;x2,y2;" etc
*/
char *
protocol_new_body ( char *color, cpVect *array, int vector_count, int id , float mass){

    char *result = (char *) calloc(MAXLINE, sizeof(char));
    int i = 0;
    char buffer[MAX_COORDINATE_SIZE];

    sprintf(buffer, "%s;%i;%i;%s;%3.3f;%i;", BEGIN_CHARACTER, NEW_BODY_MESSAGE, id, color, mass, vector_count);

    strcpy (result, buffer);  //can strcpy because first element in resultprint

    while ( i < vector_count ) {

		sprintf( buffer, "%3.3f,%3.3f;" , array[i].x, array[i].y );
		strcat( result, buffer );

		i++;
    }

    strcat (result, END_CHARACTER );

    return result;
}

/*
	will append "id,angle,x,y;" for a body to string
	used in loop by cpSpaceEachBody

	parameters: body, string

	returns: nothing
*/
void body_iterator (cpBody *body, char *string){

	body_information *info = cpBodyGetUserData( body );
	char buffer[MAX_COORDINATE_SIZE];

	sprintf( buffer, "%i," , info->body_id );
	strcat( string, buffer );

	cpFloat cp_angle = cpBodyGetAngle( body );
	cpVect cp_vector = cpBodyGetPos( body );
	cpFloat cp_x = cp_vector.x;
	cpFloat cp_y = cp_vector.y;


	sprintf( buffer, "%f,%f,%f;" , (float) cp_angle , (float) cp_x , (float) cp_y );
	strcat( string, buffer );

}

/*
	returns the string for the server telling the clients about all the
	body position updates. format: "message type;id1,angle1,x1,y1;id2,angle2,x2,y2;" etc

	Parameters: cpSpace space

	returns: string with info
*/
char *protocol_send_coords( cpSpace *space ){

    char *string = (char * ) calloc(MAXLINE, sizeof(char));
	char buffer[MAX_COORDINATE_SIZE];
	
	sprintf(buffer, "%s;%i;", BEGIN_CHARACTER, UPDATE_POSITIONS_MESSAGE );
	strcpy(string, buffer);

	cpSpaceEachBody(space, (cpSpaceBodyIteratorFunc) body_iterator, string );

	strcat(string, END_CHARACTER);

	return string;
}

/*
	frees the polygon struct
	parameter: polygon to be freed
*/
void polygon_destroy ( polygon_struct *polygon ){

	g_array_free ( polygon->vectors, TRUE );
	free ( polygon );

}

/*
	turns the string received from a client into a polygon struct

	parameters: string from client

	returns: polygon struct
*/
polygon_struct *protocol_extract_body( char *string ){

	if ( strcmp(string, "error") == 0 ){

		printf("invalid string received\n");
		return NULL;
	}

	polygon_struct *polygon = polygon_new();

	float num1, num2;

	char *result;

	strtok(string, ";");
	strtok(NULL, ";");

	polygon->body_id = atoi( strtok(NULL, ";"));
	polygon->color = strtok( NULL, ";" );
	polygon->mass = atof( strtok( NULL, ";" ));
	polygon->vector_count = atoi( strtok( NULL, ";" ));
	result = strtok( NULL, ";" );

	while( result[0] != '?' ) {

	    sscanf(result, "%f,%f", &num1, &num2 );
	    cpFloat float1, float2;
	    float1 = num1;
	    float2 = num2;
	    cpVect vector = cpv( float1, float2 );

	    g_array_append_val( polygon->vectors, vector );
	    result = strtok( NULL, ";" );
	}

	return polygon;

}

/* 
	extracts the coordinate updates from the string received from the server.

	Parameters: string from server of format "message type;id1,angle1,x1,y1;id2,angle2,x2,y2;"

	returns: struct with 2 arrays: 1 of vectors and 1 of angles
 */
coord_update *protocol_extract_coords( char *string ){
    
	char string2[strlen(string)];

    strcpy(string2, string);

	int id, max_id;
	float angle, x, y;
	char *result;
	id = 0;
	max_id = 1;
	
	strtok(string, ";"); // Take away beginning characters
	strtok(NULL, ";");  // skip message type
	result = strtok(NULL, ";"); // get the next one

	/*loop over once to find max_id*/
	while( result[0] != '?' ) {

	    sscanf(result, "%i,%f,%f,%f" , &id , &angle , &x , &y );
	    
		if (id > max_id ) 
			max_id = id;
	    
		result = strtok( NULL, ";" );
	}

	/*malloc based off max_id, initialize the angles -3000*/
	cpVect *vector_array = (cpVect * ) malloc ( sizeof( cpVect ) * (max_id + 1) );
	float *angle_array = (float * ) malloc ( sizeof( float ) * (max_id + 1) );

	for (int i = 0; i <= max_id ; i++){
		angle_array[i] = -3000.0;
	}

	strtok( string2, ";" ); // skip message type

	result = strtok( NULL, ";" ); // get the next one

	while( result[0] != '?' ) {

		sscanf(result, "%i,%f,%f,%f" , &id , &angle , &x , &y );

		if (id <= max_id) {

			cpVect vector = cpv( x , y );
			vector_array[id] = vector;
			angle_array[id] = angle;
		}

		result = strtok( NULL, ";" );
	}

	coord_update *package = (coord_update *) malloc( sizeof( coord_update ));
	package->angle_array = angle_array;
	package->vector_array = vector_array;

	return package;
}

/*
	updates the polygon struct with the shape's corners
	used as an iterator in polygon_from_body()

	parameters: cpBody pointer, cpShape pointer, polygon struct pointer
	
	returns: nothing
 */
static void 
body_fill_array (cpBody *body, cpShape *shape, polygon_struct *polygon) {

	cpVect point = cpSegmentShapeGetA (shape);

	polygon->vectors = g_array_append_val (polygon->vectors, point);
}

/*
	gets a polygon struct from a cpBody

	parameters: cpBody pointer

	returns: polygon struct pointer
 */
polygon_struct *
polygon_from_body (cpBody *body) {

	polygon_struct *polygon = polygon_new();

	body_information *info = cpBodyGetUserData(body);

	polygon->color = color_to_string(info->color);
	polygon->body_id = info->body_id;
	polygon->mass = 1; //This is only used on the server and client doesn't
	//care about the mass

	cpBodyEachShape (body, (cpBodyShapeIteratorFunc) body_fill_array, polygon);

	polygon->vector_count = polygon->vectors->len;

	return polygon;
}


/* 
	polygon struct initializer 

	parameters: nothing

	returns: polygon_struct pointer
 */
polygon_struct *polygon_new(){

    polygon_struct *polygon = (polygon_struct *) malloc ( sizeof ( polygon_struct ) );
	polygon->color = (char *)malloc(20 * sizeof(char));
	polygon->color = "yellow";
	polygon->mass = 1;
	polygon->vectors = g_array_new ( FALSE, FALSE, sizeof( cpVect ) );
	polygon->vector_count = 0;
	polygon->body_id = 0;

	return polygon;
}

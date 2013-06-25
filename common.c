#include <string.h>
#include "specs/common.h"

/*
	converts a string into a COLOR enum.

	parameters: string with a color written

	returns: the color enum, with RED as default
	if it is an invalid string
*/
COLOR conv_color ( const char *color ){

	if (strcmp("red", color ) == 0 ) return RED;
	else if (strcmp("green", color) == 0 ) return GREEN;
	else if (strcmp("yellow", color) == 0) return YELLOW;
	else if (strcmp("blue", color) == 0) return BLUE;
	else return RED;
}

/*
	converts a COLOR enum into a string.
	
	parameters: a COLOR enum

	returns: the string of the color, with
	"none" returned if the COLOR is invalid
*/
char *color_to_string (COLOR color) {

	if (color == RED) return "red";
	else if (color == BLUE) return "blue";
	else if (color == GREEN) return "green";
	else if (color == YELLOW) return "yellow";
	else return "none";
}

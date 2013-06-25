#ifndef common_h
#define common_h

/*
	COLOR enumeration
	
	Enum for colors; red = 0, yellow = 1, blue = 2, green = 3
*/
typedef enum {

	RED = 0,
	YELLOW,
    BLUE,
	GREEN

} COLOR;

/* 
	body_information struct 

	contains the body id and the color
*/
typedef struct {

	COLOR color;
	int body_id;

} body_information;


/*
	converts a string into a COLOR enum.

	parameters: string with a color written

	returns: the color enum, with RED as default
	if it is an invalid string
*/
COLOR conv_color ( const char *color );

/*
	converts a COLOR enum into a string.
	
	parameters: a COLOR enum

	returns: the string of the color, with
	"none" returned if the COLOR is invalid
*/
char *color_to_string (COLOR color);

#endif

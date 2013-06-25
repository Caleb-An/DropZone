#ifndef PROTOCOLS
#define PROTOCOLS
#define MAX_COLOR_SIZE 7
#define MAX_COORDINATE_SIZE 50
#define MAXLINE 400

#define NEW_BODY_MESSAGE 1
#define UPDATE_POSITIONS_MESSAGE 2
#define CHAT_MESSAGE 3
#define ZONE_MESSAGE 4
#define LEVEL_MESSAGE 5
#define BEGIN_CHARACTER "~!@"
#define ID_INDEX 4
#define END_CHARACTER "?`."

#define DONE 0
#define NOT_DONE 1
#define BOGUS 2


typedef struct {
    int body_id;
    char *color;
    float mass;
    GArray *vectors;
    int vector_count;

} polygon_struct;

typedef struct {
	float *angle_array;
	cpVect *vector_array;
} coord_update;

typedef struct {
    float x1;
    float y1;
    float x2;
    float y2;
} drawing_zone;

typedef struct {
    int level;
    int try_number;
} level_info;


polygon_struct *polygon_new ();

polygon_struct *polygon_from_body (cpBody *body);

int protocol_recv_full_message(char *message);

int protocol_message_length(char *message);

char *protocol_send_chat (char *message);

char *protocol_decode_chat(char *message);

char *protocol_send_level(int level, int try_number);

level_info *protocol_decode_level(char *message);

char *protocol_send_zone(float x1, float y1, float x2, float y2);

drawing_zone *protocol_decode_zone(char *string);


/* server side functions */


char *protocol_new_body ( char *color, cpVect *array, int vector_count, int id, float mass );

char *protocol_send_coords( cpSpace *space );

polygon_struct *protocol_extract_body( char *string );

void polygon_destroy ( polygon_struct *polygon );

//static void body_fill_array (cpBody *body, cpShape *shape, polygon_struct *polygon);

/*client side functions */

char *ctos_convert( char *color, float *array, int int_count );

coord_update *protocol_extract_coords( char *string );

#endif

COMPILERFLAGS = -Wall -std="c99" -g
CC = gcc
GTKFLAGS = `pkg-config --libs --cflags gtk+-3.0`

INCLUDE_DIR = ./include

#LIBPATH =
#LIBPATH = /opt/local/lib



CFLAGS = $(COMPILERFLAGS) -I$(INCLUDE_DIR)

#LIBRARIES = libchipmunk.a
LIBRARIES += -lchipmunk -lm

OBJS = networking.o protocols.o graphics.o physics.o common.o
BINS = server client gui

all: $(BINS)

gui: gui.c physics.c graphics.c common.c
	$(CC) $(CFLAGS) -o gui gui.c physics.c graphics.c common.c $(LIBRARIES) $(GTKFLAGS)

server: physics common specs/common.h protocols graphics networking
	$(CC) $(CFLAGS) -o server server.c $(OBJS) $(LIBRARIES) $(GTKFLAGS)

client: common graphics protocols client.c networking
	$(CC) $(CFLAGS) -o client client.c $(OBJS) $(LIBRARIES) $(GTKFLAGS)

protocols: protocols.c specs/protocols.h common
	$(CC) $(CFLAGS) -c -o protocols.o protocols.c $(GTKFLAGS)

graphics: graphics.c specs/graphics.h common
	$(CC) $(CFLAGS) -c -o graphics.o graphics.c $(GTKFLAGS)

physics: physics.c specs/physics.h common
	$(CC) $(CFLAGS) -c -o physics.o physics.c

common: common.c
	$(CC) $(CFLAGS) -c -o common.o common.c

networking: networking.c specs/networking.h protocols
	$(CC) $(CFLAGS) -c -o networking.o networking.c $(GTKFLAGS)

clean:
	rm -f $(BINS) $(OBJS)

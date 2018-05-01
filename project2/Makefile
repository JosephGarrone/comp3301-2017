#Libraries to be added
LIBS= -lpthread -lX11

#Compiler Option flags
CFLAGS=-g -Wall -std=gnu99
CC=h5cc

OBJECTS= $(SOURCES:%.c=%.o)

common-srcs := $(wildcard common/*.c)
common-objects := $(patsubst %.c,%.o,$(wildcard common/*.c))

all: user-shell cag display player recorder

user-shell-srcs := $(wildcard user-shell/*.c)
user-shell-objects := $(patsubst %.c,%.o,$(wildcard user-shell/*.c))
user-shell: user-shell-src common-src $(user-shell-objects) $(common-objects)
	$(CC) $(CFLAGS) $(user-shell-objects) $(common-objects) -o bin/user-shell $(LIBS)

user-shell-src: $(user-shell-srcs)
	$(CC) $(CFLAGS) -c $(user-shell-srcs)

cag-srcs := $(wildcard cag/*.c)
cag-objects := $(patsubst %.c,%.o,$(wildcard cag/*.c))
cag: cag-src common-src $(cag-objects) $(common-objects)
	$(CC) $(CFLAGS) $(cag-objects) $(common-objects) -o bin/cag $(LIBS)

cag-src:
	$(CC) $(CFLAGS) -c $(cag-srcs) $(LIBS)

display-srcs := $(wildcard display/*.c)
display-objects := $(patsubst %.c,%.o,$(wildcard display/*.c))
display: display-src common-src $(display-objects) $(common-objects)
	$(CC) $(CFLAGS) $(display-objects) $(common-objects) -o bin/display $(LIBS)

display-src:
	$(CC) $(CFLAGS) -c $(display-srcs) $(LIBS)

recorder-srcs := $(wildcard recorder/*.c)
recorder-objects := $(patsubst %.c,%.o,$(wildcard recorder/*.c))
recorder: recorder-src common-src $(recorder-objects) $(common-objects)
	$(CC) $(CFLAGS) $(recorder-objects) $(common-objects) -o bin/recorder $(LIBS)

recorder-src:
	$(CC) $(CFLAGS) -c $(recorder-srcs) $(LIBS)

player-srcs := $(wildcard player/*.c)
player-objects := $(patsubst %.c,%.o,$(wildcard player/*.c))
player: player-src common-src $(player-objects) $(common-objects)
	$(CC) $(CFLAGS) $(player-objects) $(common-objects) -o bin/player $(LIBS)

player-src:
	$(CC) $(CFLAGS) -c $(player-srcs) $(LIBS)

common-src: $(common-srcs)
	$(CC) $(CFLAGS) -c $(common-srcs) $(LIBS)

clean:
	$(RM) user-shell/*.o
	$(RM) user-shell/user-shell
	$(RM) cag/*.o
	$(RM) cag/cag
	$(RM) display/*.o
	$(RM) display/display
	$(RM) recorder/*.o
	$(RM) recorder/recorder
	$(RM) player/*.o
	$(RM) player/player
	$(RM) common/*.o
	$(RM) bin/cag bin/user-shell bin/display bin/player bin/recorder
	$(RM) *.o

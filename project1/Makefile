#Libraries to be added
LIBS= -lpthread -lX11

#Compiler Option flags
CFLAGS=-g -Wall -std=gnu99
CC=gcc

OBJECTS= $(SOURCES:%.c=%.o)

common-srcs := $(wildcard common/*.c)
common-objects := $(patsubst %.c,%.o,$(wildcard common/*.c))

all: user-shell cag display

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
	$(CC) $(CFLAGS) -c $(cag-srcs) $(LIBS) $(LIBS)

display-srcs := $(wildcard display/*.c)
display-objects := $(patsubst %.c,%.o,$(wildcard display/*.c))
display: display-src common-src $(display-objects) $(common-objects)
	$(CC) $(CFLAGS) $(display-objects) $(common-objects) -o bin/display $(LIBS)

display-src:
	$(CC) $(CFLAGS) -c $(display-srcs) $(LIBS)

common-src: $(common-srcs)
	$(CC) $(CFLAGS) -c $(common-srcs) $(LIBS)

clean:
	$(RM) user-shell/*.o
	$(RM) user-shell/user-shell
	$(RM) cag/*.o
	$(RM) cag/cag
	$(RM) display/*.o
	$(RM) display/display
	$(RM) common/*.o
	$(RM) bin/*

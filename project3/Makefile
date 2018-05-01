#Libraries to be added
LIBS= -lpthread -Wl,-rpath-link,/mnt/e/Git/comp3301/project3/libs -L/mnt/e/Git/comp3301/project3/libs -lmemcached
LIBS2 = -lX11
LIBS3 = -l:libv4l2.so -l:libdl.so

#Compiler Option flags
CFLAGS=-g -Wall -std=gnu99
CC=arm-linux-gnueabihf-gcc
#CC=gcc

OBJECTS= $(SOURCES:%.c=%.o)

common-srcs := $(wildcard common/*.c)
common-objects := $(patsubst %.c,%.o,$(wildcard common/*.c))

all: wcd-shell wcd-control wcd-display wcd-remote wcd-mem

wcd-shell-srcs := $(wildcard wcd-shell/*.c)
wcd-shell-objects := $(patsubst %.c,%.o,$(wildcard wcd-shell/*.c))
wcd-shell: wcd-shell-src common-src $(wcd-shell-objects) $(common-objects)
	$(CC) $(CFLAGS) $(wcd-shell-objects) $(common-objects) -o bin/wcd-shell $(LIBS)

wcd-shell-src: $(wcd-shell-srcs)
	$(CC) $(CFLAGS) -c $(wcd-shell-srcs)

wcd-control-srcs := $(wildcard wcd-control/*.c)
wcd-control-objects := $(patsubst %.c,%.o,$(wildcard wcd-control/*.c))
wcd-control: wcd-control-src common-src $(wcd-control-objects) $(common-objects)
	$(CC) $(CFLAGS) $(wcd-control-objects) $(common-objects) -o bin/wcd-control $(LIBS) $(LIBS3)

wcd-control-src:
	$(CC) $(CFLAGS) -c $(wcd-control-srcs) $(LIBS)

wcd-display-srcs := $(wildcard wcd-display/*.c)
wcd-display-objects := $(patsubst %.c,%.o,$(wildcard wcd-display/*.c))
wcd-display: wcd-display-src common-src $(wcd-display-objects) $(common-objects)
	$(CC) $(CFLAGS) $(wcd-display-objects) $(common-objects) -o bin/wcd-display $(LIBS) $(LIBS2)

wcd-display-src:
	$(CC) $(CFLAGS) -c $(wcd-display-srcs) $(LIBS)

wcd-remote-srcs := $(wildcard wcd-remote/*.c)
wcd-remote-objects := $(patsubst %.c,%.o,$(wildcard wcd-remote/*.c))
wcd-remote: wcd-remote-src common-src $(wcd-remote-objects) $(common-objects)
	$(CC) $(CFLAGS) $(wcd-remote-objects) $(common-objects) -o bin/wcd-remote $(LIBS)

wcd-remote-src:
	$(CC) $(CFLAGS) -c $(wcd-remote-srcs) $(LIBS)
	wcd-remote-srcs := $(wildcard wcd-remote/*.c)
wcd-remote-objects := $(patsubst %.c,%.o,$(wildcard wcd-remote/*.c))
wcd-remote: wcd-remote-src common-src $(wcd-remote-objects) $(common-objects)
	$(CC) $(CFLAGS) $(wcd-remote-objects) $(common-objects) -o bin/wcd-remote $(LIBS)

wcd-remote-src:
	$(CC) $(CFLAGS) -c $(wcd-remote-srcs) $(LIBS)

wcd-mem-srcs := $(wildcard wcd-mem/*.c)
wcd-mem-objects := $(patsubst %.c,%.o,$(wildcard wcd-mem/*.c))
wcd-mem: wcd-mem-src common-src $(wcd-mem-objects) $(common-objects)
	$(CC) $(CFLAGS) $(wcd-mem-objects) $(common-objects) -o bin/wcd-mem $(LIBS)

wcd-mem-src:
	$(CC) $(CFLAGS) -c $(wcd-mem-srcs) $(LIBS)
	wcd-mem-srcs := $(wildcard wcd-mem/*.c)
wcd-mem-objects := $(patsubst %.c,%.o,$(wildcard wcd-mem/*.c))
wcd-mem: wcd-mem-src common-src $(wcd-mem-objects) $(common-objects)
	$(CC) $(CFLAGS) $(wcd-mem-objects) $(common-objects) -o bin/wcd-mem $(LIBS)

wcd-mem-src:
	$(CC) $(CFLAGS) -c $(wcd-mem-srcs) $(LIBS)

common-src: $(common-srcs)
	$(CC) $(CFLAGS) -c $(common-srcs) $(LIBS)

clean:
	$(RM) wcd-shell/*.o
	$(RM) wcd-shell/wcd-shell
	$(RM) wcd-control/*.o
	$(RM) wcd-control/wcd-control
	$(RM) wcd-display/*.o
	$(RM) wcd-display/wcd-display
	$(RM) wcd-remote/*.o
	$(RM) wcd-remote/wcd-remote
	$(RM) wcd-mem/*.o
	$(RM) wcd-mem/wcd-mem
	$(RM) common/*.o
	$(RM) bin/wcd-control bin/wcd-shell bin/wcd-display bin/wcd-remote bin/wd-remote
	$(RM) *.o

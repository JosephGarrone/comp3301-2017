#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <linux/videodev2.h>
#include <libv4l2.h>
#include <libmemcached/memcached.h>

#include "../common/s4354198_structs.h"
#include "../common/s4354198_defines.h"
#include "../common/s4354198_utils.h"
#include "../common/s4354198_externs.h"
#include "../common/s4354198_memcached.h"

/* Function prototypes */
void sigint_handler(int signal);
int start_display(int x, int y, int width, int height);
void start_avconv(int display, int width, int height, char* options, char* delay);
void* stream_video(void*);
void handle_wcd(char* input);
void handle_list();
void handle_sel(char* input);
void handle_img(char* input);
void handle_kill(char* input);
void handle_clear(char* input);
int num_displays();
int* next_display();
int display_get_attr(int display, int attr);
bool valid_display(int display);
static void xioctl(int fh, int request, void *arg);

/* Globals */
// Contains application data
Application* app;
// Streaming thread
pthread_t streamer;
sem_t canWrite;

int main(int argc, char** argv) {
    app = (Application*) malloc(sizeof(Application));
    app->runtimeInfo = (RuntimeInfo*) malloc(sizeof(RuntimeInfo));
    app->comms = (Comms*) malloc(sizeof(Comms));
    app->displays = (int*) malloc(sizeof(int) * MAX_DISPLAYS);

    sem_init(&canWrite, 0, 1);

    for (int i = 0; i < MAX_DISPLAYS; i++) {
        app->displays[i] = -1;
        app->runtimeInfo->avconv[i] = -1;
        app->runtimeInfo->display[i] = -1;
    }
    
    signal(SIGINT, sigint_handler);
    
    app->memc = s4354198_connect_memcached_server();

    pthread_create(&streamer, NULL, &stream_video, NULL);

    size_t size;
    char* input = NULL;
    char* token = NULL;
    while (1) {
        getline(&input, &size, stdin);

        input[strlen(input) - 1] = '\0';

        token = strsep(&input, " ");

        if (s4354198_str_match(token, CMD_WCD)) {
            handle_wcd(input);
        } else if (s4354198_str_match(token, CMD_LIST)) {
            handle_list();
        } else if (s4354198_str_match(token, CMD_SEL)) {
            handle_sel(input);
        } else if (s4354198_str_match(token, CMD_IMG)) {
            handle_img(input);
        } else if (s4354198_str_match(token, CMD_KILL)) {
            handle_kill(input);
        } else if (s4354198_str_match(token, CMD_CLEAR)) {
            handle_clear(input);
        }
    }
    
    free(input);

    return 0;
}

/**
 * Handles the WCD command
 */
void handle_wcd(char* input) {
    int x, y, width, height;

    if (sscanf(input, "%d %d %d %d", &x, &y, &width, &height) != 4) {
        s4354198_o("Invalid commands for wcd. See help.\n");
    } else {
        s4354198_o("Processing wcd command\n");
        int display = start_display(x, y, width, height);
        start_avconv(display, width, height, "", NULL);
    }
}

/**
 * Handles selecting of a display
 */
void handle_sel(char* input) {
    if (strlen(input) > 0) {
        for (int i = 0; i < strlen(input); i++) {
            if (!isdigit(input[i])) {
                s4354198_o("Invalid display specified\n");
                return;
            }
        }
        
        int display = atoi(input);

        if (valid_display(display)) {
            s4354198_mem_set(MEM_SEL_DISP, input);
            s4354198_o("Selected display #%d\n", display);
        } else {
            s4354198_o("Invalid display specified\n");
        }
    } else {
        s4354198_o("Invalid display specified\n");
    }
}

/**
 * Handle the killing of a display and its avconv process
 */
void handle_kill(char* input) {
    int status; 

    if (strlen(input) > 0) {
        for (int i = 0; i < strlen(input); i++) {
            if (!isdigit(input[i])) {
                s4354198_o("Invalid display specified\n");
                return;
            }
        }
        
        int display = atoi(input);

        sem_wait(&canWrite);

        // Kill avconv
        int avconvPid = app->runtimeInfo->avconv[display];
        s4354198_o("Killing %d\n", avconvPid);
        app->runtimeInfo->avconv[display] = -1;
        if (avconvPid >= 0) {
            kill(avconvPid, SIGINT);
            sleep(2);
            waitpid(avconvPid, &status, WNOHANG);
            kill(avconvPid, SIGKILL);
            waitpid(avconvPid, &status, 0);
        }

        // Kill display
        int displayPid = app->runtimeInfo->display[display];
        kill(displayPid, SIGINT);
        waitpid(displayPid, &status, 0);

        // Remove the id
        app->displays[display] = -1;

        // Remove display from memcached
        char displays[MAX_DISPLAYS * 40];
        int index = 0;
        if (num_displays() > 0) {
            int count = 0;
            for (int i = 0; i < MAX_DISPLAYS; i++) {
                int curDisplay = app->displays[i];
    
                if (curDisplay >= 0) {
                    int x = display_get_attr(curDisplay, 0);
                    int y = display_get_attr(curDisplay, 1);
                    int width = display_get_attr(curDisplay, 2);
                    int height = display_get_attr(curDisplay, 3);

                    if (count > 0) {
                        index += sprintf(&(displays[index]), ",%d:%d:%d:%d:%d",
                            curDisplay,
                            x,
                            y,
                            width,
                            height
                        );
                    } else {
                        index += sprintf(&(displays[index]), "%d:%d:%d:%d:%d",
                            curDisplay,
                            x,
                            y,
                            width,
                            height
                        );
                    }
                    count++;
                }
            }
        } else {
            sprintf(displays, " ");
        }
        s4354198_mem_set(MEM_DISPLAYS, displays);

        // Clear the selected display
        char* selDisplay = s4354198_mem_get(MEM_SEL_DISP);
        int selDisplayN = atoi(selDisplay);
        if (selDisplayN == display) {
            s4354198_mem_set(MEM_SEL_DISP, "-");
        }
        free(selDisplay);

        sem_post(&canWrite);

        s4354198_o("Killed display #%d\n", display);
    } else {
        s4354198_o("Invalid display specified\n");
    }
}

/**
 * Handles the listing of displays
 */
void handle_list() {
    if (num_displays() > 0) {
        for (int i = 0; i < MAX_DISPLAYS; i++) {
            int display = app->displays[i];

            if (display >= 0) {
                s4354198_o("Display #%d is at (%d, %d) with size %dx%d\n",
                    display,
                    display_get_attr(display, 0),
                    display_get_attr(display, 1),
                    display_get_attr(display, 2),
                    display_get_attr(display, 3)
                );
            }
        }
    } else {
        s4354198_o("There are no current displays\n");
    }
}

/**
 * Handles clearing of a display
 */
void handle_clear(char* input) {
    if (strlen(input) > 0) {
        for (int i = 0; i < strlen(input); i++) {
            if (!isdigit(input[i])) {
                s4354198_o("Invalid display specified\n");
                return;
            }
        }
        
        int display = atoi(input);

        if (valid_display(display)) {
            s4354198_o("Clearing display #%d\n", display);
            int width = display_get_attr(display, 2);
            int height = display_get_attr(display, 3);
            start_avconv(display, width, height, "lutrgb=r=0:g=0:b=0", NULL);
        } else {
            s4354198_o("Invalid display specified\n");
        }
    } else {
        s4354198_o("Invalid display specified\n");
    }
}

/**
 * Handles the img command
 */
void handle_img(char* input) {
    char* original = strdup(input);
    char* originalPtr = original;

    if (num_displays() > 0) {
        char* selDisplay = s4354198_mem_get(MEM_SEL_DISP);

        if (selDisplay[0] == '-') {
            s4354198_o("Please select a display first\n");
        } else {
            int display = atoi(selDisplay);
            char* token = strsep(&input, " ");

            int width = display_get_attr(display, 2);
            int height = display_get_attr(display, 3);

            s4354198_o("Attempting to apply effect to display #%d\n", display);

            if (s4354198_str_match(token, "raw")) {
                start_avconv(display, width, height, "", NULL);
            } else if (s4354198_str_match(token, "flp")) {
                start_avconv(display, width, height, "vflip", NULL);
            } else if (s4354198_str_match(token, "dly")) {
                bool valid = true;
                for (int i = 0; i < strlen(input); i++) {
                    if (!isdigit(i)) {
                        valid = false;
                        break;
                    }
                }
                if (valid) {
                    start_avconv(display, width, height, "", input);
                } else {
                    s4354198_o("Invalid delay rate specified\n");
                }
            } else if (s4354198_str_match(token, "blr")) {
                start_avconv(display, width, height, "boxblur=4:1", NULL);
            } else if (s4354198_str_match(token, "custom")) {
                start_avconv(display, width, height, input, NULL);
            } else if (s4354198_str_match(token, "text")) {
                char textStr[500];
                sprintf(textStr, "drawtext=text='%s':fontfile=/usr/share/fonts/truetype/roboto/Roboto-Regular.ttf:x=((main_w-(text_w/2))/2):y=((main_h-(text_h/2))/2):fontcolor=white:fontsize=24:shadowcolor=black:shadowx=1:shadowy=1", input);
                start_avconv(display, width, height, textStr, NULL);
            } else {
                s4354198_o("Invalid effect specified\n");
            }
        }
    } else {
        s4354198_o("There are currently no displays\n");
    }

    free(originalPtr);
}

/**
 * Checks if a display is valid
 */
bool valid_display(int display) {
    bool valid = false;

    for (int i = 0; i < MAX_DISPLAYS; i++) {
        if (app->displays[i] == display) {
            valid = true;
            break;
        }
    }

    return valid;
}

/**
 * Returns the number of displays
 */
int num_displays() {
    int count = 0;

    for (int i = 0; i < MAX_DISPLAYS; i++) {
        if (app->displays[i] >= 0) {
            count++;
        }
    }

    return count;
}

/**
 * Returns the number of avconv processes
 */
int num_av() {
    int count = 0;

    for (int i = 0; i < MAX_DISPLAYS; i++) {
        if (app->runtimeInfo->avconv[i] >= 0) {
            count++;
        }
    }

    return count;
}

/**
 * Gets the attr of a display
 */
int display_get_attr(int display, int attr) {
    char* displays = s4354198_mem_get(MEM_DISPLAYS);
    char* ptr = displays;
    int result = 0;

    char* curDisplay = strsep(&displays, ",");
    while (curDisplay != NULL) {
        char* dataOrig = strsep(&curDisplay, ",");
        char* data = strdup(dataOrig);
        char* dataPtr = data;

        char* id = strsep(&data, ":");
        char* x = strsep(&data, ":");
        char* y = strsep(&data, ":");
        char* width = strsep(&data, ":");
        char* height = data;

        if (atoi(id) == display) {
            switch (attr) {
                case 0:
                    result = atoi(x);
                    break;
                case 1:
                    result = atoi(y);
                    break;
                case 2:
                    result = atoi(width);
                    break;
                case 3:
                    result = atoi(height);
                    break;
            }
            free(dataPtr);
            break;
        }
        free(dataPtr);
        curDisplay = strsep(&displays, ",");
    }

    free(ptr);

    return result;
}

/**
 * Gets the storage space for the next display id.
 */
int* next_display() {
    int* result = NULL;

    for (int i = 0; i < MAX_DISPLAYS; i++) {
        if (app->displays[i] == -1) {
            result = &(app->displays[i]);
            break;
        }
    }

    return result;
}

/**
 * Handles exit 
 */
void sigint_handler(int signal) {
    int status;
    s4354198_p("Exiting control\n");

    for (int i = 0; i < num_displays(); i++) {
        if (app->runtimeInfo->avconv[i] >= 0) {
            kill(app->runtimeInfo->avconv[i], SIGINT);
            waitpid(app->runtimeInfo->avconv[i], &status, 0);
            s4354198_p("avconv %d has been killed\n", i);
        }

        if (app->runtimeInfo->display[i] >= 0) {
            kill(app->runtimeInfo->display[i], SIGINT);
            waitpid(app->runtimeInfo->display[i], &status, 0);
            s4354198_p("display %d has been killed\n", i);
        }
    }

    exit(0);
}

/**
 * Starts the avconv process for a specific display
 */
void start_avconv(int display, int width, int height, char* options, char* delay) {
    int status;

    sem_wait(&canWrite);
    if (app->runtimeInfo->avconv[display] >= 0) {
        kill(app->runtimeInfo->avconv[display], SIGINT);
        sleep(2);
        waitpid(app->runtimeInfo->avconv[display], &status, WNOHANG);
        kill(app->runtimeInfo->avconv[display], SIGKILL);
        waitpid(app->runtimeInfo->avconv[display], &status, 0);
    }

    pid_t pid = fork();
    
    if (pid > 0) {
        char* result = NULL;
        s4354198_wait_mem_get(MEM_NEW_DISP, "-", &result);
        free(result);

        app->runtimeInfo->avconv[display] = pid;

        sem_post(&canWrite);
    } else {
        // Start avconv
        dup2(app->comms->toDisplayFd[display], STDOUT);
        dup2(app->comms->toAvFd[display], STDIN);

        // Throwaway stderr
        int devNull = open("/dev/null", O_WRONLY);
        dup2(devNull, STDERR);

        char scale[50];
        sprintf(scale, "scale=w=%d:h=%d", width, height);
        
        char widthS[10];
        char heightS[10];

        sprintf(widthS, "%d", width);
        sprintf(heightS, "%d", height);

        if (delay != NULL) {
            execlp(AVCONV_EXECUTABLE, AVCONV_EXECUTABLE, 
                "-y",
                "-f", "image2pipe", "-c:v", "ppm", "-i", "pipe:0", "-r", "30", 
                "-f", "image2pipe", "-c:v", "ppm", "-r", delay, "-filter:v", scale, "pipe:1",
                NULL
            );
        } else {
            if (strlen(options) > 0) {
                char actualOptions[200];
                sprintf(actualOptions, "[in] %s, %s [out]", scale, options);
                execlp(AVCONV_EXECUTABLE, AVCONV_EXECUTABLE, 
                    "-y",
                    "-f", "image2pipe", "-c:v", "ppm", "-i", "pipe:0", "-r", "30", 
                    "-f", "image2pipe", "-c:v", "ppm", "-r", "30", "-vf", actualOptions, "pipe:1",
                    NULL
                );
            } else {
                execlp(AVCONV_EXECUTABLE, AVCONV_EXECUTABLE, 
                    "-y",
                    "-f", "image2pipe", "-c:v", "ppm", "-i", "pipe:0", "-r", "30", 
                    "-f", "image2pipe", "-c:v", "ppm", "-r", "30", "-filter:v", scale, "pipe:1",
                    NULL
                );
            }
        }
        s4354198_exit(1, "execl failed to create avconv process.\n");
    }
}

/**
 * Start a display
 */
int start_display(int x, int y, int width, int height) {
    char* result = NULL;
    s4354198_wait_mem_get(MEM_NEW_DISP, "-", &result);
    free(result);

    int thisDisplay = num_displays();

    char display[5];
    sprintf(display, "%d", thisDisplay);
    s4354198_mem_set(MEM_NEW_DISP, display);

    int processOut[2];
    int processIn[2];
    int avConvIn[2];

    if (pipe(processOut) || pipe(processIn) || pipe(avConvIn)) {
        s4354198_exit(1, "Failed to create display process pipes.\n");
    }

    pid_t pid = fork();
    
    if (pid == -1) {
        // Failed to fork
        s4354198_exit(1, "Error creating display process.\n");
    } else if (pid > 0) {
        app->runtimeInfo->display[thisDisplay] = pid;
        app->comms->toAv[thisDisplay] = fdopen(avConvIn[SOCKET_WRITE], "w");
        app->comms->fromDisplay[thisDisplay] = fdopen(processOut[SOCKET_READ], "r");
        app->comms->toAvFd[thisDisplay] = avConvIn[SOCKET_READ];
        app->comms->toDisplayFd[thisDisplay] = processIn[SOCKET_WRITE];
        app->runtimeInfo->display[thisDisplay] = pid;

        int* next = next_display();
        *next = thisDisplay;

        sleep(1);
    } else {
        dup2(processOut[SOCKET_WRITE], STDOUT);
        dup2(processIn[SOCKET_READ], STDIN);

        char xS[10];
        char yS[10];
        char widthS[10];
        char heightS[10];

        sprintf(xS, "%d", x);
        sprintf(yS, "%d", y);
        sprintf(widthS, "%d", width);
        sprintf(heightS, "%d", height);

        execlp(DISPLAY_EXECUTABLE, DISPLAY_EXECUTABLE, xS, yS, widthS, heightS, NULL);
        s4354198_exit(1, "execl failed to create display process.\n");
    }

    return thisDisplay;
}

/**
 * Video buffer struct
 */
struct buffer {
    void   *start;
    size_t length;
};

/**
 * No idea, i copied this from pracs :) tehe
 */
static void xioctl(int fh, int request, void *arg)
{
    int r;

    do {
        r = v4l2_ioctl(fh, request, arg);
    } while (r == -1 && ((errno == EINTR) || (errno == EAGAIN)));

    if (r == -1) {
        fprintf(stderr, "error %d, %s\\n", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }
}

/**
 * Start fetching frames
 */
void* stream_video(void* voidPtr) {
    struct v4l2_format              fmt;
    struct v4l2_buffer              buf;
    struct v4l2_requestbuffers      req;
    enum v4l2_buf_type              type;
    fd_set                          fds;
    struct timeval                  tv;
    int                             r, fd = -1;
    unsigned int                    i, n_buffers;
    char                            *dev_name = "/dev/video0";
    struct buffer                   *buffers;

    fd = v4l2_open(dev_name, O_RDWR | O_NONBLOCK, 0);
    if (fd < 0) {
        perror("Cannot open device");
        exit(EXIT_FAILURE);
    }

    CLEAR(fmt);
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = FRAME_WIDTH;
    fmt.fmt.pix.height      = FRAME_HEIGHT;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24;
    fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;
    xioctl(fd, VIDIOC_S_FMT, &fmt);
    if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_RGB24) {
        s4354198_p("Libv4l didn't accept RGB24 format. Can't proceed.\\n");
        exit(EXIT_FAILURE);
    }
    if ((fmt.fmt.pix.width != FRAME_WIDTH) || (fmt.fmt.pix.height != FRAME_HEIGHT)) {
        s4354198_p("Warning: driver is sending image at %dx%d\\n",
                fmt.fmt.pix.width, fmt.fmt.pix.height);
    }

    CLEAR(req);
    req.count = 2;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    xioctl(fd, VIDIOC_REQBUFS, &req);

    buffers = calloc(req.count, sizeof(*buffers));
    for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
        CLEAR(buf);

        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = n_buffers;

        xioctl(fd, VIDIOC_QUERYBUF, &buf);

        buffers[n_buffers].length = buf.length;
        buffers[n_buffers].start = v4l2_mmap(NULL, buf.length,
                    PROT_READ | PROT_WRITE, MAP_SHARED,
                    fd, buf.m.offset);

        if (MAP_FAILED == buffers[n_buffers].start) {
                perror("mmap");
                exit(EXIT_FAILURE);
        }
    }

    for (i = 0; i < n_buffers; ++i) {
        CLEAR(buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        xioctl(fd, VIDIOC_QBUF, &buf);
    }
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    xioctl(fd, VIDIOC_STREAMON, &type);

    while (1) {
        do {
            FD_ZERO(&fds);
            FD_SET(fd, &fds);

            /* Timeout. */
            tv.tv_sec = 2;
            tv.tv_usec = 0;

            r = select(fd + 1, &fds, NULL, NULL, &tv);
        } while ((r == -1 && (errno = EINTR)));
        if (r == -1) {
            perror("select");
        }

        CLEAR(buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        xioctl(fd, VIDIOC_DQBUF, &buf);
        
        //s4354198_p("Locking\n");
        sem_wait(&canWrite);
        //s4354198_p("Locked\n");
        for (int i = 0; i < num_av(); i++) {
            if (app->runtimeInfo->avconv[i] >= 0) {
                fprintf(app->comms->toAv[i], "P6\n%d %d %d\n",
                    fmt.fmt.pix.width, fmt.fmt.pix.height, FRAME_DEPTH);
                fwrite(buffers[buf.index].start, buf.bytesused, 1, app->comms->toAv[i]);
                fflush(app->comms->toAv[i]);
            }
        }
        sem_post(&canWrite);

        xioctl(fd, VIDIOC_QBUF, &buf);
    }
    
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    xioctl(fd, VIDIOC_STREAMOFF, &type);
    for (i = 0; i < n_buffers; ++i)
        v4l2_munmap(buffers[i].start, buffers[i].length);
    v4l2_close(fd);
}

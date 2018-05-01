#ifndef DEFINES_H
#define DEFINES_H

#define MIN_WIDTH 10
#define MAX_WIDTH 20
#define MIN_HEIGHT 10
#define MAX_HEIGHT 20
#define MIN_REFRESH 200
#define MAX_REFRESH 2000

#define DISPLAY_EXECUTABLE "display"
#define CAG_EXECUTABLE "cag"

#define PROMPT ":^) "
#define PROMPT_COLOUR "\e[31;1m"
#define PROMPT_INPUT_COLOUR "\e[0m\e[32m"
#define PROMPT_RESET "\e[0m"
#define PROMPT_HELP "\e[0m\e[33m"
#define PROMPT_ERROR "\e[0m\e[33m"
#define PROMPT_TIME "\e[34;1m"

#define CMD_CELL "cell"
#define CMD_STILL "still"
#define CMD_OSC "osc"
#define CMD_SHIP "ship"
#define CMD_START "start"
#define CMD_STOP "stop"
#define CMD_CLEAR "clear"
#define CMD_HELP "help"
#define CMD_END "end"

#define FORM_ALIVE "alive"
#define FORM_DEAD "dead"
#define FORM_BLOCK "block"
#define FORM_BEEHIVE "beehive"
#define FORM_LOAF "loaf"
#define FORM_BOAT "boat"
#define FORM_BLINKER "blinker"
#define FORM_TOAD "toad"
#define FORM_BEACON "beacon"
#define FORM_GLIDER "glider"

#define SOCKET_READ 0
#define SOCKET_WRITE 1

#define STDIN 0
#define STDOUT 1
#define STDERR 2

#define FIFO_SHELL_CAG "/tmp/fifo_shell_cag"
#define FIFO_SHELL_CAG_PERMS 0666
#define FIFO_CAG_SHELL "/tmp/fifo_cag_shell"
#define FIFO_CAG_SHELL_PERMS 0666
#define FIFO_CAG_DISPLAY "/tmp/fifo_cag_display"
#define FIFO_CAG_DISPLAY_PERMS 0666
#define FIFO_DISPLAY_CAG "/tmp/fifo_display_cag"
#define FIFO_DISPLAY_CAG_PERMS 0666

#define COMMS_NEW "new"
#define COMMS_STOP "stop"
#define COMMS_START "start"
#define COMMS_CLEAR "clear"
#define COMMS_DEAD "dead"

#define STATE_DEAD 0

#define BUFFER_SIZE 128

#define DELIM_LEFT "["
#define DELIM_RIGHT "]"

#define DEL 127
#define BACKSPACE "\b \b"

#define CELL_SIDE 20

#endif
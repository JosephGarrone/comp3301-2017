#ifndef DEFINES_H
#define DEFINES_H

#define DISPLAY_EXECUTABLE "./wcd-display"
#define CONTROL_EXECUTABLE "./wcd-control"
#define REMOTE_EXECUTABLE "./wcd-remote"
#define MEMCACHED_EXECUTABLE "./wcd-mem"
#define AVCONV_EXECUTABLE "avconv"

#define PROMPT ":^) "
#define PROMPT_COLOUR "\e[31;1m"
#define PROMPT_INPUT_COLOUR "\e[0m\e[32m"
#define PROMPT_RESET "\e[0m"
#define PROMPT_HELP "\e[0m\e[33m"
#define PROMPT_ERROR "\e[0m\e[33m"
#define PROMPT_TIME "\e[34;1m"

#define CMD_HELP "help"
#define CMD_WCD "wcd"
#define CMD_SYS "sys"
#define CMD_CLEAR "clear"
#define CMD_LIST "list"
#define CMD_KILL "kill"
#define CMD_SEL "sel"
#define CMD_IMG "img"
#define CMD_EXIT "exit"

#define FRAME_WIDTH 640
#define FRAME_HEIGHT 480
#define FRAME_HEADER 15
#define FRAME_DEPTH 255

#define SOCKET_READ 0
#define SOCKET_WRITE 1

#define STDIN 0
#define STDOUT 1
#define STDERR 2

#define BUFFER_SIZE 128

#define DEL 127
#define BACKSPACE "\b \b"

#define MEM_DISPLAYS "displays"
#define MEM_NEW_DISP "new_display"
#define MEM_SEL_DISP "sel_display"
#define MEM_KEY "key"

#define CLEAR(x) memset(&(x), 0, sizeof(x))

#endif
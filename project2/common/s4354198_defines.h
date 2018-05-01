#ifndef DEFINES_H
#define DEFINES_H

#define MIN_WIDTH 10
#define MAX_WIDTH 20
#define MIN_HEIGHT 10
#define MAX_HEIGHT 20
#define MIN_REFRESH 200
#define MAX_REFRESH 2000

#define DISPLAY_EXECUTABLE "./display"
#define CAG_EXECUTABLE "./cag"
#define CP_EXECUTABLE "./player"
#define CR_EXECUTABLE "./recorder"

#define PROMPT ":^) %s) "
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
#define CMD_MOUNT "mount"
#define CMD_LS "ls"
#define CMD_CD "cd"
#define CMD_MKDIR "mkdir"
#define CMD_FRAME "frame"
#define CMD_REC "rec"
#define CMD_SIZE "size"
#define CMD_FREE "free"
#define CMD_S "s"
#define CMD_P "p"
#define CMD_PLAY "play"
#define CMD_TOGGLE_DRAWINGS_OUT "stfu"
#define CMD_TOUCH "touch"
#define CMD_HALP "halp"
#define CMD_STOP_OUTPUT "stop_output"
#define CMD_START_OUTPUT "start_output"

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
#define FIFO_SHELL_CR "/tmp/fifo_shell_cr"
#define FIFO_SHELL_CR_PERMS 0666
#define FIFO_SHELL_CP "/tmp/fifo_shell_cp"
#define FIFO_SHELL_CP_PERMS 0666
#define FIFO_CAG_SHELL "/tmp/fifo_cag_shell"
#define FIFO_CAG_SHELL_PERMS 0666
#define FIFO_CAG_DISPLAY "/tmp/fifo_cag_display"
#define FIFO_CAG_DISPLAY_PERMS 0666
#define FIFO_DISPLAY_CAG "/tmp/fifo_display_cag"
#define FIFO_DISPLAY_CAG_PERMS 0666
#define FIFO_CR_SHELL "/tmp/fifo_cr_shell"
#define FIFO_CR_SHELL_PERMS 0666
#define FIFO_CAG_CR "/tmp/fifo_cag_cr"
#define FIFO_CAG_CR_PERMS 0666
#define FIFO_CP_SHELL "/tmp/fifo_cp_shell"
#define FIFO_CP_SHELL_PERMS 0666
#define FIFO_CP_DISPLAY "/tmp/fifo_cp_display"
#define FIFO_CP_DISPLAY_PERMS 0666

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

#define DEFAULT_DIR "/"
#define ROOT_DIR "/"

#define ERR_NO_MOUNTED PROMPT_ERROR "No mounted filesystem\n" PROMPT_RESET

#define CFS_ROOT "RootCFS"
#define CFS_VOLUME "FV"
#define CFS_SECTOR "Sector"
#define CFS_FINODE "FInode"
#define CFS_DINODE "DInode"
#define CFS_ATTR_NAME "Name"
#define CFS_ATTR_FV "FileVolume"
#define CFS_ATTR_TIMESTAMP "Timestamp"
#define CFS_ATTR_FILECOUNT "FileCount"
#define CFS_ATTR_MODE "Mode"
#define CFS_ATTR_INODE "FInodeNumber"
#define CFS_ATTR_OWNER "Owner"
#define CFS_ATTR_SECTOR_COUNT "SectorCount"
#define CFS_ATTR_INODES "FileINodes"
#define CFS_ATTR_SECTORS "Sectors"
#define CFS_ATTR_SPACE_MAP "SpaceMap"
#define CFS_OWNER "Joseph Garrone (s4354198)"
#define CFS_NO_DATA "---"
#define CFS_DEFAULT_MODE "rw-"
#define CFS_VOLUME_COUNT 4
#define CFS_VOLUME_SECTORS 600
#define CFS_SECTOR_RANK 2
#define CFS_SECTOR_WIDTH 20
#define CFS_SECTOR_HEIGHT 20
#define CFS_MAX_FILES 1024

#define PR_CMD_INIT "init"
#define PR_CMD_START "start"
#define PR_CMD_PAUSE "pause"
#define PR_CMD_RESUME "resume"
#define PR_CMD_STOP "stop"
#define PR_CMD_DONE "done"

#endif
#ifndef CFS_H
#define CFS_H

#include "hdf5.h"
#include "s4354198_structs.h"

/* Function prototypes */
void s4354198_createCFS(char* filename);
char* s4354198_path_get_abs(char* cwd, char* path);
void s4354198_mkdir(PathInfo* pathInfo);
void s4354198_mkfile(PathInfo* pathInfo);
INode* s4354198_ls(PathInfo* pathInfo);
bool s4354198_path(char* path, PathInfo** result, char** msg);
void s4354198_path_free_info(PathInfo* pathInfo);
bool s4354198_path_taken(PathInfo* pathInfo, bool* byDir);
bool s4354198_write_sector_to_file(char* filename, int* data);
int s4354198_get_file_sector_count(PathInfo* pathInfo);
int s4354198_get_file_sector_count_from_filename(char* filename);
int* s4354198_get_file_frame(PathInfo* pathInfo, int frame);
int* s4354198_get_file_frame_from_filename(char* filename, int frame);
int s4354198_get_used_sectors();
PathInfo* s4354198_path_info();

#endif
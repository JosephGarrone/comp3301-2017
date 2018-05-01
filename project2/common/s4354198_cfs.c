#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <ctype.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <termios.h>
#include <semaphore.h>
#include <stdarg.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "s4354198_utils.h"
#include "s4354198_defines.h"
#include "s4354198_structs.h"
#include "s4354198_cfs.h"

#include "hdf5.h"

/* Externs */
extern Application* app;

/* Function prototypes */
void cfs_mkdir(hid_t root, PathInfo* pathInfo);
void cfs_mkfile(hid_t root, PathInfo* pathInfo);
int get_next_dir_inode(hid_t root);
int get_next_file_inode(hid_t root);
herr_t iterate_dir_count(hid_t loc, const char* name, const H5L_info_t* info, void* data);
herr_t iterate_file_count(hid_t loc, const char* name, const H5L_info_t* info, void* data);
herr_t iterate_object_exists(hid_t loc, const char* name, const H5L_info_t* info, void* data);
herr_t iterate_ls(hid_t loc, const char* name, const H5L_info_t* info, void* data);
hid_t wait_open_cfs();
int get_file_sector_count(int* sectors, int length);
int* get_file_sectors(hid_t file);
char* get_file_directory(int inode, hid_t root);
int* get_dir_inodes(hid_t dir);
herr_t iterate_find_dir(hid_t loc, const char* name, const H5L_info_t* infoh5, void* data);
void set_dir_inodes(hid_t dir, int* inodes);
char* get_dir_name_nice(hid_t root, char* nodeName);
char* get_dir_name(hid_t root, char* niceName);
herr_t iterate_find_dir_by_name(hid_t loc, const char* name, const H5L_info_t* infoh5, void* data);
hid_t get_file(PathInfo* pathInfo, hid_t root);
herr_t find_file(hid_t loc, const char* name, const H5L_info_t* infoh5, void* data);
int get_volume_next_sector(char* volumeName, hid_t root);
void write_volume_sector(char* volumeName, char* sectorName, hid_t file, int* data);
void mark_used_volume_sector(char* volumeName, int sector, hid_t hdfFile);

/**
 * Waits until it can open the CFS file
 */
hid_t wait_open_cfs() {
    hid_t file = -1;

    while ((file = H5Fopen(app->cfs->filename, H5F_ACC_RDWR, H5P_DEFAULT)) < 0) {
        ; // Read open
    }

    app->cfs->file = file;

    return file;
}

/**
 * Creates the initial HDF volume configuration
 */
void s4354198_createCFS(char* filename) {
    hid_t file = H5Fcreate(filename, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);

    // Create root
    hid_t group = H5Gcreate(file, CFS_ROOT, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);

    // Create volumes
    for (int i = 0; i < CFS_VOLUME_COUNT; i++) {
        char volume[16];
        sprintf(volume, "%s%d", CFS_VOLUME, i);

        hid_t fileVolume = H5Gcreate(file, volume, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);

        // Create sectors
        for (int j = 0; j < CFS_VOLUME_SECTORS; j++) {
            char sector[16];
            sprintf(sector, "%s%d", CFS_SECTOR, j);

            hsize_t dims[CFS_SECTOR_RANK] = {CFS_SECTOR_WIDTH, CFS_SECTOR_HEIGHT};

            hid_t space = H5Screate_simple(CFS_SECTOR_RANK, dims, NULL);
            hid_t dataSector = H5Dcreate(fileVolume, sector, H5T_STD_I32LE, space,
                H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);

            H5Sclose(space);
            H5Dclose(dataSector);
        }

        // Create space map
        hsize_t dimSec[2] = {CFS_VOLUME_SECTORS, 1};
        hid_t space = H5Screate_simple(2, dimSec, NULL);
        hid_t attr = H5Acreate(fileVolume, CFS_ATTR_SPACE_MAP, H5T_NATIVE_INT, space, H5P_DEFAULT, H5P_DEFAULT);
        int flatData[CFS_VOLUME_SECTORS];
        for (int i = 0; i < CFS_VOLUME_SECTORS; i++) {
            flatData[i] = 0;
        }
        H5Awrite(attr, H5T_NATIVE_INT, &flatData);
        H5Sclose(space);
        H5Aclose(attr);

        H5Gclose(fileVolume);
    }

    H5Gclose(group);
    H5Fclose(file);
}

/**
 * Gets the number of used sectors
 */
int s4354198_get_used_sectors() {
    int used = 0;

    hid_t hdfFile = wait_open_cfs();
    hid_t root = H5Gopen(hdfFile, CFS_ROOT, H5P_DEFAULT);

    char volumeName[20];
    for (int i = 0; i < CFS_VOLUME_COUNT; i++) {
        sprintf(volumeName, "%s%d", CFS_VOLUME, i);
        
        hid_t volume = H5Gopen(hdfFile, volumeName, H5P_DEFAULT);
        
        hid_t sectors = H5Aopen(volume, CFS_ATTR_SPACE_MAP, H5P_DEFAULT);
        hid_t dataSpace = H5Aget_space(sectors);
        hsize_t dimensions[2];
        H5Sget_simple_extent_dims(dataSpace, dimensions, NULL);

        int* data = (int*) malloc(sizeof(int) * dimensions[0] * dimensions[1]);
        H5Aread(sectors, H5T_NATIVE_INT, data);

        for (int i = 0; i < dimensions[0] * dimensions[1]; i++) {
            if (data[i] != 0) {
                used++;
            }
        }

        free(data);

        H5Aclose(sectors);
        H5Gclose(volume);
    }
    
    H5Gclose(root);
    H5Fclose(hdfFile);

    return used;
}

/**
 * Reads a frame from a file, specified by name
 */
int* s4354198_get_file_frame_from_filename(char* filename, int frame) {
    char* msg;
    PathInfo* pathInfo = s4354198_path_info();

    s4354198_path(filename, &pathInfo, &msg);

    int* data = s4354198_get_file_frame(pathInfo, frame);

    s4354198_path_free_info(pathInfo);

    return data;
}

/**
 * Reads a frame from a file
 */
int* s4354198_get_file_frame(PathInfo* pathInfo, int frame) {
    // Move to 0 indexing
    frame--;

    int* data = malloc(sizeof(int) * MAX_WIDTH * MAX_HEIGHT);
    char sectorName[20];

    hid_t hdfFile = wait_open_cfs();
    hid_t root = H5Gopen(hdfFile, CFS_ROOT, H5P_DEFAULT);
    hid_t file = get_file(pathInfo, root);

    // Get fv sector
    int fvSector;
    hid_t fvSectors = H5Dopen(file, CFS_ATTR_SECTORS, H5P_DEFAULT);
    hid_t dataSpace = H5Dget_space(fvSectors);
    int rank = H5Sget_simple_extent_ndims(dataSpace);
    hsize_t dimensions[2];
    H5Sget_simple_extent_dims(dataSpace, dimensions, NULL);
    hsize_t offset[2] = {0, 0};
    H5Sselect_hyperslab(dataSpace, H5S_SELECT_SET, offset, NULL, dimensions, NULL);
    hid_t memSpace = H5Screate_simple(rank, dimensions, NULL);
    H5Sselect_hyperslab(memSpace, H5S_SELECT_SET, offset, NULL, dimensions, NULL);
    int* sectorData = (int*) malloc(sizeof(int) * dimensions[0] * dimensions[1]);
    H5Dread(fvSectors, H5T_NATIVE_INT, memSpace, dataSpace, H5P_DEFAULT, sectorData);
    fvSector = sectorData[frame];
    H5Dclose(fvSectors);
    sprintf(sectorName, "%s%d", CFS_SECTOR, fvSector);

    // Get fv sector data
    hid_t volume = H5Gopen(hdfFile, pathInfo->volume, H5P_DEFAULT);
    hid_t sector = H5Dopen(volume, sectorName, H5P_DEFAULT);
    
    dataSpace = H5Dget_space(sector);
    rank = H5Sget_simple_extent_ndims(dataSpace);
    H5Sget_simple_extent_dims(dataSpace, dimensions, NULL);
    offset[0] = 0;
    offset[1] = 0;
    H5Sselect_hyperslab(dataSpace, H5S_SELECT_SET, offset, NULL, dimensions, NULL);
    memSpace = H5Screate_simple(rank, dimensions, NULL);
    H5Sselect_hyperslab(memSpace, H5S_SELECT_SET, offset, NULL, dimensions, NULL);
    H5Dread(sector, H5T_NATIVE_INT, memSpace, dataSpace, H5P_DEFAULT, data);

    H5Dclose(sector);
    H5Gclose(volume);

    H5Gclose(file);
    H5Gclose(root);
    H5Fclose(hdfFile);

    return data;
}

/**
 * Gets the number of sectors of a file
 */
int s4354198_get_file_sector_count(PathInfo* pathInfo) {
    int sectors = 0;

    hid_t hdfFile = wait_open_cfs();
    hid_t root = H5Gopen(hdfFile, CFS_ROOT, H5P_DEFAULT);
    hid_t file = get_file(pathInfo, root);

    // Get the sector count
    hid_t attr = H5Aopen(file, CFS_ATTR_SECTOR_COUNT, H5P_DEFAULT);
    hid_t fileType = H5Aget_type(attr);
    hid_t memType = H5Tget_native_type(fileType, H5T_DIR_ASCEND);
    H5Aread(attr, memType, &sectors);
    H5Tclose(fileType);
    H5Tclose(memType);
    H5Aclose(attr);
    
    H5Gclose(file);
    H5Gclose(root);
    H5Fclose(hdfFile);

    return sectors;
}

/**
 * Gets the file sector count from an absolute path
 */
int s4354198_get_file_sector_count_from_filename(char* filename) {
    char* msg;
    PathInfo* pathInfo = s4354198_path_info();

    s4354198_path(filename, &pathInfo, &msg);

    int sectors = s4354198_get_file_sector_count(pathInfo);

    s4354198_path_free_info(pathInfo);

    return sectors;
}

/**
 * Writes a sector to a file
 */
bool s4354198_write_sector_to_file(char* filename, int* data) {
    bool success = true;
    char* path = strdup(filename);
    char sector[20];

    hid_t hdfFile = wait_open_cfs();
    hid_t root = H5Gopen(hdfFile, CFS_ROOT, H5P_DEFAULT);

    char* msg;
    PathInfo* pathInfo = s4354198_path_info();

    s4354198_path(path, &pathInfo, &msg);

    // Get the new sector id
    hid_t file = get_file(pathInfo, root);
    int volumeSector = get_volume_next_sector(pathInfo->volume, hdfFile);
    sprintf(sector, "%s%d", CFS_SECTOR, volumeSector);

    // Write the sector
    if (volumeSector >= 0) {
        write_volume_sector(pathInfo->volume, sector, hdfFile, data);
        mark_used_volume_sector(pathInfo->volume, volumeSector, hdfFile);
    } else {
        success = false;
        free(pathInfo);
        free(path);
    
        H5Gclose(file);
        H5Gclose(root);
        H5Fclose(hdfFile);
        return success;
    }

    // Update the files info
    int sectors;

    // Update the sector count of the file
    hid_t attr = H5Aopen(file, CFS_ATTR_SECTOR_COUNT, H5P_DEFAULT);
    hid_t fileType = H5Aget_type(attr);
    hid_t memType = H5Tget_native_type(fileType, H5T_DIR_ASCEND);
    H5Aread(attr, memType, &sectors);
    sectors++;
    H5Awrite(attr, memType, &sectors);
    H5Tclose(fileType);
    H5Tclose(memType);
    H5Aclose(attr);

    // Update the file volume sector indices
    hid_t fvSectors = H5Dopen(file, CFS_ATTR_SECTORS, H5P_DEFAULT);
    hid_t dataSpace = H5Dget_space(fvSectors);
    int rank = H5Sget_simple_extent_ndims(dataSpace);
    hsize_t dimensions[2];
    H5Sget_simple_extent_dims(dataSpace, dimensions, NULL);
    hsize_t offset[2] = {0, 0};
    H5Sselect_hyperslab(dataSpace, H5S_SELECT_SET, offset, NULL, dimensions, NULL);
    hid_t memSpace = H5Screate_simple(rank, dimensions, NULL);
    H5Sselect_hyperslab(memSpace, H5S_SELECT_SET, offset, NULL, dimensions, NULL);
    int* sectorData = (int*) malloc(sizeof(int) * dimensions[0] * dimensions[1]);
    H5Dread(fvSectors, H5T_NATIVE_INT, memSpace, dataSpace, H5P_DEFAULT, sectorData);
    for (int i = 0; i < CFS_VOLUME_SECTORS; i++) {
        if (sectorData[i] == -1) {
            sectorData[i] = volumeSector;
            break;
        }
    }
    H5Dwrite(fvSectors, H5T_NATIVE_INT, memSpace, dataSpace, H5P_DEFAULT, sectorData);
    H5Dclose(fvSectors);

    // Add timestamp
    struct timeval time;
    gettimeofday(&time, NULL);
    unsigned long long milli =
        (unsigned long long)(time.tv_sec) * 1000 +
        (unsigned long long)(time.tv_usec) / 1000;
    char timestamp[32];
    sprintf(timestamp, "%llu", milli);
    fileType = H5Tcopy(H5T_C_S1);
    H5Tset_size(fileType, strlen(timestamp) + 1);
    memType = H5Tcopy(H5T_C_S1);
    H5Tset_size(memType, strlen(timestamp) + 1);
    dataSpace = H5Screate_simple(1, dimensions, NULL);
    attr = H5Aopen(file, CFS_ATTR_TIMESTAMP, H5P_DEFAULT);
    H5Awrite(attr, memType, timestamp);
    H5Sclose(dataSpace);
    H5Tclose(fileType);
    H5Tclose(memType);
    H5Aclose(attr);

    free(pathInfo);
    free(path);

    H5Gclose(file);
    H5Gclose(root);
    H5Fclose(hdfFile);

    return success;
}

/**
 * Marks a sector as used
 */
void mark_used_volume_sector(char* volumeName, int sector, hid_t hdfFile) {
    hid_t volume = H5Gopen(hdfFile, volumeName, H5P_DEFAULT);

    hid_t sectors = H5Aopen(volume, CFS_ATTR_SPACE_MAP, H5P_DEFAULT);
    hid_t dataSpace = H5Aget_space(sectors);
    hsize_t dimensions[2];
    H5Sget_simple_extent_dims(dataSpace, dimensions, NULL);

    int* data = (int*) malloc(sizeof(int) * dimensions[0] * dimensions[1]);
    H5Aread(sectors, H5T_NATIVE_INT, data);

    data[sector] = 1;

    H5Awrite(sectors, H5T_NATIVE_INT, data);

    H5Aclose(sectors);
    H5Gclose(volume);
}

/**
 * Write data to a volume sector
 */
void write_volume_sector(char* volumeName, char* sectorName, hid_t file, int* data) {
    hid_t volume = H5Gopen(file, volumeName, H5P_DEFAULT);
    hid_t sector = H5Dopen(volume, sectorName, H5P_DEFAULT);
    
    hid_t dataSpace = H5Dget_space(sector);
    int rank = H5Sget_simple_extent_ndims(dataSpace);
    hsize_t dimensions[2];
    H5Sget_simple_extent_dims(dataSpace, dimensions, NULL);
    hsize_t offset[2] = {0, 0};
    H5Sselect_hyperslab(dataSpace, H5S_SELECT_SET, offset, NULL, dimensions, NULL);
    hid_t memSpace = H5Screate_simple(rank, dimensions, NULL);
    H5Sselect_hyperslab(memSpace, H5S_SELECT_SET, offset, NULL, dimensions, NULL);

    H5Dwrite(sector, H5T_NATIVE_INT, memSpace, dataSpace, H5P_DEFAULT, data);

    H5Dclose(sector);
    H5Gclose(volume);
}

/**
 * Gets the id of the next available sector
 */
int get_volume_next_sector(char* volumeName, hid_t root) {
    int sector = -1;

    hid_t volume = H5Gopen(root, volumeName, H5P_DEFAULT);

    hid_t sectors = H5Aopen(volume, CFS_ATTR_SPACE_MAP, H5P_DEFAULT);
    hid_t dataSpace = H5Aget_space(sectors);
    hsize_t dimensions[2];
    H5Sget_simple_extent_dims(dataSpace, dimensions, NULL);

    int* data = (int*) malloc(sizeof(int) * dimensions[0] * dimensions[1]);
    H5Aread(sectors, H5T_NATIVE_INT, data);

    for (int i = 0; i < dimensions[0] * dimensions[1]; i++) {
        if (data[i] == 0) {
            sector = i;
            break;
        }
    }

    H5Aclose(sectors);
    H5Gclose(volume);

    return sector;
}

/**
 * Gets a file
 */
hid_t get_file(PathInfo* pathInfo, hid_t root) {
    FindDirInfo info;
    info.actualName = NULL;
    info.pathInfo = pathInfo;

    H5Literate(root, H5_INDEX_NAME, H5_ITER_NATIVE, NULL, find_file, (void*) &info);

    return H5Gopen(root, info.actualName, H5P_DEFAULT);
}

/**
 * Handles iteration to find a file
 */
herr_t find_file(hid_t loc, const char* name, const H5L_info_t* info, void* data) {
    FindDirInfo* dirInfo = (FindDirInfo*) data;
    char filename[512];
    char fileVolume[512];
    int nodeNumber;

    hid_t node = H5Gopen(loc, name, H5P_DEFAULT);

    if (name[0] == 'F') {
        // Get the name of the file
        hid_t attr = H5Aopen(node, CFS_ATTR_NAME, H5P_DEFAULT);
        hid_t fileType = H5Aget_type(attr);
        hid_t memType = H5Tget_native_type(fileType, H5T_DIR_ASCEND);
        H5Aread(attr, memType, filename);
        H5Tclose(fileType);
        H5Tclose(memType);
        H5Aclose(attr);

        // Get the inode of the file
        attr = H5Aopen(node, CFS_ATTR_INODE, H5P_DEFAULT);
        fileType = H5Aget_type(attr);
        memType = H5Tget_native_type(fileType, H5T_DIR_ASCEND);
        H5Aread(attr, memType, &nodeNumber);
        H5Tclose(fileType);
        H5Tclose(memType);
        H5Aclose(attr);

        // Get the name of the file volume for the directory
        attr = H5Aopen(node, CFS_ATTR_FV, H5P_DEFAULT);
        fileType = H5Aget_type(attr);
        memType = H5Tget_native_type(fileType, H5T_DIR_ASCEND);
        H5Aread(attr, memType, fileVolume);
        H5Tclose(fileType);
        H5Tclose(memType);
        H5Aclose(attr);

        // Check the file is in the ls'd directory
        char* dir = get_file_directory(nodeNumber, loc);
        char dirName[100];
        if (dir != NULL) {
            hid_t parentDir = H5Gopen(loc, dir, H5P_DEFAULT);
            attr = H5Aopen(parentDir, CFS_ATTR_NAME, H5P_DEFAULT);
            fileType = H5Aget_type(attr);
            memType = H5Tget_native_type(fileType, H5T_DIR_ASCEND);
            H5Aread(attr, memType, dirName);
            H5Tclose(fileType);
            H5Tclose(memType);
            H5Aclose(attr);
            H5Gclose(parentDir);
        }

        if (!s4354198_str_match(filename, dirInfo->pathInfo->target)) {
            H5Gclose(node);        
            free(dir);
            return 0;
        } else if (!s4354198_str_match(fileVolume, dirInfo->pathInfo->volume)) {
            H5Gclose(node);        
            free(dir);
            return 0;
        } else if ((dirInfo->pathInfo->directory != NULL && dir == NULL) 
                || (dirInfo->pathInfo->directory == NULL && dir != NULL)) {
            H5Gclose(node);        
            free(dir);
            return 0;
        } else if (dirInfo->pathInfo->directory != NULL && dir != NULL 
                && !s4354198_str_match(dirName, dirInfo->pathInfo->directory)) {
            H5Gclose(node);        
            free(dir);
            return 0;
        }
        free(dir);

        // We found the file
        dirInfo->actualName = strdup(name);
        H5Gclose(node);       
        return 1;
    }

    H5Gclose(node);       

    return 0;
}

/**
 * Gets the next available dir inode number
 */ 
int get_next_dir_inode(hid_t root) {
    int node = 0;

    H5Literate(root, H5_INDEX_NAME, H5_ITER_NATIVE, NULL, iterate_dir_count, (void*) &node);

    return node;
}

/**
 * Handles iteration over a directory
 */
herr_t iterate_dir_count(hid_t loc, const char* name, const H5L_info_t* info, void* data) {
    int* node = (int*) data;

    if (name[0] == 'D') {
        (*node)++;
    }

    return 0;
}

/**
 * Gets the next available file inode number
 */ 
int get_next_file_inode(hid_t root) {
    int node = 0;

    H5Literate(root, H5_INDEX_NAME, H5_ITER_NATIVE, NULL, iterate_file_count, (void*) &node);

    return node;
}

/**
 * Handles iteration over a directory
 */
herr_t iterate_file_count(hid_t loc, const char* name, const H5L_info_t* info, void* data) {
    int* node = (int*) data;

    if (name[0] == 'F') {
        (*node)++;
    }

    return 0;
}

/**
 * Check to see if a path already exists
 */ 
bool s4354198_path_taken(PathInfo* pathInfo, bool* byDir) {
    // If its a volume
    if (pathInfo->target == NULL && pathInfo->directory == NULL) {
        *byDir = true;
        return true;
    }

    hid_t file = wait_open_cfs();
    hid_t root = H5Gopen(file, CFS_ROOT, H5P_DEFAULT);

    herr_t err = H5Literate(root, H5_INDEX_NAME, H5_ITER_NATIVE, NULL, iterate_object_exists, (void*) pathInfo);
    
    H5Gclose(root);
    H5Fclose(file);

    *byDir = (err == 1);

    return pathInfo->exists;
}

/**
 * Handles iteration over an object to check for path existance
 */
herr_t iterate_object_exists(hid_t loc, const char* name, const H5L_info_t* info, void* data) {
    PathInfo* pathInfo = (PathInfo*) data;
    hid_t attr;
    hid_t fileType;
    hid_t memType;
    char filename[512];
    char fileVolume[512];
    int nodeNumber;

    hid_t node = H5Gopen(loc, name, H5P_DEFAULT);

    if (name[0] == 'F') {
        // Get the name of the file
        attr = H5Aopen(node, CFS_ATTR_NAME, H5P_DEFAULT);
        fileType = H5Aget_type(attr);
        memType = H5Tget_native_type(fileType, H5T_DIR_ASCEND);
        H5Aread(attr, memType, filename);
        H5Tclose(fileType);
        H5Tclose(memType);
        H5Aclose(attr);

        // Get the name of the file volume
        attr = H5Aopen(node, CFS_ATTR_FV, H5P_DEFAULT);
        fileType = H5Aget_type(attr);
        memType = H5Tget_native_type(fileType, H5T_DIR_ASCEND);
        H5Aread(attr, memType, fileVolume);
        H5Tclose(fileType);
        H5Tclose(memType);
        H5Aclose(attr);
        
        // Get the inode of the file
        attr = H5Aopen(node, CFS_ATTR_INODE, H5P_DEFAULT);
        fileType = H5Aget_type(attr);
        memType = H5Tget_native_type(fileType, H5T_DIR_ASCEND);
        H5Aread(attr, memType, &nodeNumber);
        H5Tclose(fileType);
        H5Tclose(memType);
        H5Aclose(attr);

        // Get the name of the parent directory
        char* parentNode = get_file_directory(nodeNumber, loc);
        char* parentNodeName = NULL;
        if (parentNode != NULL) {
            parentNodeName = get_dir_name_nice(loc, parentNode);
        }

        H5Gclose(node);

        if (s4354198_str_match(fileVolume, pathInfo->volume)) {
            if (pathInfo->directory == NULL && parentNodeName == NULL) {
                if (s4354198_str_match(filename, pathInfo->target)) {
                    pathInfo->exists = true;
                    free(parentNode);
                    free(parentNodeName);
                    // Short circuit end
                    return 2;
                }
            } else if (pathInfo->directory != NULL && parentNodeName != NULL) {
                if (s4354198_str_match(pathInfo->directory, parentNodeName)) {
                    pathInfo->exists = true;
                    free(parentNode);
                    free(parentNodeName);
                    return 2;
                }
            }
        } else {
            free(parentNode);
            free(parentNodeName);
        }
    } else if (name[0] == 'D') {
        // Get the name of the directory
        attr = H5Aopen(node, CFS_ATTR_NAME, H5P_DEFAULT);
        fileType = H5Aget_type(attr);
        memType = H5Tget_native_type(fileType, H5T_DIR_ASCEND);
        H5Aread(attr, memType, filename);
        H5Tclose(fileType);
        H5Tclose(memType);
        H5Aclose(attr);

        // Get the name of the file volume
        attr = H5Aopen(node, CFS_ATTR_FV, H5P_DEFAULT);
        fileType = H5Aget_type(attr);
        memType = H5Tget_native_type(fileType, H5T_DIR_ASCEND);
        H5Aread(attr, memType, fileVolume);
        H5Tclose(fileType);
        H5Tclose(memType);
        H5Aclose(attr);

        H5Gclose(node);

        if (s4354198_str_match(fileVolume, pathInfo->volume)) {
            if (pathInfo->directory == NULL) {
                if (s4354198_str_match(filename, pathInfo->target)) {
                    pathInfo->exists = true;
                    // Short circuit end
                    return 1;
                }
            } else {
                if (s4354198_str_match(filename, pathInfo->directory)) {
                    // Now check subfiles
                }
            }
        }
    }

    return 0;
}

/**
 * Actually makes the specified directory in the specified root and file volume
 */
void cfs_mkdir(hid_t root, PathInfo* pathInfo) {
    char node[16];
    sprintf(node, "%s%d", CFS_DINODE, get_next_dir_inode(root));

    // Create directory
    hid_t dir = H5Gcreate(root, node, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);

    // Add file volume name
    hid_t memType = H5Tcopy(H5T_C_S1);
    H5Tset_size(memType, strlen(pathInfo->volume) + 1);
    H5Tset_strpad(memType, H5T_STR_NULLTERM);
    hid_t space = H5Screate(H5S_SCALAR);
    hid_t attr = H5Acreate(dir, CFS_ATTR_FV, memType, space, H5P_DEFAULT, H5P_DEFAULT);
    H5Awrite(attr, memType, pathInfo->volume);
    H5Sclose(space);
    H5Tclose(memType);
    H5Aclose(attr);

    // Add directory name
    memType = H5Tcopy(H5T_C_S1);
    H5Tset_size(memType, strlen(pathInfo->target) + 1);
    H5Tset_strpad(memType, H5T_STR_NULLTERM);
    space = H5Screate(H5S_SCALAR);
    attr = H5Acreate(dir, CFS_ATTR_NAME, memType, space, H5P_DEFAULT, H5P_DEFAULT);
    H5Awrite(attr, memType, pathInfo->target);
    H5Sclose(space);
    H5Tclose(memType);
    H5Aclose(attr);
    
    // Add file count
    hsize_t dims[1] = {1};
    space = H5Screate_simple(1, dims, NULL);
    attr = H5Acreate(dir, CFS_ATTR_FILECOUNT, H5T_NATIVE_INT, space, H5P_DEFAULT, H5P_DEFAULT);
    int fileCount = 0;
    H5Awrite(attr, H5T_NATIVE_INT, &fileCount);
    H5Sclose(space);
    H5Aclose(attr);

    // Add timestamp
    struct timeval time;
    gettimeofday(&time, NULL);
    unsigned long long milli =
        (unsigned long long)(time.tv_sec) * 1000 +
        (unsigned long long)(time.tv_usec) / 1000;
    char timestamp[32];
    sprintf(timestamp, "%llu", milli);
    hid_t fileType = H5Tcopy(H5T_C_S1);
    H5Tset_size(fileType, strlen(timestamp) + 1);
    memType = H5Tcopy(H5T_C_S1);
    H5Tset_size(memType, strlen(timestamp) + 1);
    space = H5Screate_simple(1, dims, NULL);
    attr = H5Acreate(dir, CFS_ATTR_TIMESTAMP, fileType, space, H5P_DEFAULT, H5P_DEFAULT);
    H5Awrite(attr, memType, timestamp);
    H5Sclose(space);
    H5Tclose(fileType);
    H5Tclose(memType);
    H5Aclose(attr);

    // Create file inodes dataset
    hsize_t inodeDims[1] = {CFS_MAX_FILES};
    space = H5Screate_simple(1, inodeDims, NULL);
    hid_t properties = H5Pcreate(H5P_DATASET_CREATE);
    int fillValue = -1;
    H5Pset_fill_value(properties, H5T_STD_I32LE, &fillValue);
    hid_t dataSector = H5Dcreate(dir, CFS_ATTR_INODES, H5T_STD_I32LE, space,
        H5P_DEFAULT, properties, H5P_DEFAULT);
    H5Sclose(space);
    H5Dclose(dataSector);

    // Close the directory
    H5Gclose(dir);
}

/**
 * Makes the specified directory
 */
void s4354198_mkdir(PathInfo* pathInfo) {
    hid_t file = wait_open_cfs();//H5Fopen(app->cfs->filename, H5F_ACC_RDWR, H5P_DEFAULT);
    hid_t root = H5Gopen(file, CFS_ROOT, H5P_DEFAULT);

    cfs_mkdir(root, pathInfo);

    H5Gclose(root);
    H5Fclose(file);
}

/**
 * Gets the name of a directory
 */
char* get_dir_name_nice(hid_t root, char* nodeName) {
    char dirName[100];

    hid_t parentDir = H5Gopen(root, nodeName, H5P_DEFAULT);
    hid_t attr = H5Aopen(parentDir, CFS_ATTR_NAME, H5P_DEFAULT);
    hid_t fileType = H5Aget_type(attr);
    hid_t memType = H5Tget_native_type(fileType, H5T_DIR_ASCEND);
    H5Aread(attr, memType, dirName);
    H5Tclose(fileType);
    H5Tclose(memType);
    H5Aclose(attr);
    H5Gclose(parentDir);

    return strdup(dirName);
}

/**
 * Gets a dir's DInode name from the actual name
 */
char* get_dir_name(hid_t root, char* niceName) {
    FindDirInfo info;
    info.niceName = niceName;
    info.actualName = NULL;
    
    H5Literate(root, H5_INDEX_NAME, H5_ITER_NATIVE, NULL, iterate_find_dir_by_name, (void*) &info);

    return info.actualName;
}

/**
 * Handles iteration to find DInode name for a dir
 */
herr_t iterate_find_dir_by_name(hid_t loc, const char* name, const H5L_info_t* infoh5, void* data) {
    FindDirInfo* info = (FindDirInfo*) data;
    char filename[512];

    if (name[0] == 'F') {
        // Do nothing
    } else if (name[0] == 'D') {
        hid_t node = H5Gopen(loc, name, H5P_DEFAULT);
        
        // Get the name of the directory
        hid_t attr = H5Aopen(node, CFS_ATTR_NAME, H5P_DEFAULT);
        hid_t fileType = H5Aget_type(attr);
        hid_t memType = H5Tget_native_type(fileType, H5T_DIR_ASCEND);
        H5Aread(attr, memType, filename);
        H5Tclose(fileType);
        H5Tclose(memType);
        H5Aclose(attr);

        if (s4354198_str_match(filename, info->niceName)) {
            info->actualName = strdup(name);
            H5Gclose(node);
            return 1;
        }

        H5Gclose(node);
    }

    return 0;
}

/**
 * Actually makes the specified file in the specified directory and file volume
 */
void cfs_mkfile(hid_t root, PathInfo* pathInfo) {
    char node[16];
    int number = get_next_file_inode(root);
    sprintf(node, "%s%d", CFS_FINODE, number);

    // Create file
    hid_t file = H5Gcreate(root, node, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);

    // Add file volume name
    hid_t memType = H5Tcopy(H5T_C_S1);
    H5Tset_size(memType, strlen(pathInfo->volume) + 1);
    H5Tset_strpad(memType, H5T_STR_NULLTERM);
    hid_t space = H5Screate(H5S_SCALAR);
    hid_t attr = H5Acreate(file, CFS_ATTR_FV, memType, space, H5P_DEFAULT, H5P_DEFAULT);
    H5Awrite(attr, memType, pathInfo->volume);
    H5Sclose(space);
    H5Tclose(memType);
    H5Aclose(attr);

    // Add file name
    memType = H5Tcopy(H5T_C_S1);
    H5Tset_size(memType, strlen(pathInfo->target) + 1);
    H5Tset_strpad(memType, H5T_STR_NULLTERM);
    space = H5Screate(H5S_SCALAR);
    attr = H5Acreate(file, CFS_ATTR_NAME, memType, space, H5P_DEFAULT, H5P_DEFAULT);
    H5Awrite(attr, memType, pathInfo->target);
    H5Sclose(space);
    H5Tclose(memType);
    H5Aclose(attr);

    // Add file inode number
    hsize_t dims[1] = {1};
    space = H5Screate_simple(1, dims, NULL);
    attr = H5Acreate(file, CFS_ATTR_INODE, H5T_NATIVE_INT, space, H5P_DEFAULT, H5P_DEFAULT);
    H5Awrite(attr, H5T_NATIVE_INT, &number);
    H5Sclose(space);
    H5Aclose(attr);

    // Add mode
    memType = H5Tcopy(H5T_C_S1);
    H5Tset_size(memType, strlen(CFS_DEFAULT_MODE) + 1);
    H5Tset_strpad(memType, H5T_STR_NULLTERM);
    space = H5Screate(H5S_SCALAR);
    attr = H5Acreate(file, CFS_ATTR_MODE, memType, space, H5P_DEFAULT, H5P_DEFAULT);
    H5Awrite(attr, memType, CFS_DEFAULT_MODE);
    H5Sclose(space);
    H5Tclose(memType);
    H5Aclose(attr);
    
    // Add owner
    memType = H5Tcopy(H5T_C_S1);
    H5Tset_size(memType, strlen(CFS_OWNER) + 1);
    H5Tset_strpad(memType, H5T_STR_NULLTERM);
    space = H5Screate(H5S_SCALAR);
    attr = H5Acreate(file, CFS_ATTR_OWNER, memType, space, H5P_DEFAULT, H5P_DEFAULT);
    H5Awrite(attr, memType, CFS_OWNER);
    H5Sclose(space);
    H5Tclose(memType);
    H5Aclose(attr);

    // Add sector count
    space = H5Screate_simple(1, dims, NULL);
    attr = H5Acreate(file, CFS_ATTR_SECTOR_COUNT, H5T_NATIVE_INT, space, H5P_DEFAULT, H5P_DEFAULT);
    int sectorCount = 0;
    H5Awrite(attr, H5T_NATIVE_INT, &sectorCount);
    H5Sclose(space);
    H5Aclose(attr);
    
    // Add sector dataset
    hsize_t dimSec[2] = {CFS_VOLUME_SECTORS, 1};
    space = H5Screate_simple(2, dimSec, NULL);
    attr = H5Dcreate(file, CFS_ATTR_SECTORS, H5T_NATIVE_INT, space, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    int flatData[CFS_VOLUME_SECTORS];
    for (int i = 0; i < CFS_VOLUME_SECTORS; i++) {
        flatData[i] = -1;
    }
    H5Dwrite(attr, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT, &flatData);
    H5Sclose(space);
    H5Dclose(attr);

    // Add file to directory
    if (pathInfo->directory != NULL) {
        char* parentDirName = get_dir_name(root, pathInfo->directory);
        hid_t parentDir = H5Gopen(root, parentDirName, H5P_DEFAULT);
        int* parentInodes = get_dir_inodes(parentDir);
        for (int i = 0; i < CFS_MAX_FILES; i++) {
            if (parentInodes[i] == -1) {
                parentInodes[i] = number;
                break;
            }
        }
        set_dir_inodes(parentDir, parentInodes);
        free(parentInodes);
        free(parentDirName);
    }

    // Add timestamp
    struct timeval time;
    gettimeofday(&time, NULL);
    unsigned long long milli =
        (unsigned long long)(time.tv_sec) * 1000 +
        (unsigned long long)(time.tv_usec) / 1000;
    char timestamp[32];
    sprintf(timestamp, "%llu", milli);
    hid_t fileType = H5Tcopy(H5T_C_S1);
    H5Tset_size(fileType, strlen(timestamp) + 1);
    memType = H5Tcopy(H5T_C_S1);
    H5Tset_size(memType, strlen(timestamp) + 1);
    space = H5Screate_simple(1, dims, NULL);
    attr = H5Acreate(file, CFS_ATTR_TIMESTAMP, fileType, space, H5P_DEFAULT, H5P_DEFAULT);
    H5Awrite(attr, memType, timestamp);
    H5Sclose(space);
    H5Tclose(fileType);
    H5Tclose(memType);
    H5Aclose(attr);

    // Close the file
    H5Gclose(file);
}

/**
 * Makes the specified file
 */
void s4354198_mkfile(PathInfo* pathInfo) {
    hid_t file = wait_open_cfs();
    hid_t root = H5Gopen(file, CFS_ROOT, H5P_DEFAULT);

    cfs_mkfile(root, pathInfo);

    H5Gclose(root);
    H5Fclose(file);
}

/**
 * Formats a datetime string
 */
char* format_time(char* timestamp) {
    char* endToken;
    char* buffer = (char*) malloc(sizeof(char) * 80);
    unsigned long long ms = strtoull(timestamp, &endToken, 10);
    long int sec = ms / 1000;

    struct tm* tmInfo = localtime(&sec);
    strftime(buffer, 80, "%a %Y-%m-%d %H:%M:%S %Z", tmInfo);

    return buffer;
}

/**
 * Lists the specified directory
 */
INode* s4354198_ls(PathInfo* pathInfo) {
    INode* node = (INode*) malloc(sizeof(INode));
    node->name = NULL;
    node->info = pathInfo;
    node->next = NULL;
    node->prev = NULL;

    // Special case for /
    if (pathInfo->target != NULL && s4354198_str_match(pathInfo->target, "/")) {
        INode* last = NULL;
        INode* current = node;

        for (int i = 0; i < CFS_VOLUME_COUNT; i++) {
            current->next = NULL;
            current->prev = NULL;
            current->info = pathInfo;

            current->name = (char*) malloc(sizeof(char) * 100);
            sprintf(current->name, "%s%d", CFS_VOLUME, i);

            current->type = (char*) malloc(sizeof(char) * 100);
            sprintf(current->type, "%s", "VOLUME");

            current->sectors = (char*) malloc(sizeof(char) * 100);
            sprintf(current->sectors, "%d", CFS_VOLUME_SECTORS);

            current->timestamp = (char*) malloc(sizeof(char) * 100);
            char* timestamp = format_time("0");
            sprintf(current->timestamp, "%s", timestamp);
            free(timestamp);

            current->mode = (char*) malloc(sizeof(char) * 100);
            sprintf(current->mode, "%s", CFS_NO_DATA);

            current->owner = (char*) malloc(sizeof(char) * 100);
            sprintf(current->owner, "%s", CFS_OWNER);

            if (last != NULL) {
                last->next = current;
                current->prev = last;
            }

            last = current;
            current = (INode*) malloc(sizeof(INode));
        }

        return node;
    }
    
    hid_t file = wait_open_cfs();
    hid_t root = H5Gopen(file, CFS_ROOT, H5P_DEFAULT);

    H5Literate(root, H5_INDEX_NAME, H5_ITER_NATIVE, NULL, iterate_ls, (void*) node);

    H5Gclose(root);
    H5Fclose(file);

    return node;
}

/**
 * Gets the finodes from a dir
 */
int* get_dir_inodes(hid_t dir) {
    hid_t sectors = H5Dopen(dir, CFS_ATTR_INODES, H5P_DEFAULT);
    hid_t dataSpace = H5Dget_space(sectors);
    int rank = H5Sget_simple_extent_ndims(dataSpace);
    hsize_t dimensions[2];
    H5Sget_simple_extent_dims(dataSpace, dimensions, NULL);
    hsize_t offset[2] = {0, 0};
    H5Sselect_hyperslab(dataSpace, H5S_SELECT_SET, offset, NULL, dimensions, NULL);
    hid_t memSpace = H5Screate_simple(rank, dimensions, NULL);
    H5Sselect_hyperslab(memSpace, H5S_SELECT_SET, offset, NULL, dimensions, NULL);

    int* data = (int*) malloc(sizeof(int) * dimensions[0]);
    H5Dread(sectors, H5T_NATIVE_INT, memSpace, dataSpace, H5P_DEFAULT, data);

    H5Dclose(sectors);

    return data;
}

/**
 * Sets the finodes of a directory
 */
void set_dir_inodes(hid_t dir, int* inodes) {
    hid_t sectors = H5Dopen(dir, CFS_ATTR_INODES, H5P_DEFAULT);
    hid_t dataSpace = H5Dget_space(sectors);
    int rank = H5Sget_simple_extent_ndims(dataSpace);
    hsize_t dimensions[2];
    H5Sget_simple_extent_dims(dataSpace, dimensions, NULL);
    hsize_t offset[2] = {0, 0};
    H5Sselect_hyperslab(dataSpace, H5S_SELECT_SET, offset, NULL, dimensions, NULL);
    hid_t memSpace = H5Screate_simple(rank, dimensions, NULL);
    H5Sselect_hyperslab(memSpace, H5S_SELECT_SET, offset, NULL, dimensions, NULL);

    H5Dwrite(sectors, H5T_NATIVE_INT, memSpace, dataSpace, H5P_DEFAULT, inodes);

    H5Dclose(sectors);
}

/**
 * Handles iteration to find owner of a file
 */
herr_t iterate_find_dir(hid_t loc, const char* name, const H5L_info_t* infoh5, void* data) {
    FindDirInfo* info = (FindDirInfo*) data;

    if (name[0] == 'F') {
        // Do nothing
    } else if (name[0] == 'D') {
        hid_t node = H5Gopen(loc, name, H5P_DEFAULT);

        int* nodes = get_dir_inodes(node);

        for (int i = 0; i < CFS_MAX_FILES; i++) {
            if (nodes[i] == -1) {
                H5Gclose(node);
                return 0;
            } else {
                if (nodes[i] == info->inode) {
                    info->result = strdup(name);
                    H5Gclose(node);
                    return 1;
                }
            }
        }
        
        H5Gclose(node);
    }

    return 0;
}

/**
 * Gets the file's parent directory
 */
char* get_file_directory(int inode, hid_t root) {
    FindDirInfo info;
    info.result = NULL;
    info.inode = inode;

    H5Literate(root, H5_INDEX_NAME, H5_ITER_NATIVE, NULL, iterate_find_dir, (void*) &info);

    return info.result;
}

/**
 * Gets the sectors from a file
 */
int* get_file_sectors(hid_t file) {
    hid_t sectors = H5Dopen(file, CFS_ATTR_SECTORS, H5P_DEFAULT);
    hid_t dataSpace = H5Dget_space(sectors);
    int rank = H5Sget_simple_extent_ndims(dataSpace);
    hsize_t dimensions[2];
    H5Sget_simple_extent_dims(dataSpace, dimensions, NULL);
    hsize_t offset[2] = {0, 0};
    H5Sselect_hyperslab(dataSpace, H5S_SELECT_SET, offset, NULL, dimensions, NULL);
    hid_t memSpace = H5Screate_simple(rank, dimensions, NULL);
    H5Sselect_hyperslab(memSpace, H5S_SELECT_SET, offset, NULL, dimensions, NULL);

    int* data = (int*) malloc(sizeof(int) * dimensions[0] * dimensions[1]);
    H5Dread(sectors, H5T_NATIVE_INT, memSpace, dataSpace, H5P_DEFAULT, data);

    H5Dclose(sectors);

    return data;
}

/**
 * Counts the used sectors in a file
 */
int get_file_sector_count(int* sectors, int length) {
    int count = 0;

    for (int i = 0; i < length; i++) {
        if (sectors[i] >= 0) {
            count++;
        } else {
            break;
        }
    }

    return count;
}

/**
 * Handles iteration over a directory
 */
herr_t iterate_ls(hid_t loc, const char* name, const H5L_info_t* info, void* data) {
    INode* prev = (INode*) data;
    INode* last = (INode*) data;
    INode* inode;

    hid_t node = H5Gopen(loc, name, H5P_DEFAULT);

    hid_t attr;
    hid_t fileType;
    hid_t memType;
    char filename[512];
    char fileVolume[512];
    char timestamp[512];
    char mode[512];
    char sector[16];
    int nodeNumber;
    int sectors;

    if (prev->name == NULL) {
        inode = prev;
    } else {
        inode = (INode*) malloc(sizeof(INode));
    }

    // Find the last node
    while (last != NULL && last->next != NULL) {
        last = last->next;
    }

    // Make the node
    if (name[0] == 'F') {
        // Get the name of the file
        attr = H5Aopen(node, CFS_ATTR_NAME, H5P_DEFAULT);
        fileType = H5Aget_type(attr);
        memType = H5Tget_native_type(fileType, H5T_DIR_ASCEND);
        H5Aread(attr, memType, filename);
        H5Tclose(fileType);
        H5Tclose(memType);
        H5Aclose(attr);

        // Get the inode of the file
        attr = H5Aopen(node, CFS_ATTR_INODE, H5P_DEFAULT);
        fileType = H5Aget_type(attr);
        memType = H5Tget_native_type(fileType, H5T_DIR_ASCEND);
        H5Aread(attr, memType, &nodeNumber);
        H5Tclose(fileType);
        H5Tclose(memType);
        H5Aclose(attr);

        // Get the name of the file volume for the directory
        attr = H5Aopen(node, CFS_ATTR_FV, H5P_DEFAULT);
        fileType = H5Aget_type(attr);
        memType = H5Tget_native_type(fileType, H5T_DIR_ASCEND);
        H5Aread(attr, memType, fileVolume);
        H5Tclose(fileType);
        H5Tclose(memType);
        H5Aclose(attr);

        // Check the file is in the ls'd directory
        char* dir = get_file_directory(nodeNumber, loc);
        char dirName[100];
        if (dir != NULL) {
            hid_t parentDir = H5Gopen(loc, dir, H5P_DEFAULT);
            attr = H5Aopen(parentDir, CFS_ATTR_NAME, H5P_DEFAULT);
            fileType = H5Aget_type(attr);
            memType = H5Tget_native_type(fileType, H5T_DIR_ASCEND);
            H5Aread(attr, memType, dirName);
            H5Tclose(fileType);
            H5Tclose(memType);
            H5Aclose(attr);
            H5Gclose(parentDir);
        }

        if (!s4354198_str_match(fileVolume, prev->info->volume)) {
            H5Gclose(node);        
            free(dir);
            return 0;
        } else if ((prev->info->target != NULL && dir == NULL) 
                || (prev->info->target == NULL && dir != NULL)) {
            H5Gclose(node);        
            free(dir);
            return 0;
        } else if (prev->info->target != NULL && !s4354198_str_match(prev->info->target, dirName)) {
            H5Gclose(node);
            free(dir);
            return 0;
        }
        free(dir);

        // Get the mode of the file
        attr = H5Aopen(node, CFS_ATTR_MODE, H5P_DEFAULT);
        fileType = H5Aget_type(attr);
        memType = H5Tget_native_type(fileType, H5T_DIR_ASCEND);
        H5Aread(attr, memType, mode);
        H5Tclose(fileType);
        H5Tclose(memType);
        H5Aclose(attr);
        
        // Get the timestamp of the file
        attr = H5Aopen(node, CFS_ATTR_TIMESTAMP, H5P_DEFAULT);
        fileType = H5Aget_type(attr);
        memType = H5Tget_native_type(fileType, H5T_DIR_ASCEND);
        H5Aread(attr, memType, timestamp);
        H5Tclose(fileType);
        H5Tclose(memType);
        H5Aclose(attr);
        
        // Get the sectors of the file
        attr = H5Aopen(node, CFS_ATTR_SECTOR_COUNT, H5P_DEFAULT);
        fileType = H5Aget_type(attr);
        memType = H5Tget_native_type(fileType, H5T_DIR_ASCEND);
        H5Aread(attr, memType, &sectors);
        H5Tclose(fileType);
        H5Tclose(memType);
        H5Aclose(attr);
        sprintf(sector, "%d", sectors);

        // Get the timestamp
        char* timestampStr = format_time(timestamp);

        // Read folder
        inode->name = strdup(filename);
        inode->type = strdup("FILE");
        inode->sectors = strdup(sector);
        inode->timestamp = strdup(timestampStr);
        inode->mode = strdup(mode);
        inode->owner = strdup(CFS_OWNER);

        // Free stuff
        free(timestampStr);
    } else {
        // Get the name of the directory
        attr = H5Aopen(node, CFS_ATTR_NAME, H5P_DEFAULT);
        fileType = H5Aget_type(attr);
        memType = H5Tget_native_type(fileType, H5T_DIR_ASCEND);
        H5Aread(attr, memType, filename);
        H5Tclose(fileType);
        H5Tclose(memType);
        H5Aclose(attr);

        // Get the name of the file volume for the directory
        attr = H5Aopen(node, CFS_ATTR_FV, H5P_DEFAULT);
        fileType = H5Aget_type(attr);
        memType = H5Tget_native_type(fileType, H5T_DIR_ASCEND);
        H5Aread(attr, memType, fileVolume);
        H5Tclose(fileType);
        H5Tclose(memType);
        H5Aclose(attr);

        // Check the folder is in the ls'd directory
        if (!s4354198_str_match(fileVolume, prev->info->volume) 
                || prev->info->directory != NULL || prev->info->target != NULL) {
            H5Gclose(node);  
            return 0;
        }
        
        // Get the timestamp of the directory
        attr = H5Aopen(node, CFS_ATTR_TIMESTAMP, H5P_DEFAULT);
        fileType = H5Aget_type(attr);
        memType = H5Tget_native_type(fileType, H5T_DIR_ASCEND);
        H5Aread(attr, memType, timestamp);
        H5Tclose(fileType);
        H5Tclose(memType);
        H5Aclose(attr);

        // Get the timestamp
        char* timestampStr = format_time(timestamp);

        // Read folder
        inode->name = strdup(filename);
        inode->type = strdup("DIR");
        inode->sectors = strdup(CFS_NO_DATA);
        inode->timestamp = strdup(timestampStr);
        inode->mode = strdup(CFS_NO_DATA);
        inode->owner = strdup(CFS_OWNER);

        // Free stuff
        free(timestampStr);
    }

    // Add to linked list
    if (last != NULL) {
        last->next = inode;
        inode->prev = last;
    }
    inode->next = NULL;

    H5Gclose(node);

    return 0;
}

/**
 * Converts a cwd and a relative or absolute path into an absolute path
 */
char* s4354198_path_get_abs(char* cwd, char* path) {
    char* result = (char*) malloc(sizeof(char) * (strlen(path) + strlen(cwd) + 10));
    
    if (path[0] != '/') {
        if (cwd[strlen(cwd) - 1] != '/') {
            sprintf(result, "%s/%s", cwd, path);
        } else {
            sprintf(result, "%s%s", cwd, path);
        }
    } else {
        strcpy(result, path);
    }
    
    return result;
}

/**
 * Converts an absolute path to a struct
 */
bool s4354198_path(char* path, PathInfo** result, char** msg) {
    char* parts[6];
    int index = 0;
    *result = s4354198_path_info();

    path = &(path[1]);

    while (index < 5) {
        char* section = strsep(&path, "/");

        if (section == NULL) {
            break;
        }

        // File volume part
        if (index == 0) {
            // If the first section is empty then we have the root dir
            if (strlen(section) == 0) {
                index++;
                break;
            }
            if (section[0] != 'F' || section[1] != 'V') {
                *msg = strdup("Invalid volume name");
                return false;
            }
            for (int i = 0; i < strlen(&(section[2])); i++) {
                if (!isdigit(section[2+i])) {
                    *msg = strdup("Invalid volume specified");
                    return false;
                }
            }
            if (atoi(&(section[2])) >= CFS_VOLUME_COUNT) {
                *msg = strdup("Invalid volume number specified");
                return false;
            }

            parts[index++] = strdup(section);
        } else if (index == 2) {
            if (strlen(section) == 0) {
                break;
            }
            parts[index++] = strdup(section);
        } else if (index == 4) {
            if (strlen(section) == 0) {
                break;
            }
            parts[index++] = strdup(section);
        }

        // Path ends with '/' for a file, therefore invalid
        if (path != NULL && index > 4) {
            *msg = strdup("Path cannot specify a subdirectory");
            return false;
        } else if (path != NULL) {
            parts[index++] = strdup("/");
        } else if (strlen(section) > 0 && index < 2) {
            parts[index++] = strdup("/");
        }
    }

    // Even number of parts indicates ends with / so its a dir
    if (index % 2 == 0) {
        (*result)->isDir = true;
    }

    if (index == 4) {
        (*result)->volume = parts[0];
        (*result)->target = parts[2];
    } else if (index > 4) {
        (*result)->volume = parts[0];
        (*result)->directory = parts[2];
        (*result)->target = parts[4];
        (*result)->isDir = false;
    } else if (index > 2) {
        (*result)->volume = parts[0];
        (*result)->target = parts[2];
    } else if (index == 2) {
        (*result)->volume = parts[0];
    } else if (index == 1) {
        (*result)->target = strdup("/");
    } else {
        *msg = strdup("Invalid path");
        return false;
    }

    return true;
}

/**
 * Creates a new path info object
 */
PathInfo* s4354198_path_info() {
    PathInfo* info = (PathInfo*) malloc(sizeof(PathInfo));

    info->volume = NULL;
    info->directory = NULL;
    info->target = NULL;
    info->exists = false;
    info->isDir = false;

    return info;
}

/**
 * Frees a PathInfo struct
 */
void s4354198_path_free_info(PathInfo* pathInfo) {
    if (pathInfo->volume != NULL) {
        free(pathInfo->volume);
    }
    if (pathInfo->directory != NULL) {
        free(pathInfo->directory);
    }
    if (pathInfo->target != NULL) {
        free(pathInfo->target);
    }
    free(pathInfo);
}
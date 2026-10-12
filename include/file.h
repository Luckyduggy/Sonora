#ifndef FILE_H
#define FILE_H

#include "playlist.h"

int file_scan_directory(const char *path, Playlist *playlist);
int file_is_mp3(const char *path);
int is_valid_mp3_header(const char *path);

#endif
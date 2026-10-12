#include "../include/file.h"
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

int file_scan_directory(const char *path, Playlist *playlist){
    DIR *dir = opendir(path);
    if (dir == NULL) {
        printf("Error: can not open directory: %s\n", path);
        return 1;
    }

    struct dirent *current;
    while ((current = readdir(dir)) != NULL) {
        if (strcmp(current->d_name, ".") == 0 || strcmp(current->d_name, "..") == 0) {
            continue;
        }

        char fullPath[1024];
        int len = snprintf(fullPath, sizeof(fullPath), "%s/%s", path, current->d_name);
        if (len < 0 || (size_t)len >= sizeof(fullPath)) {
            printf("Error: path is too long: %s/%s\n", path, current->d_name);
            continue;
        }

        struct stat st;
        if (lstat(fullPath, &st) != 0) {
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            file_scan_directory(fullPath, playlist);
            continue;
        }

        if (S_ISLNK(st.st_mode) && stat(fullPath, &st) != 0) {
            continue;
        }

        if (!S_ISREG(st.st_mode) || !file_is_mp3(current->d_name)) {
            continue;
        }

        if (playlist_add(playlist, fullPath) != 0) {
            printf("Error: can not add %s\n", fullPath);
        }
    }

    closedir(dir);
    return 0;
}

int file_is_mp3(const char *path){
    const char *music = strrchr(path, '.');
    if (music == NULL) {
        return 0;
    }
    return strcasecmp(music, ".mp3") == 0;
}
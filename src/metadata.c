#include "../include/metadata.h"

#include <stdlib.h>
#include <string.h>

int metadata_load(Music *music, const char *path){
    if (music == NULL || path == NULL) {
        return 1;
    }

    TagLib_File *file = taglib_file_new(path);
    if (file == NULL) {
        return 1;
    }

    TagLib_Tag *tag = taglib_file_tag(file);
    if (tag == NULL) {
        taglib_file_free(file);
        return 1;
    }

    const char *raw_title = taglib_tag_title(tag);
    const char *raw_artist = taglib_tag_artist(tag);
    const char *raw_album = taglib_tag_album(tag);

    const char *title = (raw_title != NULL) ? raw_title : "";
    const char *artist = (raw_artist != NULL) ? raw_artist : "";
    const char *album = (raw_album != NULL) ? raw_album : "";

    music->title = malloc(strlen(title) + 1);
    music->artist = malloc(strlen(artist) + 1);
    music->album = malloc(strlen(album) + 1);
    music->path = malloc(strlen(path) + 1);

    if (music->title == NULL ||
        music->artist == NULL ||
        music->album == NULL ||
        music->path == NULL) {

        free(music->title);
        free(music->artist);
        free(music->album);
        free(music->path);

        taglib_tag_free_strings();
        taglib_file_free(file);

        return 1;
    }

    strcpy(music->title, title);
    strcpy(music->artist, artist);
    strcpy(music->album, album);
    strcpy(music->path, path);

    const TagLib_AudioProperties *properties = taglib_file_audioproperties(file);

    if (properties == NULL) {
        free(music->title);
        free(music->artist);
        free(music->album);
        free(music->path);

        taglib_tag_free_strings();
        taglib_file_free(file);

        return 1; 
    }

    music->duration = taglib_audioproperties_length(properties);

    taglib_tag_free_strings();
    taglib_file_free(file);

    return 0;
}
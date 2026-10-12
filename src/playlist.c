#include "../include/playlist.h"
#include "../include/metadata.h"

#include <stdlib.h>

static void music_free(Music *music)
{
    if (music == NULL) {
        return;
    }

    free(music->title);
    free(music->artist);
    free(music->album);
    free(music->path);

    free(music);
}

void playlist_init(Playlist *playlist)
{
    if (playlist == NULL) {
        return;
    }

    playlist->count = 0;
    playlist->capacity = 10;
    playlist->current = 0;

    playlist->music = malloc(playlist->capacity * sizeof(Music *));

    if (playlist->music == NULL) {
        playlist->capacity = 0;
    }
}

void playlist_free(Playlist *playlist)
{
    if (playlist == NULL) {
        return;
    }

    for (int i = 0; i < playlist->count; i++) {
        music_free(playlist->music[i]);
    }

    free(playlist->music);

    playlist->music = NULL;
    playlist->count = 0;
    playlist->capacity = 0;
    playlist->current = 0;
}

int playlist_add(Playlist *playlist, const char *path)
{
    if (playlist == NULL || path == NULL) {
        return 1;
    }

    if (playlist->count >= playlist->capacity) {

        int new_capacity;

        if (playlist->capacity == 0) {
            new_capacity = 10;
        } else {
            new_capacity = playlist->capacity * 2;
        }

        Music **new_music = realloc(playlist->music, new_capacity * sizeof(Music *));

        if (new_music == NULL) {
            return 1;
        }

        playlist->music = new_music;
        playlist->capacity = new_capacity;
    }

    Music *new_song = malloc(sizeof(Music));

    if (new_song == NULL) {
        return 1;
    }

    new_song->title = NULL;
    new_song->artist = NULL;
    new_song->album = NULL;
    new_song->path = NULL;
    new_song->duration = 0;

    if (metadata_load(new_song, path) != 0) {
        music_free(new_song);
        return 1;
    }

    playlist->music[playlist->count] = new_song;
    playlist->count++;

    return 0;
}

void playlist_next(Playlist *playlist)
{
    if (playlist == NULL || playlist->count == 0) {
        return;
    }

    playlist->current++;

    if (playlist->current >= playlist->count) {
        playlist->current = 0;
    }
}

void playlist_previous(Playlist *playlist)
{
    if (playlist == NULL || playlist->count == 0) {
        return;
    }

    if (playlist->current == 0) {
        playlist->current = playlist->count - 1;
    } else {
        playlist->current--;
    }
}

Music *playlist_current(const Playlist *playlist)
{
    if (playlist == NULL || playlist->count == 0) {
        return NULL;
    }

    return playlist->music[playlist->current];
}
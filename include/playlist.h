#ifndef PLAYLIST_H
#define PLAYLIST_H

typedef struct {
    char *title;
    char *artist;
    char *album;
    char *path;
    int duration;

} Music;

typedef struct {
    Music **music;
    int count;
    int capacity;
    int current;
} Playlist;

void playlist_init(Playlist *playlist);
void playlist_free(Playlist *playlist);

int playlist_add(Playlist *playlist, const char *path);

void playlist_next(Playlist *playlist);
void playlist_previous(Playlist *playlist);

Music *playlist_current(const Playlist *playlist);

#endif
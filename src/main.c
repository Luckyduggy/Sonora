#define _DEFAULT_SOURCE

#include "../include/audio.h"
#include "../include/file.h"
#include "../include/playlist.h"
#include "../include/ui.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define MAX_VOLUME 128
#define MAX_PATH_LENGTH 1024

enum {
    FOLDER_CANCELLED = 1,
    FOLDER_SCAN_FAILED = 2
};

static Playlist playlist;
static Playlist scanned_playlist;

static char **names = NULL;
static int started = 0;
static float volume = 0.7f;

static SDL_Thread *folder_thread = NULL;
static SDL_atomic_t folder_done;

static char selected_folder[MAX_PATH_LENGTH];
static int folder_result = FOLDER_CANCELLED;

static int cmp_music(const void *a, const void *b)
{
    const Music *x = *(Music *const *)a;
    const Music *y = *(Music *const *)b;

    return strcasecmp(x->path, y->path);
}

static void free_names(void)
{
    free(names);
    names = NULL;
}

static int build_names(void)
{
    free_names();

    if (playlist.count == 0) {
        return 0;
    }

    names = calloc(playlist.count, sizeof(*names));

    if (names == NULL) {
        return 1;
    }

    for (size_t i = 0; i < playlist.count; i++) {
        Music *song = playlist.music[i];

        if (song->title == NULL || song->title[0] == '\0') {
            const char *base = strrchr(song->path, '/');
            base = base != NULL ? base + 1 : song->path;

            char *fallback = strdup(base);

            if (fallback != NULL) {
                char *dot = strrchr(fallback, '.');

                if (dot != NULL && dot != fallback) {
                    *dot = '\0';
                }

                free(song->title);
                song->title = fallback;
            }
        }

        names[i] = song->title;
    }

    return 0;
}

static void set_volume(float value)
{
    if (value < 0.0f) {
        value = 0.0f;
    }

    if (value > 1.0f) {
        value = 1.0f;
    }

    volume = value;
    audio_set_volume((int)(volume * MAX_VOLUME));
}

static int play_track(int index)
{
    if (index < 0 || (size_t)index >= playlist.count) {
        return 1;
    }

    if (audio_load(playlist.music[index]->path) != 0) {
        return 1;
    }

    set_volume(volume);
    audio_play();

    playlist.current = index;
    started = 1;

    return 0;
}

static void toggle_play_pause(void)
{
    if (!started || !audio_is_playing()) {
        play_track(playlist.current);
    } else if (audio_is_paused()) {
        audio_resume();
    } else {
        audio_pause();
    }
}

static void next_track(void)
{
    int old = playlist.current;

    if (started) {
        playlist_next(&playlist);
    }

    if (play_track(playlist.current) != 0) {
        playlist.current = old;
    }
}

static void prev_track(void)
{
    if (started && audio_get_position() > 3.0) {
        audio_seek(0.0);
        return;
    }

    int old = playlist.current;

    if (started) {
        playlist_previous(&playlist);
    }

    if (play_track(playlist.current) != 0) {
        playlist.current = old;
    }
}

static int folder_dialog_worker(void *arg)
{
    (void)arg;

    char path[MAX_PATH_LENGTH] = {0};

    int result = ui_choose_folder(path, sizeof(path));

    if (result != 0 || path[0] == '\0') {
        folder_result = FOLDER_CANCELLED;
        goto finished;
    }

    snprintf(
        selected_folder,
        sizeof(selected_folder),
        "%s",
        path
    );

    if (file_scan_directory(path, &scanned_playlist) != 0) {
        fprintf(
            stderr,
            "[RESCAN] Cannot scan directory: %s\n",
            path
        );

        playlist_free(&scanned_playlist);
        playlist_init(&scanned_playlist);

        folder_result = FOLDER_SCAN_FAILED;
        goto finished;
    }

    if (scanned_playlist.count > 1) {
        qsort(
            scanned_playlist.music,
            scanned_playlist.count,
            sizeof(Music *),
            cmp_music
        );
    }

    folder_result = 0;

finished:
    SDL_AtomicSet(&folder_done, 1);

    return 0;
}

static void finish_folder_rescan(void)
{
    if (folder_thread == NULL ||
        SDL_AtomicGet(&folder_done) == 0) {
        return;
    }

    SDL_WaitThread(folder_thread, NULL);
    folder_thread = NULL;

    if (folder_result == FOLDER_CANCELLED) {
        fprintf(stderr, "[RESCAN] Folder selection cancelled\n");
        return;
    }

    if (folder_result == FOLDER_SCAN_FAILED) {
        fprintf(stderr, "[RESCAN] Folder scanning failed\n");
        return;
    }

    audio_stop();
    started = 0;

    free_names();
    playlist_free(&playlist);

    playlist = scanned_playlist;
    playlist_init(&scanned_playlist);

    playlist.current = -1;

    if (build_names() != 0) {
        fprintf(stderr, "[RESCAN] Cannot build track names\n");

        free_names();
        playlist_free(&playlist);
        playlist_init(&playlist);

        playlist.current = -1;
    }

    ui_playlist_changed();

    fprintf(
        stderr,
        "[RESCAN] Finished. Tracks: %zu\n",
        playlist.count
    );
}

static void handle_action(UiAction a)
{
    switch (a.type) {
    case UI_PLAY_PAUSE:
        toggle_play_pause();
        break;

    case UI_NEXT:
        next_track();
        break;

    case UI_PREV:
        prev_track();
        break;

    case UI_SELECT_TRACK:
        play_track(a.index);
        break;

    case UI_SEEK:
        if (started && audio_is_playing()) {
            audio_seek(a.value);
        }
        break;

    case UI_SET_VOLUME:
        set_volume((float)a.value);
        break;

    case UI_RESCAN:
        if (folder_thread != NULL) {
            break;
        }

        selected_folder[0] = '\0';
        folder_result = FOLDER_CANCELLED;

        SDL_AtomicSet(&folder_done, 0);

        folder_thread = SDL_CreateThread(
            folder_dialog_worker,
            "folder-rescan",
            NULL
        );

        if (folder_thread == NULL) {
            fprintf(
                stderr,
                "[RESCAN] Cannot start thread: %s\n",
                SDL_GetError()
            );
        }

        break;

    default:
        break;
    }
}

int main(int argc, char *argv[])
{
    char *default_folder = NULL;
    const char *folder = NULL;

    playlist_init(&playlist);
    playlist_init(&scanned_playlist);

    if (argc > 1) {
        folder = argv[1];
    } else {
        const char *home = getenv("HOME");

        if (home == NULL) {
            fprintf(stderr, "Error: HOME is not set\n");

            playlist_free(&playlist);
            playlist_free(&scanned_playlist);

            return 1;
        }

        size_t path_size = strlen(home) + sizeof("/Music");

        default_folder = malloc(path_size);

        if (default_folder == NULL) {
            fprintf(stderr, "Error: cannot allocate folder path\n");

            playlist_free(&playlist);
            playlist_free(&scanned_playlist);

            return 1;
        }

        snprintf(
            default_folder,
            path_size,
            "%s/Music",
            home
        );

        folder = default_folder;
    }

    if (file_scan_directory(folder, &playlist) != 0) {
        fprintf(stderr, "Error: cannot scan directory: %s\n", folder);

        free(default_folder);
        playlist_free(&playlist);
        playlist_free(&scanned_playlist);

        return 1;
    }

    free(default_folder);

    if (playlist.count > 1) {
        qsort(
            playlist.music,
            playlist.count,
            sizeof(Music *),
            cmp_music
        );
    }

    playlist.current = -1;

    if (build_names() != 0) {
        fprintf(stderr, "Error: cannot build track names\n");

        free_names();
        playlist_free(&playlist);
        playlist_free(&scanned_playlist);

        return 1;
    }

    if (ui_init() != 0) {
        fprintf(stderr, "Error: cannot initialize UI\n");

        free_names();
        playlist_free(&playlist);
        playlist_free(&scanned_playlist);

        return 1;
    }

    if (audio_init() != 0) {
        fprintf(stderr, "Error: cannot initialize audio\n");

        ui_quit();
        free_names();
        playlist_free(&playlist);
        playlist_free(&scanned_playlist);

        return 1;
    }

    set_volume(volume);

    int running = 1;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            handle_action(ui_handle_event(&event, &running));
        }

        finish_folder_rescan();

        if (started &&
            !audio_is_playing() &&
            !audio_is_paused() &&
            (size_t)(playlist.current + 1) < playlist.count) {
            playlist_next(&playlist);
            play_track(playlist.current);
        }

        UiState st;
        memset(&st, 0, sizeof(st));

        st.tracks = (const char *const *)names;
        st.track_count = playlist.count;
        st.current_track = started ? playlist.current : -1;
        st.is_playing =
            started && audio_is_playing() && !audio_is_paused();
        st.position = audio_get_position();
        st.volume = volume;

        if (started &&
            playlist.current >= 0 &&
            (size_t)playlist.current < playlist.count) {
            const Music *song = playlist.music[playlist.current];

            st.title = song->title;
            st.artist = song->artist;
            st.duration = (double)song->duration;
        }

        ui_draw(&st);

        SDL_Delay(10);
    }

    if (folder_thread != NULL) {
        SDL_WaitThread(folder_thread, NULL);
        folder_thread = NULL;
    }

    audio_quit();
    ui_quit();

    free_names();
    playlist_free(&playlist);
    playlist_free(&scanned_playlist);

    return 0;
}
#ifndef UI_H
#define UI_H

#include <SDL2/SDL.h>

typedef enum {
    UI_NONE,
    UI_PLAY_PAUSE,
    UI_NEXT,
    UI_PREV,
    UI_SELECT_TRACK,
    UI_SET_VOLUME,
    UI_SEEK,
    UI_RESCAN        
} UiActionType;

typedef struct {
    UiActionType type;
    double value;
    int index;
} UiAction;

typedef struct {
    const char *const *tracks;
    int track_count;
    int current_track;
    const char *title;
    const char *artist;
    double duration;
    double position;
    float volume;
    int is_playing;
} UiState;

int ui_init(void);
void ui_quit(void);
UiAction ui_handle_event(SDL_Event *event, int *running);
void ui_draw(const UiState *state);
int  ui_choose_folder(char *out, size_t out_size);
void ui_playlist_changed(void);

#endif

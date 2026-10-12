#ifndef AUDIO_H
#define AUDIO_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>

int audio_init(void);
void audio_quit(void);

void audio_play(void);
int audio_load(const char *path);
void audio_pause(void);
void audio_resume(void);
void audio_stop(void);

void audio_set_volume(int volume);

int audio_is_playing(void);
int audio_is_paused(void);
double audio_get_position(void);

void audio_seek(double seconds);

#endif
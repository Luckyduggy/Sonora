#include "../include/audio.h"
#include <stdio.h>

static Mix_Music *music = NULL;

int audio_init(void){
    if((Mix_Init(MIX_INIT_MP3) & MIX_INIT_MP3) != MIX_INIT_MP3){
        printf("Error: %s\n", Mix_GetError());
        return 1;
    }
    if(Mix_OpenAudio(
        MIX_DEFAULT_FREQUENCY,
        MIX_DEFAULT_FORMAT,
        MIX_DEFAULT_CHANNELS,
        2048) < 0){
        printf("Error: %s\n", Mix_GetError());
        Mix_Quit();
        return 1;
    }
    return 0;
}

void audio_quit(void){
    if (music != NULL) {
        Mix_FreeMusic(music);
        music = NULL;
    }

    Mix_CloseAudio();
    Mix_Quit();
}

int audio_load(const char *path){
    Mix_Music *new_music = Mix_LoadMUS(path);

    if (new_music == NULL) {
        printf("Error: %s\n", Mix_GetError());
        return 1;
    }

    if (music != NULL) {
        Mix_FreeMusic(music);
    }

    music = new_music;

    return 0;
}

void audio_play(void){
    if(music == NULL){
        return;
    }
    Mix_PlayMusic(music, 0);
}

void audio_pause(void){
    if(music == NULL){
        return;
    }
    Mix_PauseMusic();
}

void audio_resume(void){
    if(music == NULL){
        return;
    }
    Mix_ResumeMusic();
}

void audio_stop(void){
    if(music == NULL){
        return;
    }
    Mix_HaltMusic();
}

void audio_set_volume(int volume){
    if(music == NULL){
        return;
    }
    Mix_VolumeMusic(volume);
}

int audio_is_playing(void){
    return Mix_PlayingMusic();
}

int audio_is_paused(void){
    return Mix_PausedMusic();
}

double audio_get_position(void){
    if(music == NULL){
        return 0.0;
    }
    double position = Mix_GetMusicPosition(music);
    return position < 0.0 ? 0.0 : position;
}

void audio_seek(double seconds){
    if(music == NULL){
        return;
    }
    Mix_SetMusicPosition(seconds);
}
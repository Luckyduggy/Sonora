#ifndef METADATA_H
#define METADATA_H

#include "playlist.h"
#include <taglib/tag_c.h>

int metadata_load(Music *music, const char *path);

#endif
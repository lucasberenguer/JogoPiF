#ifndef BOSS_MUSIC_H
#define BOSS_MUSIC_H

#include "game_audio.h"

const char *BossThemeName(GameTrack track);
Music LoadBossTheme(GameTrack track, unsigned char **data);

#endif

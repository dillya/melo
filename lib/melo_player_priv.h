/*
 * Copyright (C) 2020 Alexandre Dilly <dillya@sparod.com>
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA.
 */

#ifndef _MELO_PLAYER_PRIV_H_
#define _MELO_PLAYER_PRIV_H_

#include "melo/melo_player.h"

void melo_player_settings_init (void);
void melo_player_settings_deinit (void);

bool melo_player_play_media (
    const char *id, const char *path, const char *name, MeloTags *tags);

void melo_player_update_playlist_controls (bool prev, bool next);

#endif /* !_MELO_PLAYER_PRIV_H_ */

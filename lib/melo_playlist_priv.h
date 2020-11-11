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

#ifndef _MELO_PLAYLIST_PRIV_H_
#define _MELO_PLAYLIST_PRIV_H_

#include "melo/melo_playlist.h"

void melo_playlist_load_playlists (void);
void melo_playlist_unload_playlists (void);

bool melo_playlist_play_previous (void);
bool melo_playlist_play_next (void);

#endif /* !_MELO_PLAYLIST_PRIV_H_ */

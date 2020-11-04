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

#ifndef _MELO_PLAYLIST_H_
#define _MELO_PLAYLIST_H_

#include <glib-object.h>

#include <melo/melo_async.h>
#include <melo/melo_tags.h>

G_BEGIN_DECLS

typedef struct _MeloPlaylistEntry MeloPlaylistEntry;

/**
 * MeloPlaylist:
 */

#define MELO_TYPE_PLAYLIST melo_playlist_get_type ()
G_DECLARE_FINAL_TYPE (MeloPlaylist, melo_playlist, MELO, PLAYLIST, GObject)

MeloPlaylist *melo_playlist_new ();

bool melo_playlist_add_event_listener (
    const char *id, MeloAsyncCb cb, void *user_data);
bool melo_playlist_remove_event_listener (
    const char *id, MeloAsyncCb cb, void *user_data);

bool melo_playlist_handle_request (
    const char *id, MeloMessage *msg, MeloAsyncCb cb, void *user_data);
void melo_playlist_cancel_request (
    const char *id, MeloAsyncCb cb, void *user_data);

bool melo_playlist_add_media (
    const char *player_id, const char *path, const char *name, MeloTags *tags);
bool melo_playlist_play_media (
    const char *player_id, const char *path, const char *name, MeloTags *tags);

bool melo_playlist_add_entry (MeloPlaylistEntry *entry);
bool melo_playlist_play_entry (MeloPlaylistEntry *entry);

MeloPlaylistEntry *melo_playlist_entry_new (
    const char *player_id, const char *path, const char *name, MeloTags *tags);
MeloPlaylistEntry *melo_playlist_entry_ref (MeloPlaylistEntry *entry);
void melo_playlist_entry_unref (MeloPlaylistEntry *entry);

MeloPlaylistEntry *melo_playlist_entry_get_parent (MeloPlaylistEntry *entry);
bool melo_playlist_entry_has_player (MeloPlaylistEntry *entry);

bool melo_playlist_entry_update (
    MeloPlaylistEntry *entry, const char *name, MeloTags *tags, bool reset);

bool melo_playlist_entry_add_media (MeloPlaylistEntry *entry,
    const char *player_id, const char *path, const char *name, MeloTags *tags,
    MeloPlaylistEntry **ref);

G_END_DECLS

#endif /* !_MELO_PLAYLIST_H_ */

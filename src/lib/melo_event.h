/*
 * melo_event.h: Event dispatcher
 *
 * Copyright (C) 2017 Alexandre Dilly <dillya@sparod.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifndef __MELO_EVENT_H__
#define __MELO_EVENT_H__

#include <glib.h>
#include <json-glib/json-glib.h>

#include "melo_player.h"

typedef enum _MeloEventType MeloEventType;
typedef struct _MeloEventClient MeloEventClient;

typedef enum _MeloEventPlayer MeloEventPlayer;

enum _MeloEventType {
  MELO_EVENT_TYPE_GENERAL = 0,
  MELO_EVENT_TYPE_MODULE,
  MELO_EVENT_TYPE_BROWSER,
  MELO_EVENT_TYPE_PLAYER,
  MELO_EVENT_TYPE_PLAYLIST,

  MELO_EVENT_TYPE_COUNT
};

enum _MeloEventPlayer {
  MELO_EVENT_PLAYER_NEW = 0,
  MELO_EVENT_PLAYER_DELETE,
  MELO_EVENT_PLAYER_STATUS,
  MELO_EVENT_PLAYER_STATE,
  MELO_EVENT_PLAYER_BUFFERING,
  MELO_EVENT_PLAYER_SEEK,
  MELO_EVENT_PLAYER_DURATION,
  MELO_EVENT_PLAYER_PLAYLIST,
  MELO_EVENT_PLAYER_VOLUME,
  MELO_EVENT_PLAYER_MUTE,
  MELO_EVENT_PLAYER_NAME,
  MELO_EVENT_PLAYER_ERROR,
  MELO_EVENT_PLAYER_TAGS,

  MELO_EVENT_PLAYER_COUNT,
};

typedef gboolean (*MeloEventCallback) (MeloEventClient *client,
                                       MeloEventType type, guint event,
                                       const gchar *id, gpointer data,
                                       gpointer user_data);

/* Event client registration */
MeloEventClient *melo_event_register (MeloEventCallback callback,
                                      gpointer user_data);
void melo_event_unregister (MeloEventClient *client);

/* Event generation */
void melo_event_new (MeloEventType type, guint event, const gchar *id,
                     gpointer data, GDestroyNotify free_data_func);

/* Event helper */
const gchar *melo_event_type_to_string (MeloEventType type);

/* Player event helpers */
inline void melo_event_player_new (const gchar *id, const MeloPlayerInfo *info);
inline void melo_event_player_delete (const gchar *id);
inline void melo_event_player_status (const gchar *id,
                                      MeloPlayerStatus *status);
inline void melo_event_player_state (const gchar *id, MeloPlayerState state);
inline void melo_event_player_buffering (const gchar *id, MeloPlayerState state,
                                         guint percent);
inline void melo_event_player_seek (const gchar *id, gint pos);
inline void melo_event_player_duration (const gchar *id, gint duration);
inline void melo_event_player_playlist (const gchar *id, gboolean has_prev,
                                        gboolean has_next);
inline void melo_event_player_volume (const gchar *id, gdouble volume);
inline void melo_event_player_mute (const gchar *id, gboolean mute);
inline void melo_event_player_name (const gchar *id, const gchar *name);
inline void melo_event_player_error (const gchar *id, const gchar *error);
inline void melo_event_player_tags (const gchar *id, MeloTags *tags);

inline const MeloPlayerInfo *melo_event_player_new_parse (gpointer data);
inline MeloPlayerStatus *melo_event_player_status_parse (gpointer data);
inline MeloPlayerState melo_event_player_state_parse (gpointer data);
inline void melo_event_player_buffering_parse (gpointer data,
                                               MeloPlayerState *state,
                                               guint *percent);
inline gint melo_event_player_seek_parse (gpointer data);
inline gint melo_event_player_duration_parse (gpointer data);
inline void melo_event_player_playlist_parse (gpointer data, gboolean *has_prev,
                                              gboolean *has_next);
inline gdouble melo_event_player_volume_parse (gpointer data);
inline gboolean melo_event_player_mute_parse (gpointer data);
inline const gchar *melo_event_player_name_parse (gpointer data);
inline const gchar *melo_event_player_error_parse (gpointer data);
inline MeloTags *melo_event_player_tags_parse (gpointer data);

const gchar *melo_event_player_to_string (MeloEventPlayer event);

#endif /* __MELO_EVENT_H__ */

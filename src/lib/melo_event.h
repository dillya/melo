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

/**
 * MeloEventType:
 * @MELO_EVENT_TYPE_GENERAL: a general event (from anywhere)
 * @MELO_EVENT_TYPE_MODULE: a module event (from #MeloModule)
 * @MELO_EVENT_TYPE_BROWSER: a browser event (from #MeloBrowser)
 * @MELO_EVENT_TYPE_PLAYER: a player event (from #MeloPlayer)
 * @MELO_EVENT_TYPE_PLAYLIST: a playlist event (from #MeloPlaylist)
 *
 * The #MeloEventType presents the source of an event. For custom or global
 * events, please use @MELO_EVENT_TYPE_GENERAL.
 */
enum _MeloEventType {
  MELO_EVENT_TYPE_GENERAL = 0,
  MELO_EVENT_TYPE_MODULE,
  MELO_EVENT_TYPE_BROWSER,
  MELO_EVENT_TYPE_PLAYER,
  MELO_EVENT_TYPE_PLAYLIST,

  /*< private >*/
  MELO_EVENT_TYPE_COUNT
};

/**
 * MeloEventPlayer:
 * @MELO_EVENT_PLAYER_NEW: a new player has been created
 * @MELO_EVENT_PLAYER_DELETE: a plater has been destroyed
 * @MELO_EVENT_PLAYER_STATUS: the status of a player has been updated
 * @MELO_EVENT_PLAYER_STATE: the state of a player has been updated
 * @MELO_EVENT_PLAYER_BUFFERING: the buffering state has been updated
 * @MELO_EVENT_PLAYER_SEEK: a seek has been done on the player
 * @MELO_EVENT_PLAYER_DURATION: the duration has been updated on the player
 * @MELO_EVENT_PLAYER_PLAYLIST: an update has been done in the playlist
 * @MELO_EVENT_PLAYER_VOLUME: the volume has changed in the player
 * @MELO_EVENT_PLAYER_MUTE: the mute has changed in the player
 * @MELO_EVENT_PLAYER_NAME: the status name of the player has changed
 * @MELO_EVENT_PLAYER_ERROR: an error occurred in the player
 * @MELO_EVENT_PLAYER_TAGS: the tags has been updated in the player
 *
 * The #MeloEventPlayer describes the sub-type for an event coming from a
 * #MeloPlayer instance. For each types, a function is available to parse it.
 */
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

  /*< private >*/
  MELO_EVENT_PLAYER_COUNT,
};

/**
 * MeloEventCallback:
 * @client: the current client instance
 * @type: the event type
 * @event: the sub-type of the event
 * @id: the Melo object ID
 * @data: the event data
 * @user_data: the user data associated to the callback
 *
 * This callback is called when a new event is emitted by one of the Melo
 * objects. The event type is provided by @type and according to this value, the
 * correct sub-type held in @event should be selected: for a
 * #MELO_EVENT_TYPE_PLAYER, you should use #MeloEventPlayer.
 *
 * This callback during client creation with melo_event_register().
 *
 * Note: this callback is not threaded and long operation or blocking calls
 * should be avoided!
 *
 * Returns: %TRUE if the event has been handled successfully, %FALSE otherwise.
 */
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
void melo_event_player_new (const gchar *id, const MeloPlayerInfo *info);
void melo_event_player_delete (const gchar *id);
void melo_event_player_status (const gchar *id, MeloPlayerStatus *status);
void melo_event_player_state (const gchar *id, MeloPlayerState state);
void melo_event_player_buffering (const gchar *id, MeloPlayerState state,
                                  guint percent);
void melo_event_player_seek (const gchar *id, gint pos);
void melo_event_player_duration (const gchar *id, gint duration);
void melo_event_player_playlist (const gchar *id, gboolean has_prev,
                                 gboolean has_next);
void melo_event_player_volume (const gchar *id, gdouble volume);
void melo_event_player_mute (const gchar *id, gboolean mute);
void melo_event_player_name (const gchar *id, const gchar *name);
void melo_event_player_error (const gchar *id, const gchar *error);
void melo_event_player_tags (const gchar *id, MeloTags *tags);

const MeloPlayerInfo *melo_event_player_new_parse (gpointer data);
MeloPlayerStatus *melo_event_player_status_parse (gpointer data);
MeloPlayerState melo_event_player_state_parse (gpointer data);
void melo_event_player_buffering_parse (gpointer data, MeloPlayerState *state,
                                        guint *percent);
gint melo_event_player_seek_parse (gpointer data);
gint melo_event_player_duration_parse (gpointer data);
void melo_event_player_playlist_parse (gpointer data, gboolean *has_prev,
                                       gboolean *has_next);
gdouble melo_event_player_volume_parse (gpointer data);
gboolean melo_event_player_mute_parse (gpointer data);
const gchar *melo_event_player_name_parse (gpointer data);
const gchar *melo_event_player_error_parse (gpointer data);
MeloTags *melo_event_player_tags_parse (gpointer data);

const gchar *melo_event_player_to_string (MeloEventPlayer event);

#endif /* __MELO_EVENT_H__ */

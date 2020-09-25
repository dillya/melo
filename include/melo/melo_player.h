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

#ifndef _MELO_PLAYER_H_
#define _MELO_PLAYER_H_

#include <glib-object.h>
#include <gst/gst.h>

#include <melo/melo_async.h>
#include <melo/melo_settings.h>
#include <melo/melo_tags.h>

G_BEGIN_DECLS

typedef enum _MeloPlayerState MeloPlayerState;
typedef enum _MeloPlayerStreamState MeloPlayerStreamState;

/**
 * MeloPlayer:
 */

#define MELO_TYPE_PLAYER melo_player_get_type ()
G_DECLARE_DERIVABLE_TYPE (MeloPlayer, melo_player, MELO, PLAYER, GObject)

/**
 * MeloPlayerClass:
 * @parent_class: #GObject parent class
 * @play: This function is called when a new media should be played
 * @set_state: This function is called to change state (playing / paused /
 *     stopped)
 * @set_position: This function is called to seek in media
 * @get_position: This function is called to get the current position in media
 * @get_asset: This function is called to get an URI for a specific asset
 *     identified by its ID
 * @settings: This function is called when settings have been created and should
 *     be populated
 *
 * The class structure for a #MeloPlayer object.
 */
struct _MeloPlayerClass {
  GObjectClass parent_class;

  bool (*play) (MeloPlayer *player, const char *path);
  bool (*set_state) (MeloPlayer *player, MeloPlayerState state);
  bool (*set_position) (MeloPlayer *player, unsigned int position);
  unsigned int (*get_position) (MeloPlayer *player);
  char *(*get_asset) (MeloPlayer *player, const char *id);
  void (*settings) (MeloPlayer *player, MeloSettings *settings);
};

/**
 * MELO_DECLARE_PLAYER:
 * @TN: the type name as MeloCustomNamePlayer
 * @t_n: the type name as melo_custom_name_player
 * @ON: the object name as CUSTOM_NAME_PLAYER
 *
 * An helper to declare a player, to use in the header source file as (for a
 * class named "custom"):
 * |[<!-- language="C" -->
 * struct _MeloCustomPlayer {
 *   GObject parent_instance;
 * };
 *
 * MELO_DEFINE_PLAYER (MeloCustomPlayer, melo_custom_player)
 * ]|
 */
#define MELO_DECLARE_PLAYER(TN, t_n, ON) \
  G_DECLARE_FINAL_TYPE (TN, t_n, MELO, ON, MeloPlayer)

/**
 * MELO_DEFINE_PLAYER:
 * @TN: the type name as MeloCustomNamePlayer
 * @t_n: the type name as melo_custom_name_player
 *
 * An helper to define a player, to use in the C source file as (for a class
 * named "custom"):
 * |[<!-- language="C" -->
 * #define MELO_TYPE_CUSTOM_PLAYER melo_custom_player_get_type ()
 * MELO_DECLARE_PLAYER (MeloCustomPlayer, melo_custom_player, CUSTOM_PLAYER)
 * ]|
 */
#define MELO_DEFINE_PLAYER(TN, t_n) G_DEFINE_TYPE (TN, t_n, MELO_TYPE_PLAYER)

/**
 * MeloPlayerState:
 * @MELO_PLAYER_STATE_NONE: No loaded media
 * @MELO_PLAYER_STATE_PLAYING: Media is playing
 * @MELO_PLAYER_STATE_PAUSED: Media is paused
 * @MELO_PLAYER_STATE_STOPPED: Media is stopped
 *
 * #MeloPlayerState indicates the current state of a #MeloPlayer.
 */
enum _MeloPlayerState {
  MELO_PLAYER_STATE_NONE,
  MELO_PLAYER_STATE_PLAYING,
  MELO_PLAYER_STATE_PAUSED,
  MELO_PLAYER_STATE_STOPPED,
};

/**
 * MeloPlayerStreamState:
 * @MELO_PLAYER_STREAM_STATE_NONE: Normal playing
 * @MELO_PLAYER_STREAM_STATE_LOADING: Player is loading: the stream is not ready
 *    to be player
 * @MELO_PLAYER_STREAM_STATE_BUFFERING: Player is buffering: the stream is
 *    paused while player is grabbing data to play more samples
 *
 * #MeloPlayerStreamState indicates the current state of the stream of a
 * #MeloPlayer. It is used to know if the player is loading or buffering when
 * playing a remote or live media.
 */
enum _MeloPlayerStreamState {
  MELO_PLAYER_STREAM_STATE_NONE,
  MELO_PLAYER_STREAM_STATE_LOADING,
  MELO_PLAYER_STREAM_STATE_BUFFERING,
};

bool melo_player_add_event_listener (MeloAsyncCb cb, void *user_data);
bool melo_player_remove_event_listener (MeloAsyncCb cb, void *user_data);

bool melo_player_handle_request (
    MeloMessage *msg, MeloAsyncCb cb, void *user_data);

char *melo_player_get_asset (const char *id, const char *asset);

GstElement *melo_player_get_sink (MeloPlayer *player, const char *name);

void melo_player_update_media (MeloPlayer *player, const char *name,
    MeloTags *tags, unsigned int merge_flags);
void melo_player_update_tags (
    MeloPlayer *player, MeloTags *tags, unsigned int merge_flags);
void melo_player_update_status (MeloPlayer *player, MeloPlayerState state,
    MeloPlayerStreamState stream_state, unsigned int stream_state_value);
void melo_player_update_state (MeloPlayer *player, MeloPlayerState state);
void melo_player_update_stream_state (MeloPlayer *player,
    MeloPlayerStreamState stream_state, unsigned int stream_state_value);
void melo_player_update_position (MeloPlayer *player, unsigned int position);
void melo_player_update_duration (
    MeloPlayer *player, unsigned int position, unsigned int duration);
void melo_player_update_volume (MeloPlayer *player, float volume, bool mute);
void melo_player_eos (MeloPlayer *player);
void melo_player_error (MeloPlayer *player, const char *error);

G_END_DECLS

#endif /* !_MELO_PLAYER_H_ */

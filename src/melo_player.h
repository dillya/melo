/*
 * melo_player.h: Player base class
 *
 * Copyright (C) 2016 Alexandre Dilly <dillya@sparod.com>
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

#ifndef __MELO_PLAYER_H__
#define __MELO_PLAYER_H__

#include <glib-object.h>

#include "melo_playlist.h"
#include "melo_tags.h"

G_BEGIN_DECLS

#define MELO_TYPE_PLAYER             (melo_player_get_type ())
#define MELO_PLAYER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), MELO_TYPE_PLAYER, MeloPlayer))
#define MELO_IS_PLAYER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MELO_TYPE_PLAYER))
#define MELO_PLAYER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), MELO_TYPE_PLAYER, MeloPlayerClass))
#define MELO_IS_PLAYER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), MELO_TYPE_PLAYER))
#define MELO_PLAYER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), MELO_TYPE_PLAYER, MeloPlayerClass))

typedef struct _MeloPlayer MeloPlayer;
typedef struct _MeloPlayerClass MeloPlayerClass;
typedef struct _MeloPlayerPrivate MeloPlayerPrivate;

typedef struct _MeloPlayerInfo MeloPlayerInfo;
typedef struct _MeloPlayerStatus MeloPlayerStatus;

typedef enum {
  MELO_PLAYER_STATE_NONE,
  MELO_PLAYER_STATE_PLAYING,
  MELO_PLAYER_STATE_PAUSED,
  MELO_PLAYER_STATE_STOPPED,
  MELO_PLAYER_STATE_ERROR,

  MELO_PLAYER_STATE_COUNT,
} MeloPlayerState;

struct _MeloPlayer {
  GObject parent_instance;

  /*< protected >*/
  MeloPlaylist *playlist;

  /*< private >*/
  MeloPlayerPrivate *priv;
};

struct _MeloPlayerClass {
  GObjectClass parent_class;

  /* Player infos */
  const MeloPlayerInfo *(*get_info) (MeloPlayer *player);

  /* Control callbacks */
  gboolean (*play) (MeloPlayer *player, const gchar *path);
  MeloPlayerState (*set_state) (MeloPlayer *player, MeloPlayerState state);
  gint (*set_pos) (MeloPlayer *player, gint pos);

  /* Status callbacks */
  MeloPlayerState (*get_state) (MeloPlayer *player);
  gchar *(*get_name) (MeloPlayer *player);
  gint (*get_pos) (MeloPlayer *player, gint *duration);
  MeloPlayerStatus *(*get_status) (MeloPlayer *player);
};

struct _MeloPlayerInfo {
  const gchar *playlist_id;
};

struct _MeloPlayerStatus {
  MeloPlayerState state;
  gchar *error;
  gchar *name;
  gint pos;
  gint duration;
  MeloTags *tags;

  gint ref_count;
};

GType melo_player_get_type (void);

MeloPlayer *melo_player_new (GType type, const gchar *id);
const gchar *melo_player_get_id (MeloPlayer *player);
const MeloPlayerInfo *melo_player_get_info (MeloPlayer *player);
MeloPlayer *melo_player_get_player_by_id (const gchar *id);

/* Playlist */
void melo_player_set_playlist (MeloPlayer *player, MeloPlaylist *playlist);
MeloPlaylist *melo_player_get_playlist (MeloPlayer *player);
gboolean melo_player_add (MeloPlayer *player, const gchar *name,
                          const gchar *full_name, const gchar *path,
                          gboolean is_current);

/* Player control */
gboolean melo_player_play (MeloPlayer *player, const gchar *path);
MeloPlayerState melo_player_set_state (MeloPlayer *player,
                                       MeloPlayerState state);
gint melo_player_set_pos (MeloPlayer *player, gint pos);

/* Player status */
MeloPlayerState melo_player_get_state (MeloPlayer *player);
gchar *melo_player_get_name (MeloPlayer *player);
gint melo_player_get_pos (MeloPlayer *player, gint *duration);
MeloPlayerStatus *melo_player_get_status (MeloPlayer *player);

/* MeloPlayerStatus helpers */
MeloPlayerStatus *melo_player_status_new (MeloPlayerState state,
                                          const gchar *name);
MeloPlayerStatus *melo_player_status_ref (MeloPlayerStatus *status);
void melo_player_status_unref (MeloPlayerStatus *status);
void melo_player_status_set_tags (MeloPlayerStatus *status, MeloTags *tags);
void melo_player_status_take_tags (MeloPlayerStatus *status, MeloTags *tags);

/* MeloPlayerState helpers */
const gchar *melo_player_state_to_string (MeloPlayerState state);
MeloPlayerState melo_player_state_from_string (const gchar *sstate);

G_END_DECLS

#endif /* __MELO_PLAYER_H__ */

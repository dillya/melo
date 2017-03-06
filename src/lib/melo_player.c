/*
 * melo_player.c: Player base class
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

#include "melo_event.h"
#include "melo_player.h"

/* Internal player list */
G_LOCK_DEFINE_STATIC (melo_player_mutex);
static GHashTable *melo_player_hash = NULL;
static GList *melo_player_list = NULL;

struct _MeloPlayerStatusPrivate {
  GMutex mutex;
  gint ref_count;

  /* Strings */
  gchar *id;
  gchar *name;
  gchar *error;

  /* Tags */
  MeloTags *tags;
};

struct _MeloPlayerPrivate {
  gchar *id;
  gchar *name;
  MeloPlayerInfo info;
};

enum {
  PROP_0,
  PROP_ID,
  PROP_NAME,
  PROP_LAST
};

static void melo_player_set_property (GObject *object, guint property_id,
                                      const GValue *value, GParamSpec *pspec);
static void melo_player_get_property (GObject *object, guint property_id,
                                      GValue *value, GParamSpec *pspec);

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (MeloPlayer, melo_player, G_TYPE_OBJECT)

static void
melo_player_finalize (GObject *gobject)
{
  MeloPlayer *player = MELO_PLAYER (gobject);
  MeloPlayerPrivate *priv = melo_player_get_instance_private (player);

  /* Send a delete player event */
  melo_event_player_delete (priv->id);

  /* Lock player list */
  G_LOCK (melo_player_mutex);

  /* Remove object from player list */
  melo_player_list = g_list_remove (melo_player_list, player);
  g_hash_table_remove (melo_player_hash, priv->id);

  /* Unlock player list */
  G_UNLOCK (melo_player_mutex);

  /* Free player ID */
  g_free (priv->id);

  /* Unref attached playlist */
  if (player->playlist)
    g_object_unref (player->playlist);

  /* Chain up to the parent class */
  G_OBJECT_CLASS (melo_player_parent_class)->finalize (gobject);
}

static void
melo_player_class_init (MeloPlayerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  /* Add custom finalize() function */
  object_class->finalize = melo_player_finalize;
  object_class->set_property = melo_player_set_property;
  object_class->get_property = melo_player_get_property;

  /* Install ID property */
  g_object_class_install_property (object_class, PROP_ID,
      g_param_spec_string ("id", "ID", "Player ID", NULL,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                           G_PARAM_STATIC_STRINGS));

  /* Install name property */
  g_object_class_install_property (object_class, PROP_NAME,
      g_param_spec_string ("name", "Name", "Player name", NULL,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                           G_PARAM_STATIC_STRINGS));
}

static void
melo_player_init (MeloPlayer *self)
{
  MeloPlayerPrivate *priv = melo_player_get_instance_private (self);

  self->priv = priv;
  priv->id = NULL;
  priv->name = NULL;
  priv->info.playlist_id = NULL;
}

const gchar *
melo_player_get_id (MeloPlayer *player)
{
  return player->priv->id;
}

const gchar *
melo_player_get_name (MeloPlayer *player)
{
  return player->priv->name;
}

static void
melo_player_set_property (GObject *object, guint property_id,
                          const GValue *value, GParamSpec *pspec)
{
  MeloPlayer *player = MELO_PLAYER (object);

  switch (property_id) {
    case PROP_ID:
      g_free (player->priv->id);
      player->priv->id = g_value_dup_string (value);
      break;
    case PROP_NAME:
      g_free (player->priv->name);
      player->priv->name = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
melo_player_get_property (GObject *object, guint property_id, GValue *value,
                          GParamSpec *pspec)
{
  MeloPlayer *player = MELO_PLAYER (object);

  switch (property_id) {
    case PROP_ID:
      g_value_set_string (value, melo_player_get_id (player));
      break;
    case PROP_NAME:
      g_value_set_string (value, melo_player_get_name (player));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

const MeloPlayerInfo *
melo_player_get_info (MeloPlayer *player)
{
  MeloPlayerClass *pclass = MELO_PLAYER_GET_CLASS (player);
  MeloPlayerPrivate *priv = player->priv;

  /* Copy info from sub module */
  if (pclass->get_info)
    priv->info = *pclass->get_info (player);

  /* Update player name */
  if (!priv->info.name)
    priv->info.name = priv->name;

  /* Update playlist ID */
  if (!priv->info.playlist_id && player->playlist)
    priv->info.playlist_id = melo_playlist_get_id (player->playlist);

  /* Update available controls */
  if (pclass->set_state)
    priv->info.control.state = TRUE;
  if (pclass->prev)
    priv->info.control.prev = TRUE;
  if (pclass->next)
    priv->info.control.next = TRUE;
  if (pclass->set_volume)
    priv->info.control.volume = TRUE;
  if (pclass->set_mute)
    priv->info.control.mute = TRUE;

  return &priv->info;
}

MeloPlayer *
melo_player_get_player_by_id (const gchar *id)
{
  MeloPlayer *play;

  /* Lock player list */
  G_LOCK (melo_player_mutex);

  /* Find player by id */
  play = g_hash_table_lookup (melo_player_hash, id);

  /* Increment reference count */
  if (play)
    g_object_ref (play);

  /* Unlock player list */
  G_UNLOCK (melo_player_mutex);

  return play;
}

GList *
melo_player_get_list ()
{
  GList *list = NULL;

  /* Lock player list */
  G_LOCK (melo_player_mutex);

  /* Copy list */
  if (melo_player_list)
    list = g_list_copy_deep (melo_player_list, (GCopyFunc) g_object_ref, NULL);

  /* Unlock player list */
  G_UNLOCK (melo_player_mutex);

  return list;
}

MeloPlayer *
melo_player_new (GType type, const gchar *id, const gchar *name)
{
  MeloPlayer *play;

  g_return_val_if_fail (id, NULL);
  g_return_val_if_fail (g_type_is_a (type, MELO_TYPE_PLAYER), NULL);

  /* Lock player list */
  G_LOCK (melo_player_mutex);

  /* Create player list */
  if (!melo_player_hash)
    melo_player_hash = g_hash_table_new_full (g_str_hash, g_str_equal,
                                               g_free, NULL);

  /* Check if ID is already used */
  if (g_hash_table_lookup (melo_player_hash, id))
    goto failed;

  /* Create a new instance of player */
  play = g_object_new (type, "id", id, "name", name, NULL);
  if (!play)
    goto failed;

  /* Add new player instance to player list */
  g_hash_table_insert (melo_player_hash, g_strdup (id), play);
  melo_player_list = g_list_append (melo_player_list, play);

  /* Unlock player list */
  G_UNLOCK (melo_player_mutex);

  /* Send a new player event */
  melo_event_player_new (id, melo_player_get_info(play));

  return play;

failed:
  G_UNLOCK (melo_player_mutex);
  return NULL;
}

void
melo_player_set_playlist (MeloPlayer *player, MeloPlaylist *playlist)
{
  if (player->playlist)
    g_object_unref (player->playlist);

  player->playlist = g_object_ref (playlist);
}

MeloPlaylist *
melo_player_get_playlist (MeloPlayer *player)
{
  g_return_val_if_fail (player->playlist, NULL);

  return g_object_ref (player->playlist);
}

gboolean
melo_player_add (MeloPlayer *player, const gchar *path, const gchar *name,
                 MeloTags *tags)
{
  MeloPlayerClass *pclass = MELO_PLAYER_GET_CLASS (player);

  g_return_val_if_fail (pclass->add, FALSE);

  return pclass->add (player, path, name, tags);
}

gboolean
melo_player_load (MeloPlayer *player, const gchar *path,
                  const gchar *name, MeloTags *tags, gboolean insert,
                  gboolean stopped)
{
  MeloPlayerClass *pclass = MELO_PLAYER_GET_CLASS (player);

  g_return_val_if_fail (pclass->load, FALSE);

  return pclass->load (player, path, name, tags, insert, stopped);
}

gboolean
melo_player_play (MeloPlayer *player, const gchar *path, const gchar *name,
                  MeloTags *tags, gboolean insert)
{
  MeloPlayerClass *pclass = MELO_PLAYER_GET_CLASS (player);

  g_return_val_if_fail (pclass->play, FALSE);

  return pclass->play (player, path, name, tags, insert);
}

MeloPlayerState
melo_player_set_state (MeloPlayer *player, MeloPlayerState state)
{
  MeloPlayerClass *pclass = MELO_PLAYER_GET_CLASS (player);

  g_return_val_if_fail (pclass->set_state, MELO_PLAYER_STATE_NONE);

  return pclass->set_state (player, state);
}

gboolean
melo_player_prev (MeloPlayer *player)
{
  MeloPlayerClass *pclass = MELO_PLAYER_GET_CLASS (player);

  g_return_val_if_fail (pclass->prev, FALSE);

  return pclass->prev (player);
}

gboolean
melo_player_next (MeloPlayer *player)
{
  MeloPlayerClass *pclass = MELO_PLAYER_GET_CLASS (player);

  g_return_val_if_fail (pclass->next, FALSE);

  return pclass->next (player);
}

gint
melo_player_set_pos (MeloPlayer *player, gint pos)
{
  MeloPlayerClass *pclass = MELO_PLAYER_GET_CLASS (player);

  g_return_val_if_fail (pclass->set_pos, 0);

  return pclass->set_pos (player, pos);
}

gdouble
melo_player_set_volume (MeloPlayer *player, gdouble volume)
{
  MeloPlayerClass *pclass = MELO_PLAYER_GET_CLASS (player);

  g_return_val_if_fail (pclass->set_volume, 0);

  return pclass->set_volume (player, volume);
}

gboolean
melo_player_set_mute (MeloPlayer *player, gboolean mute)
{
  MeloPlayerClass *pclass = MELO_PLAYER_GET_CLASS (player);

  g_return_val_if_fail (pclass->set_mute, FALSE);

  return pclass->set_mute (player, mute);
}

MeloPlayerState
melo_player_get_state (MeloPlayer *player)
{
  MeloPlayerClass *pclass = MELO_PLAYER_GET_CLASS (player);

  g_return_val_if_fail (pclass->get_state, MELO_PLAYER_STATE_NONE);

  return pclass->get_state (player);
}

gchar *
melo_player_get_media_name (MeloPlayer *player)
{
  MeloPlayerClass *pclass = MELO_PLAYER_GET_CLASS (player);

  g_return_val_if_fail (pclass->get_name, NULL);

  return pclass->get_name (player);
}

gint
melo_player_get_pos (MeloPlayer *player, gint *duration)
{
  MeloPlayerClass *pclass = MELO_PLAYER_GET_CLASS (player);

  if (!pclass->get_pos) {
    if (duration)
      *duration = 0;
    return 0;
  }

  return pclass->get_pos (player, duration);
}

gdouble
melo_player_get_volume (MeloPlayer *player)
{
  MeloPlayerClass *pclass = MELO_PLAYER_GET_CLASS (player);

  g_return_val_if_fail (pclass->get_volume, 0);

  return pclass->get_volume (player);
}

MeloPlayerStatus *
melo_player_get_status (MeloPlayer *player)
{
  MeloPlayerClass *pclass = MELO_PLAYER_GET_CLASS (player);

  g_return_val_if_fail (pclass->get_status, NULL);

  return pclass->get_status (player);
}

gboolean
melo_player_get_cover (MeloPlayer *player, GBytes **cover, gchar **type)
{
  MeloPlayerClass *pclass = MELO_PLAYER_GET_CLASS (player);

  g_return_val_if_fail (pclass->get_cover, FALSE);

  return pclass->get_cover (player, cover, type);
}

MeloPlayerStatus *
melo_player_status_new (MeloPlayer *player, MeloPlayerState state,
                        const gchar *name)
{
  MeloPlayerStatus *status;

  /* Allocate new status */
  status = g_slice_new0 (MeloPlayerStatus);
  if (!status)
    return NULL;

  /* Allocate private data */
  status->priv = g_slice_new0 (MeloPlayerStatusPrivate);
  if (!status->priv) {
    g_slice_free (MeloPlayerStatus, status);
    return NULL;
  }

  /* Init private mutex */
  g_mutex_init (&status->priv->mutex);

  /* Init reference counter */
  status->priv->ref_count = 1;

  /* Save player ID */
  if (player)
    status->priv->id = g_strdup (melo_player_get_id (player));

  /* Set state and name */
  status->state = state;
  status->priv->name = g_strdup (name);

  return status;
}

void
melo_player_status_set_state (MeloPlayerStatus *status, MeloPlayerState state)
{
  /* Update state */
  status->state = state;

  /* Send 'player state' event */
  melo_event_player_state (status->priv->id, state);
}

void
melo_player_status_set_buffering (MeloPlayerStatus *status,
                                  MeloPlayerState state, guint percent)
{
  /* Update state and buffer percent */
  status->state = state;
  status->buffer_percent = percent;

  /* Send 'player buffering' event */
  melo_event_player_buffering (status->priv->id, state, percent);
}

void
melo_player_status_set_pos (MeloPlayerStatus *status, gint pos)
{
  /* Update position */
  status->pos = pos;

  /* Send 'player seek' event */
  melo_event_player_seek (status->priv->id, pos);
}

void
melo_player_status_set_duration (MeloPlayerStatus *status, gint duration)
{
  /* Update duration */
  status->duration = duration;

  /* Send 'player duration' event */
  melo_event_player_duration (status->priv->id, duration);
}

void
melo_player_status_set_playlist (MeloPlayerStatus *status, gboolean has_prev,
                                 gboolean has_next)
{
  /* Update playlist */
  status->has_prev = has_prev;
  status->has_next = has_next;

  /* Send 'player playlist' event */
  melo_event_player_playlist (status->priv->id, has_prev, has_next);
}

void
melo_player_status_set_volume (MeloPlayerStatus *status, gdouble volume)
{
  /* Update volume */
  status->volume = volume;

  /* Send 'player volume' event */
  melo_event_player_volume (status->priv->id, volume);
}

void
melo_player_status_set_mute (MeloPlayerStatus *status, gboolean mute)
{
  /* Update mute */
  status->mute = mute;

  /* Send 'player mute' event */
  melo_event_player_mute (status->priv->id, mute);
}

void
melo_player_status_set_name (MeloPlayerStatus *status, const gchar *name,
                             gboolean send_event)
{
  MeloPlayerStatusPrivate *priv = status->priv;

  /* Lock name access */
  g_mutex_lock (&priv->mutex);

  /* Update name */
  g_free (priv->name);
  priv->name = g_strdup (name);

  /* Unlock name access */
  g_mutex_unlock (&priv->mutex);

  /* Send 'player name' event */
  if (send_event)
    melo_event_player_name (priv->id, name);
}

gchar *
melo_player_status_get_name (const MeloPlayerStatus *status)
{
  MeloPlayerStatusPrivate *priv = status->priv;
  gchar *name;

  /* Lock name access */
  g_mutex_lock (&priv->mutex);

  /* Copy name */
  name = g_strdup (priv->name);

  /* Unlock name access */
  g_mutex_unlock (&priv->mutex);

  return name;
}

void
melo_player_status_set_error (MeloPlayerStatus *status, const gchar *error,
                              gboolean send_event)
{
  MeloPlayerStatusPrivate *priv = status->priv;

  /* Lock error access */
  g_mutex_lock (&priv->mutex);

  /* Update error */
  g_free (priv->error);
  priv->error = g_strdup (error);
  if (error)
    status->state = MELO_PLAYER_STATE_ERROR;

  /* Unlock error access */
  g_mutex_unlock (&priv->mutex);

  /* Send 'player error' event */
  if (send_event)
    melo_event_player_error (priv->id, error);
}

gchar *
melo_player_status_get_error (const MeloPlayerStatus *status)
{
  MeloPlayerStatusPrivate *priv = status->priv;
  gchar *error;

  /* Lock error access */
  g_mutex_lock (&priv->mutex);

  /* Copy error */
  error = g_strdup (priv->error);

  /* Unlock error access */
  g_mutex_unlock (&priv->mutex);

  return error;
}

void
melo_player_status_take_tags (MeloPlayerStatus *status, MeloTags *tags,
                              gboolean send_event)
{
  MeloPlayerStatusPrivate *priv = status->priv;

  /* Lock tags access */
  g_mutex_lock (&priv->mutex);

  /* Free previous tags */
  if (priv->tags)
    melo_tags_unref (priv->tags);

  /* Set new tags */
  priv->tags = tags;

  /* Update timestamp */
  melo_tags_update (tags);

  /* Unlock tags access */
  g_mutex_unlock (&priv->mutex);

  /* Send 'player tags' event */
  if (send_event)
    melo_event_player_tags (priv->id, tags);
}

void
melo_player_status_set_tags (MeloPlayerStatus *status, MeloTags *tags,
                             gboolean send_event)
{
  if (tags)
    melo_tags_ref (tags);
  melo_player_status_take_tags (status, tags, send_event);
}

MeloTags *
melo_player_status_get_tags (const MeloPlayerStatus *status)
{
  MeloPlayerStatusPrivate *priv = status->priv;
  MeloTags *tags = NULL;

  /* Lock tags access */
  g_mutex_lock (&priv->mutex);

  /* Get a reference to tags */
  if (priv->tags)
    tags = melo_tags_ref (priv->tags);

  /* Unlock tags access */
  g_mutex_unlock (&priv->mutex);

  return tags;
}

void
melo_player_status_lock (const MeloPlayerStatus *status)
{
  /* Lock status access */
  g_mutex_lock (&status->priv->mutex);
}

void
melo_player_status_unlock (const MeloPlayerStatus *status)
{
  /* Unlock status access */
  g_mutex_unlock (&status->priv->mutex);
}

const gchar *
melo_player_status_lock_get_name (const MeloPlayerStatus *status)
{
  return status->priv->name;
}
const gchar *
melo_player_status_lock_get_error (const MeloPlayerStatus *status)
{
  return status->priv->error;
}

MeloPlayerStatus *
melo_player_status_ref (MeloPlayerStatus *status)
{
  status->priv->ref_count++;
  return status;
}

void
melo_player_status_unref (MeloPlayerStatus *status)
{
  status->priv->ref_count--;
  if (status->priv->ref_count)
    return;

  /* Free status */
  g_free (status->priv->id);
  g_free (status->priv->name);
  g_free (status->priv->error);
  if (status->priv->tags)
    melo_tags_unref (status->priv->tags);
  g_mutex_clear (&status->priv->mutex);
  g_slice_free (MeloPlayerStatusPrivate, status->priv);
  g_slice_free (MeloPlayerStatus, status);
}

static const gchar *melo_player_state_str[] = {
  [MELO_PLAYER_STATE_NONE] = "none",
  [MELO_PLAYER_STATE_LOADING] = "loading",
  [MELO_PLAYER_STATE_BUFFERING] = "buffering",
  [MELO_PLAYER_STATE_PLAYING] = "playing",
  [MELO_PLAYER_STATE_PAUSED_LOADING] = "paused_loading",
  [MELO_PLAYER_STATE_PAUSED_BUFFERING] = "paused_buffering",
  [MELO_PLAYER_STATE_PAUSED] = "paused",
  [MELO_PLAYER_STATE_STOPPED] = "stopped",
  [MELO_PLAYER_STATE_ERROR] = "error",
};

const gchar *
melo_player_state_to_string (MeloPlayerState state)
{
  if (state >= MELO_PLAYER_STATE_COUNT)
    return NULL;
  return melo_player_state_str[state];
}

MeloPlayerState
melo_player_state_from_string (const gchar *sstate)
{
  int i;

  if (!sstate)
    return MELO_PLAYER_STATE_NONE;

  for (i = 0; i < MELO_PLAYER_STATE_COUNT; i++)
    if (!g_strcmp0 (sstate, melo_player_state_str[i]))
      return i;

  return MELO_PLAYER_STATE_NONE;
}

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

#include "melo_player.h"

/* Internal player list */
G_LOCK_DEFINE_STATIC (melo_player_mutex);
static GHashTable *melo_player_hash = NULL;
static GList *melo_player_list = NULL;

struct _MeloPlayerStatusPrivate {
  GMutex mutex;
  gint ref_count;

  /* Tags */
  MeloTags *tags;
};

struct _MeloPlayerPrivate {
  gchar *id;
  MeloPlayerInfo info;
};

enum {
  PROP_0,
  PROP_ID,
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
}

static void
melo_player_init (MeloPlayer *self)
{
  MeloPlayerPrivate *priv = melo_player_get_instance_private (self);

  self->priv = priv;
  priv->id = NULL;
  priv->info.playlist_id = NULL;
}

const gchar *
melo_player_get_id (MeloPlayer *player)
{
  return player->priv->id;
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

  /* Update playlist ID */
  if (!priv->info.playlist_id && player->playlist)
    priv->info.playlist_id = melo_playlist_get_id (player->playlist);

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
melo_player_new (GType type, const gchar *id)
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
  play = g_object_new (type, "id", id, NULL);
  if (!play)
    goto failed;

  /* Add new player instance to player list */
  g_hash_table_insert (melo_player_hash, g_strdup (id), play);
  melo_player_list = g_list_append (melo_player_list, play);

  /* Unlock player list */
  G_UNLOCK (melo_player_mutex);

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

MeloPlayerState
melo_player_get_state (MeloPlayer *player)
{
  MeloPlayerClass *pclass = MELO_PLAYER_GET_CLASS (player);

  g_return_val_if_fail (pclass->get_state, MELO_PLAYER_STATE_NONE);

  return pclass->get_state (player);
}

gchar *
melo_player_get_name (MeloPlayer *player)
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
melo_player_status_new (MeloPlayerState state, const gchar *name)
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

  /* Set state and name */
  status->state = state;
  status->name = g_strdup (name);

  return status;
}

void
melo_player_status_take_tags (MeloPlayerStatus *status, MeloTags *tags)
{
  MeloPlayerStatusPrivate *priv = status->priv;

  /* Lock tags access */
  g_mutex_lock (&priv->mutex);

  /* Free previous tags */
  if (priv->tags)
    melo_tags_unref (priv->tags);

  /* Set new tags */
  priv->tags = tags;

  /* Unlock tags access */
  g_mutex_unlock (&priv->mutex);
}

void
melo_player_status_set_tags (MeloPlayerStatus *status, MeloTags *tags)
{
  if (tags)
    melo_tags_ref (tags);
  melo_player_status_take_tags (status, tags);
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
  g_free (status->error);
  g_free (status->name);
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

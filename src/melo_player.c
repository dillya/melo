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

struct _MeloPlayerPrivate {
  gchar *id;
};

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

  /* Chain up to the parent class */
  G_OBJECT_CLASS (melo_player_parent_class)->finalize (gobject);
}

static void
melo_player_class_init (MeloPlayerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  /* Add custom finalize() function */
  object_class->finalize = melo_player_finalize;
}

static void
melo_player_init (MeloPlayer *self)
{
  MeloPlayerPrivate *priv = melo_player_get_instance_private (self);

  self->priv = priv;
  priv->id = NULL;
}

static void
melo_player_set_id (MeloPlayer *player, const gchar *id)
{
  MeloPlayerPrivate *priv = player->priv;

  g_free (priv->id);
  priv->id = g_strdup (id);
}

const gchar *
melo_player_get_id (MeloPlayer *player)
{
  return player->priv->id;
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
  play = g_object_new (type, NULL);
  if (!play)
    goto failed;

  /* Set ID */
  melo_player_set_id (play, id);

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

gboolean
melo_player_play (MeloPlayer *player, const gchar *path)
{
  MeloPlayerClass *pclass = MELO_PLAYER_GET_CLASS (player);

  g_return_val_if_fail (pclass->play, FALSE);

  return pclass->play (player, path);
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

MeloPlayerStatus *
melo_player_get_status (MeloPlayer *player)
{
  MeloPlayerClass *pclass = MELO_PLAYER_GET_CLASS (player);

  g_return_val_if_fail (pclass->get_status, NULL);

  return pclass->get_status (player);
}

MeloPlayerStatus *
melo_player_status_new (MeloPlayerState state, const gchar *name)
{
  MeloPlayerStatus *status;

  /* Allocate new status */
  status = g_slice_new0 (MeloPlayerStatus);
  if (!status)
    return NULL;

  /* Set state and name */
  status->state = state;
  status->name = g_strdup (name);

  return status;
}

void
melo_player_status_free (MeloPlayerStatus *status)
{
  g_free (status->error);
  g_free (status->name);
  g_slice_free (MeloPlayerStatus, status);
}

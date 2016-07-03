/*
 * melo_player_file.c: File Player using GStreamer
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

#include "melo_player_file.h"

static gboolean melo_player_file_play (MeloPlayer *player, const gchar *path);
static MeloPlayerState melo_player_file_get_state (MeloPlayer *player);
static gchar *melo_player_file_get_name (MeloPlayer *player);
static gint melo_player_file_get_pos (MeloPlayer *player, gint *duration);
static MeloPlayerStatus *melo_player_file_get_status (MeloPlayer *player);

struct _MeloPlayerFilePrivate {
  GMutex mutex;
  MeloPlayerState state;
  gchar *uri;
};

G_DEFINE_TYPE_WITH_PRIVATE (MeloPlayerFile, melo_player_file, MELO_TYPE_PLAYER)

static void
melo_player_file_finalize (GObject *gobject)
{
  MeloPlayerFile *pfile = MELO_PLAYER_FILE (gobject);
  MeloPlayerFilePrivate *priv = melo_player_file_get_instance_private (pfile);

  if (priv->uri)
    g_free (priv->uri);

  /* Free player mutex */
  g_mutex_clear (&priv->mutex);

  /* Chain up to the parent class */
  G_OBJECT_CLASS (melo_player_file_parent_class)->finalize (gobject);
}

static void
melo_player_file_class_init (MeloPlayerFileClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MeloPlayerClass *pclass = MELO_PLAYER_CLASS (klass);

  pclass->play = melo_player_file_play;
  pclass->get_state = melo_player_file_get_state;
  pclass->get_name = melo_player_file_get_name;
  pclass->get_pos = melo_player_file_get_pos;
  pclass->get_status = melo_player_file_get_status;

  /* Add custom finalize() function */
  object_class->finalize = melo_player_file_finalize;
}

static void
melo_player_file_init (MeloPlayerFile *self)
{
  MeloPlayerFilePrivate *priv = melo_player_file_get_instance_private (self);

  self->priv = priv;
  priv->state = MELO_PLAYER_STATE_NONE;
  priv->uri = NULL;

  /* Init player mutex */
  g_mutex_init (&priv->mutex);
}

static gboolean
melo_player_file_play (MeloPlayer *player, const gchar *path)
{
  MeloPlayerFilePrivate *priv = (MELO_PLAYER_FILE (player))->priv;

  /* Lock player mutex */
  g_mutex_lock (&priv->mutex);

  /* Replace URI */
  if (priv->uri)
    g_free (priv->uri);
  priv->uri = g_strdup (path);

  /* Unlock player mutex */
  g_mutex_unlock (&priv->mutex);

  return TRUE;
}

static MeloPlayerState
melo_player_file_get_state (MeloPlayer *player)
{
  return (MELO_PLAYER_FILE (player))->priv->state;
}

static gchar *
melo_player_file_get_name (MeloPlayer *player)
{
  MeloPlayerFilePrivate *priv = (MELO_PLAYER_FILE (player))->priv;
  gchar *name = NULL;

  /* Lock player mutex */
  g_mutex_lock (&priv->mutex);

  /* Extract filename from URI */
  if (priv->uri)
    name = g_path_get_basename (priv->uri);

  /* Unlock player mutex */
  g_mutex_unlock (&priv->mutex);

  return name;
}

static gint
melo_player_file_get_pos (MeloPlayer *player, gint *duration)
{
  if (duration)
    *duration = 0;
  return 0;
}

static MeloPlayerStatus *
melo_player_file_get_status (MeloPlayer *player)
{
  MeloPlayerFilePrivate *priv = (MELO_PLAYER_FILE (player))->priv;
  MeloPlayerStatus *status;

  /* Allocate a new status item */
  status = melo_player_status_new (priv->state, NULL);
  status->name = melo_player_file_get_name (player);

  return status;
}

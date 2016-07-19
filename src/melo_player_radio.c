/*
 * melo_player_radio.c: Radio Player using GStreamer
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

#include <gst/gst.h>

#include "melo_player_radio.h"

static gboolean melo_player_radio_play (MeloPlayer *player, const gchar *path,
                                       const gchar *name, MeloTags *tags,
                                       gboolean insert);
static MeloPlayerState melo_player_radio_set_state (MeloPlayer *player,
                                                   MeloPlayerState state);
static gint melo_player_radio_set_pos (MeloPlayer *player, gint pos);

static MeloPlayerState melo_player_radio_get_state (MeloPlayer *player);
static gchar *melo_player_radio_get_name (MeloPlayer *player);
static gint melo_player_radio_get_pos (MeloPlayer *player, gint *duration);
static MeloPlayerStatus *melo_player_radio_get_status (MeloPlayer *player);

struct _MeloPlayerRadioPrivate {
  GMutex mutex;
  MeloPlayerStatus *status;
};

G_DEFINE_TYPE_WITH_PRIVATE (MeloPlayerRadio, melo_player_radio, MELO_TYPE_PLAYER)

static void
melo_player_radio_finalize (GObject *gobject)
{
  MeloPlayerRadio *pradio = MELO_PLAYER_RADIO (gobject);
  MeloPlayerRadioPrivate *priv =
                                melo_player_radio_get_instance_private (pradio);

  /* Free status */
  melo_player_status_unref (priv->status);

  /* Clear mutex */
  g_mutex_clear (&priv->mutex);

  /* Chain up to the parent class */
  G_OBJECT_CLASS (melo_player_radio_parent_class)->finalize (gobject);
}

static void
melo_player_radio_class_init (MeloPlayerRadioClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MeloPlayerClass *pclass = MELO_PLAYER_CLASS (klass);

  /* Control */
  pclass->play = melo_player_radio_play;
  pclass->set_state = melo_player_radio_set_state;
  pclass->set_pos = melo_player_radio_set_pos;

  /* Status */
  pclass->get_state = melo_player_radio_get_state;
  pclass->get_name = melo_player_radio_get_name;
  pclass->get_pos = melo_player_radio_get_pos;
  pclass->get_status = melo_player_radio_get_status;

  /* Add custom finalize() function */
  object_class->finalize = melo_player_radio_finalize;
}

static void
melo_player_radio_init (MeloPlayerRadio *self)
{
  MeloPlayerRadioPrivate *priv = melo_player_radio_get_instance_private (self);

  self->priv = priv;

  /* Init player mutex */
  g_mutex_init (&priv->mutex);

  /* Create new status handler */
  priv->status = melo_player_status_new (MELO_PLAYER_STATE_NONE, NULL);
}

static gboolean
melo_player_radio_play (MeloPlayer *player, const gchar *path,
                        const gchar *name, MeloTags *tags, gboolean insert)
{
  return FALSE;
}

static MeloPlayerState
melo_player_radio_set_state (MeloPlayer *player, MeloPlayerState state)
{
  return MELO_PLAYER_STATE_NONE;
}

static gint
melo_player_radio_set_pos (MeloPlayer *player, gint pos)
{
  return -1;
}

static MeloPlayerState
melo_player_radio_get_state (MeloPlayer *player)
{
  return (MELO_PLAYER_RADIO (player))->priv->status->state;
}

static gchar *
melo_player_radio_get_name (MeloPlayer *player)
{
  MeloPlayerRadioPrivate *priv = (MELO_PLAYER_RADIO (player))->priv;
  gchar *name = NULL;

  /* Lock player mutex */
  g_mutex_lock (&priv->mutex);

  /* Copy name */
  name = g_strdup (priv->status->name);

  /* Unlock player mutex */
  g_mutex_unlock (&priv->mutex);

  return name;
}

static gint
melo_player_radio_get_pos (MeloPlayer *player, gint *duration)
{
  MeloPlayerRadioPrivate *priv = (MELO_PLAYER_RADIO (player))->priv;

  /* Get duration */
  if (duration)
    *duration = priv->status->duration;

  return 0;
}

static MeloPlayerStatus *
melo_player_radio_get_status (MeloPlayer *player)
{
  MeloPlayerRadioPrivate *priv = (MELO_PLAYER_RADIO (player))->priv;
  MeloPlayerStatus *status;

  /* Lock player mutex */
  g_mutex_lock (&priv->mutex);

  /* Copy status */
  status = melo_player_status_ref (priv->status);

  /* Unlock player mutex */
  g_mutex_unlock (&priv->mutex);

  return status;
  return NULL;
}

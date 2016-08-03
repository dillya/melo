/*
 * melo_player_airplay.c: Airplay Player using GStreamer
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

#include <string.h>

#include <gst/gst.h>

#include "melo_player_airplay.h"

static gboolean melo_player_airplay_play (MeloPlayer *player, const gchar *path,
                                          const gchar *name, MeloTags *tags,
                                          gboolean insert);
static MeloPlayerState melo_player_airplay_set_state (MeloPlayer *player,
                                                      MeloPlayerState state);

static MeloPlayerState melo_player_airplay_get_state (MeloPlayer *player);
static gchar *melo_player_airplay_get_name (MeloPlayer *player);
static gint melo_player_airplay_get_pos (MeloPlayer *player, gint *duration);
static MeloPlayerStatus *melo_player_airplay_get_status (MeloPlayer *player);

struct _MeloPlayerAirplayPrivate {
  GMutex mutex;
  MeloPlayerStatus *status;

  /* Gstreamer pipeline */
  GstElement *pipeline;
  guint bus_watch_id;

  /* Format */
  guint samplerate;
  guint channel_count;
};

G_DEFINE_TYPE_WITH_PRIVATE (MeloPlayerAirplay, melo_player_airplay, MELO_TYPE_PLAYER)

static void
melo_player_airplay_finalize (GObject *gobject)
{
  MeloPlayerAirplay *pair = MELO_PLAYER_AIRPLAY (gobject);
  MeloPlayerAirplayPrivate *priv =
                                melo_player_airplay_get_instance_private (pair);

  /* Stop pipeline */
  melo_player_airplay_teardown (pair);

  /* Free status */
  melo_player_status_unref (priv->status);

  /* Clear mutex */
  g_mutex_clear (&priv->mutex);

  /* Chain up to the parent class */
  G_OBJECT_CLASS (melo_player_airplay_parent_class)->finalize (gobject);
}

static void
melo_player_airplay_class_init (MeloPlayerAirplayClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MeloPlayerClass *pclass = MELO_PLAYER_CLASS (klass);

  /* Control */
  pclass->play = melo_player_airplay_play;
  pclass->set_state = melo_player_airplay_set_state;

  /* Status */
  pclass->get_state = melo_player_airplay_get_state;
  pclass->get_name = melo_player_airplay_get_name;
  pclass->get_pos = melo_player_airplay_get_pos;
  pclass->get_status = melo_player_airplay_get_status;

  /* Add custom finalize() function */
  object_class->finalize = melo_player_airplay_finalize;
}

static void
melo_player_airplay_init (MeloPlayerAirplay *self)
{
  MeloPlayerAirplayPrivate *priv =
                                melo_player_airplay_get_instance_private (self);

  self->priv = priv;

  /* Init player mutex */
  g_mutex_init (&priv->mutex);

  /* Create new status handler */
  priv->status = melo_player_status_new (MELO_PLAYER_STATE_NONE, NULL);
}

static gboolean
bus_call (GstBus *bus, GstMessage *msg, gpointer data)
{
  MeloPlayerAirplay *pair = MELO_PLAYER_AIRPLAY (data);
  MeloPlayerAirplayPrivate *priv = pair->priv;
  GError *error;

  /* Process bus message */
  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_EOS:
      /* Stop playing */
      gst_element_set_state (priv->pipeline, GST_STATE_NULL);
      priv->status->state = MELO_PLAYER_STATE_STOPPED;
      break;
    case GST_MESSAGE_ERROR:
      /* End of stream */
      priv->status->state = MELO_PLAYER_STATE_ERROR;

      /* Lock player mutex */
      g_mutex_lock (&priv->mutex);

      /* Update error message */
      g_free (priv->status->error);
      gst_message_parse_error (msg, &error, NULL);
      priv->status->error = g_strdup (error->message);
      g_error_free (error);

      /* Unlock player mutex */
      g_mutex_unlock (&priv->mutex);
      break;
    default:
      ;
  }

  return TRUE;
}

static gboolean
melo_player_airplay_play (MeloPlayer *player, const gchar *path,
                          const gchar *name, MeloTags *tags, gboolean insert)
{
  MeloPlayerAirplayPrivate *priv = (MELO_PLAYER_AIRPLAY (player))->priv;

  /* Lock player mutex */
  g_mutex_lock (&priv->mutex);

  /* Update tags */
  melo_player_status_take_tags (priv->status, tags);

  /* Unlock player mutex */
  g_mutex_unlock (&priv->mutex);

  return TRUE;
}

static MeloPlayerState
melo_player_airplay_set_state (MeloPlayer *player, MeloPlayerState state)
{
  MeloPlayerAirplayPrivate *priv = (MELO_PLAYER_AIRPLAY (player))->priv;

  /* Lock player mutex */
  g_mutex_lock (&priv->mutex);

  if (state == MELO_PLAYER_STATE_PLAYING)
    gst_element_set_state (priv->pipeline, GST_STATE_PLAYING);
  else if (state == MELO_PLAYER_STATE_PAUSED)
    gst_element_set_state (priv->pipeline, GST_STATE_PAUSED);
  else
    state = priv->status->state;
  priv->status->state = state;

  /* Unlock player mutex */
  g_mutex_unlock (&priv->mutex);

  return state;
}

static gint
melo_player_airplay_set_pos (MeloPlayer *player, gint pos)
{
  return -1;
}

static MeloPlayerState
melo_player_airplay_get_state (MeloPlayer *player)
{
  return (MELO_PLAYER_AIRPLAY (player))->priv->status->state;
}

static gchar *
melo_player_airplay_get_name (MeloPlayer *player)
{
  MeloPlayerAirplayPrivate *priv = (MELO_PLAYER_AIRPLAY (player))->priv;
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
melo_player_airplay_get_pos (MeloPlayer *player, gint *duration)
{
  MeloPlayerAirplayPrivate *priv = (MELO_PLAYER_AIRPLAY (player))->priv;

  /* Get duration */
  if (duration)
    *duration = priv->status->duration;

  /* Get length */
  return priv->status->pos;
}

static MeloPlayerStatus *
melo_player_airplay_get_status (MeloPlayer *player)
{
  MeloPlayerAirplayPrivate *priv = (MELO_PLAYER_AIRPLAY (player))->priv;
  MeloPlayerStatus *status;

  /* Lock player mutex */
  g_mutex_lock (&priv->mutex);

  /* Copy status */
  status = melo_player_status_ref (priv->status);

  /* Unlock player mutex */
  g_mutex_unlock (&priv->mutex);

  return status;
}

static gboolean
melo_player_airplay_parse_format (MeloPlayerAirplayPrivate *priv,
                                  MeloAirplayCodec codec, const gchar *format)
{
  guint tmp;

  switch (codec) {
    case MELO_AIRPLAY_CODEC_ALAC:
      /* Get payload type */
      tmp = strtoul (format, &format, 10);

      /* Get ALAC parameters:
       *  - Max samples per frame (4 bytes)
       *  - 7a: ? (1 byte)
       *  - Sample size (4 bytes)
       *  - Rice historymult (1 byte)
       *  - Rice initialhistory (1 byte)
       *  - Rice kmodifier (1 byte)
       *  - Channel count (1 byte)
       *  - 80: ? (2 bytes)
       *  - 82: ? (4 bytes)
       *  - 86: ? (4 bytes)
       *  - Sample rate (4 bytes)
       */
      tmp = strtoul (format, &format, 10);
      tmp = strtoul (format, &format, 10);
      tmp = strtoul (format, &format, 10);
      tmp = strtoul (format, &format, 10);
      tmp = strtoul (format, &format, 10);
      tmp = strtoul (format, &format, 10);
      priv->channel_count = strtoul (format, &format, 10);
      tmp = strtoul (format, &format, 10);
      tmp = strtoul (format, &format, 10);
      tmp = strtoul (format, &format, 10);
      priv->samplerate = strtoul (format, &format, 10);

      break;
    case MELO_AIRPLAY_CODEC_PCM:
      /* Get payload type */
      tmp = strtoul (format, &format, 10);

      /* Get bits count */
      tmp = strtoul (format, &format, 10);

      /* Get samplerate and channel count */
      priv->samplerate = strtoul (format, &format, 10);
      priv->channel_count = strtoul (format, &format, 10);

      break;
    case MELO_AIRPLAY_CODEC_AAC:
    default:
      priv->samplerate = 44100;
      priv->channel_count = 2;
  }

  /* Set default values if not found */
  if (!priv->samplerate)
    priv->samplerate = 44100;
  if (!priv->channel_count)
      priv->channel_count = 2;

  return TRUE;
}

gboolean
melo_player_airplay_setup (MeloPlayerAirplay *pair,
                           MeloAirplayTransport transport, guint *port,
                           MeloAirplayCodec codec, const gchar *format)
{
  MeloPlayerAirplayPrivate *priv = pair->priv;
  guint max_port = *port + 100;
  GstElement *src, *sink;
  const gchar *id;
  gchar *pname;
  GstBus *bus;

  if (priv->pipeline)
    return FALSE;

  /* Parse format */
  if (!melo_player_airplay_parse_format (priv, codec, format))
    return FALSE;

  /* Get ID from player */
  id = melo_player_get_id (MELO_PLAYER (pair));
  pname = g_strdup_printf ("player_pipeline_%s", id);

  /* Create pipeline */
  priv->pipeline = gst_pipeline_new (pname);
  g_free (pname);

  /* Create source */
  if (transport == MELO_AIRPLAY_TRANSPORT_UDP)
    src = gst_element_factory_make ("udpsrc", NULL);
  else
    src = gst_element_factory_make ("tcpserversrc", NULL);

  /* Set server port */
  g_object_set (src, "port", *port, "reuse", FALSE, NULL);

  /* Add a fake sink and link everything */
  sink = gst_element_factory_make ("fakesink", NULL);
  gst_bin_add_many (GST_BIN (priv->pipeline), src, sink, NULL);
  gst_element_link (src, sink);

  /* Add a message handler */
  bus = gst_pipeline_get_bus (GST_PIPELINE (priv->pipeline));
  priv->bus_watch_id = gst_bus_add_watch (bus, bus_call, pair);
  gst_object_unref (bus);

  /* Start the pipeline */
  while (gst_element_set_state (priv->pipeline, GST_STATE_PLAYING) ==
                                                     GST_STATE_CHANGE_FAILURE) {
    /* Incremnent port until we found a free port */
    *port += 2;
    if (*port > max_port)
      return FALSE;

    /* Update port */
    g_object_set (src, "port", *port, NULL);
  }

  return TRUE;
}

gboolean
melo_player_airplay_record (MeloPlayerAirplay *pair, guint seq)
{
  MeloPlayerAirplayPrivate *priv = pair->priv;

  /* Lock player mutex */
  g_mutex_lock (&priv->mutex);

  /* Set playing */
  priv->status->state = MELO_PLAYER_STATE_PLAYING;

  /* Unlock player mutex */
  g_mutex_unlock (&priv->mutex);

  return TRUE;
}

gboolean
melo_player_airplay_flush (MeloPlayerAirplay *pair, guint seq)
{
  MeloPlayerAirplayPrivate *priv = pair->priv;

  /* Lock player mutex */
  g_mutex_lock (&priv->mutex);

  /* Set playing */
  priv->status->state = MELO_PLAYER_STATE_PAUSED;

  /* Unlock player mutex */
  g_mutex_unlock (&priv->mutex);

  return TRUE;
}

gboolean
melo_player_airplay_teardown (MeloPlayerAirplay *pair)
{
  MeloPlayerAirplayPrivate *priv = pair->priv;

  if (!priv->pipeline)
    return FALSE;

  /* Stop pipeline */
  gst_element_set_state (priv->pipeline, GST_STATE_NULL);

  /* Remove message handler */
  g_source_remove (priv->bus_watch_id);

  /* Free gstreamer pipeline */
  g_object_unref (priv->pipeline);

  return TRUE;
}

gboolean
melo_player_airplay_set_volume (MeloPlayerAirplay *pair, gdouble volume)
{
  return TRUE;
}

gboolean
melo_player_airplay_set_progress (MeloPlayerAirplay *pair, guint start,
                                  guint cur, guint end)
{
  MeloPlayerAirplayPrivate *priv = pair->priv;

  /* Lock player mutex */
  g_mutex_lock (&priv->mutex);

  /* Set playing */
  priv->status->state = MELO_PLAYER_STATE_PLAYING;
  priv->status->pos = (cur - start) * 1000L / priv->samplerate;
  priv->status->duration =  (end - start) * 1000L / priv->samplerate;

  /* Unlock player mutex */
  g_mutex_unlock (&priv->mutex);

  return TRUE;
}

gboolean
melo_player_airplay_set_cover (MeloPlayerAirplay *pair, GBytes *cover,
                               const gchar *cover_type)
{
  MeloPlayerAirplayPrivate *priv = pair->priv;
  gboolean ret = FALSE;

  /* Lock player mutex */
  g_mutex_lock (&priv->mutex);

  /* Set cover if not already set */
  if (!priv->status->tags->cover && !priv->status->tags->cover_type) {
    priv->status->tags->cover = cover;
    priv->status->tags->cover_type = g_strdup (cover_type);
    melo_tags_update (priv->status->tags);
    ret = TRUE;
  } else
    g_bytes_unref (cover);

  /* Unlock player mutex */
  g_mutex_unlock (&priv->mutex);

  return ret;
}

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

#include <gst/gst.h>

#include "melo_player_file.h"

static gboolean bus_call (GstBus *bus, GstMessage *msg, gpointer data);
static void pad_added_handler (GstElement *src, GstPad *pad, GstElement *sink);

static gboolean melo_player_file_play (MeloPlayer *player, const gchar *path);
static MeloPlayerState melo_player_file_get_state (MeloPlayer *player);
static gchar *melo_player_file_get_name (MeloPlayer *player);
static gint melo_player_file_get_pos (MeloPlayer *player, gint *duration);
static MeloPlayerStatus *melo_player_file_get_status (MeloPlayer *player);

struct _MeloPlayerFilePrivate {
  GMutex mutex;

  /* Status */
  MeloPlayerState state;
  gchar *uri;
  gchar *error;

  /* Gstreamer pipeline */
  GstElement *pipeline;
  GstElement *src;
  guint bus_watch_id;
};

G_DEFINE_TYPE_WITH_PRIVATE (MeloPlayerFile, melo_player_file, MELO_TYPE_PLAYER)

static void
melo_player_file_finalize (GObject *gobject)
{
  MeloPlayerFile *pfile = MELO_PLAYER_FILE (gobject);
  MeloPlayerFilePrivate *priv = melo_player_file_get_instance_private (pfile);

  /* Stop pipeline */
  gst_element_set_state (priv->pipeline, GST_STATE_NULL);
  g_free (priv->error);

  /* Remove message handler */
  g_source_remove (priv->bus_watch_id);

  /* Free gstreamer pipeline */
  g_object_unref (priv->pipeline);

  /* Free URI */
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
  GstElement *sink;
  GstBus *bus;

  self->priv = priv;
  priv->state = MELO_PLAYER_STATE_NONE;
  priv->uri = NULL;
  priv->error = NULL;

  /* Init player mutex */
  g_mutex_init (&priv->mutex);

  /* Create pipeline */
  priv->pipeline = gst_pipeline_new ("file_player_pipeline");
  priv->src = gst_element_factory_make ("uridecodebin",
                                        "file_player_uridecodebin");
  sink = gst_element_factory_make ("autoaudiosink",
                                   "file_player_autoaudiosink");
  gst_bin_add_many (GST_BIN (priv->pipeline), priv->src, sink, NULL);

  /* Add signal handler on new pad */
  g_signal_connect(priv->src, "pad-added",
                   G_CALLBACK (pad_added_handler), sink);

  /* Add a message handler */
  bus = gst_pipeline_get_bus (GST_PIPELINE (priv->pipeline));
  priv->bus_watch_id = gst_bus_add_watch (bus, bus_call, self);
  gst_object_unref (bus);
}

static gboolean
bus_call (GstBus *bus, GstMessage *msg, gpointer data)
{
  MeloPlayerFile *pfile = MELO_PLAYER_FILE (data);
  MeloPlayerFilePrivate *priv = pfile->priv;
  GError *error;

  /* Process bus message */
  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_EOS:
      /* End of stream */
      priv->state = MELO_PLAYER_STATE_STOPPED;
      break;

    case GST_MESSAGE_ERROR:
      /* End of stream */
      priv->state = MELO_PLAYER_STATE_ERROR;

      /* Update error message */
      g_free (priv->error);
      gst_message_parse_error (msg, &error, NULL);
      priv->error = g_strdup (error->message);
      g_error_free (error);
      break;

    default:
      break;
  }

  return TRUE;
}

static void
pad_added_handler (GstElement *src, GstPad *pad, GstElement *sink)
{
  GstStructure *str;
  GstPad *sink_pad;
  GstCaps *caps;

  /* Get sink pad from sink element */
  sink_pad = gst_element_get_static_pad (sink, "sink");
  if (GST_PAD_IS_LINKED (sink_pad)) {
    g_object_unref (sink_pad);
    return;
  }

  /* Only select audio pad */
  caps = gst_pad_query_caps (pad, NULL);
  str = gst_caps_get_structure (caps, 0);
  if (!g_strrstr (gst_structure_get_name (str), "audio")) {
    gst_caps_unref (caps);
    gst_object_unref (sink_pad);
    return;
  }
  gst_caps_unref (caps);

  /* Link elements */
  gst_pad_link (pad, sink_pad);
  g_object_unref (sink_pad);
}

static gboolean
melo_player_file_play (MeloPlayer *player, const gchar *path)
{
  MeloPlayerFilePrivate *priv = (MELO_PLAYER_FILE (player))->priv;

  /* Lock player mutex */
  g_mutex_lock (&priv->mutex);

  /* Stop pipeline */
  gst_element_set_state (priv->pipeline, GST_STATE_NULL);
  g_free (priv->error);
  priv->error = NULL;

  /* Set new location to src element */
  g_object_set (priv->src, "uri", path, NULL);
  gst_element_set_state (priv->pipeline, GST_STATE_PLAYING);

  /* Replace URI */
  g_free (priv->uri);
  priv->uri = g_strdup (path);
  priv->state = MELO_PLAYER_STATE_PLAYING;

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
  status->error = g_strdup(priv->error);
  status->name = melo_player_file_get_name (player);

  return status;
}

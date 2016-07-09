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
static MeloPlayerState melo_player_file_set_state (MeloPlayer *player,
                                                   MeloPlayerState state);
static gint melo_player_file_set_pos (MeloPlayer *player, gint pos);

static MeloPlayerState melo_player_file_get_state (MeloPlayer *player);
static gchar *melo_player_file_get_name (MeloPlayer *player);
static gint melo_player_file_get_pos (MeloPlayer *player, gint *duration);
static MeloPlayerStatus *melo_player_file_get_status (MeloPlayer *player);

struct _MeloPlayerFilePrivate {
  GMutex mutex;

  /* Status */
  MeloPlayerStatus *status;
  gchar *uri;

  /* Gstreamer pipeline */
  GstElement *pipeline;
  GstElement *src;
  guint bus_watch_id;

  /* Gstreamer tags */
  GstTagList *tag_list;
};

G_DEFINE_TYPE_WITH_PRIVATE (MeloPlayerFile, melo_player_file, MELO_TYPE_PLAYER)

static void
melo_player_file_finalize (GObject *gobject)
{
  MeloPlayerFile *pfile = MELO_PLAYER_FILE (gobject);
  MeloPlayerFilePrivate *priv = melo_player_file_get_instance_private (pfile);

  /* Stop pipeline */
  gst_element_set_state (priv->pipeline, GST_STATE_NULL);
  melo_player_status_unref (priv->status);

  /* Remove message handler */
  g_source_remove (priv->bus_watch_id);

  /* Free gstreamer pipeline */
  g_object_unref (priv->pipeline);

  /* Free tag list */
  gst_tag_list_unref (priv->tag_list);

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

  /* Control */
  pclass->play = melo_player_file_play;
  pclass->set_state = melo_player_file_set_state;
  pclass->set_pos = melo_player_file_set_pos;

  /* Status */
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
  priv->uri = NULL;

  /* Init player mutex */
  g_mutex_init (&priv->mutex);

  /* Create new status handler */
  priv->status = melo_player_status_new (MELO_PLAYER_STATE_NONE, NULL);

  /* Create a new tag list */
  priv->tag_list = gst_tag_list_new_empty ();

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
    case GST_MESSAGE_DURATION_CHANGED:
    case GST_MESSAGE_ASYNC_DONE: {
      gint64 duration;

      /* Get duration */
      if (gst_element_query_duration (priv->src, GST_FORMAT_TIME, &duration)) {
        priv->status->duration = duration / 1000000; }
      break;
    }
    case GST_MESSAGE_TAG: {
      GstTagList *tags;
      MeloTags *mtags;

      /* Get tag list from message */
      gst_message_parse_tag (msg, &tags);

      /* Lock player mutex */
      g_mutex_lock (&priv->mutex);

      /* Merge tags */
      gst_tag_list_insert (priv->tag_list, tags, GST_TAG_MERGE_REPLACE);

      /* Fill MeloTags with GstTagList */
      mtags = melo_tags_new_from_gst_tag_list (priv->tag_list,
                                               MELO_TAGS_FIELDS_FULL);
      melo_player_status_take_tags (priv->status, mtags);

      /* Unlock player mutex */
      g_mutex_unlock (&priv->mutex);

      /* Free tag list */
      gst_tag_list_unref (tags);
      break;
    }
    case GST_MESSAGE_EOS:
      /* End of stream */
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
  gchar *name;

  /* Lock player mutex */
  g_mutex_lock (&priv->mutex);

  /* Stop pipeline */
  gst_element_set_state (priv->pipeline, GST_STATE_NULL);
  melo_player_status_unref (priv->status);
  gst_tag_list_unref (priv->tag_list);

  /* Replace URI */
  g_free (priv->uri);
  priv->uri = g_strdup (path);

  /* Create new status */
  name = g_path_get_basename (priv->uri);
  priv->status = melo_player_status_new (MELO_PLAYER_STATE_PLAYING, name);
  priv->tag_list = gst_tag_list_new_empty ();

  /* Set new location to src element */
  g_object_set (priv->src, "uri", path, NULL);
  gst_element_set_state (priv->pipeline, GST_STATE_PLAYING);

  /* Add new file to playlist */
  melo_player_add (player, name, name, path, TRUE);

  /* Unlock player mutex */
  g_mutex_unlock (&priv->mutex);

  return TRUE;
}

static MeloPlayerState
melo_player_file_set_state (MeloPlayer *player, MeloPlayerState state)
{
  MeloPlayerFilePrivate *priv = (MELO_PLAYER_FILE (player))->priv;

  /* Lock player mutex */
  g_mutex_lock (&priv->mutex);

  if (state == MELO_PLAYER_STATE_NONE) {
    gst_element_set_state (priv->pipeline, GST_STATE_NULL);
    melo_player_status_unref (priv->status);
    priv->status = melo_player_status_new (MELO_PLAYER_STATE_NONE, NULL);
  } else if (state == MELO_PLAYER_STATE_PLAYING)
    gst_element_set_state (priv->pipeline, GST_STATE_PLAYING);
  else if (state == MELO_PLAYER_STATE_PAUSED)
    gst_element_set_state (priv->pipeline, GST_STATE_PAUSED);
  else if (state == MELO_PLAYER_STATE_STOPPED)
    gst_element_set_state (priv->pipeline, GST_STATE_NULL);
  else
    state = priv->status->state;
  priv->status->state = state;

  /* Unlock player mutex */
  g_mutex_unlock (&priv->mutex);

  return state;
}

static gint
melo_player_file_set_pos (MeloPlayer *player, gint pos)
{
  MeloPlayerFilePrivate *priv = (MELO_PLAYER_FILE (player))->priv;
  gint64 time = (gint64) pos * 1000000;

  /* Seek to new position */
  if (!gst_element_seek (priv->pipeline, 1.0, GST_FORMAT_TIME,
                         GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_SET, time,
                         GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE))
    return -1;

  return melo_player_file_get_pos (player, NULL);
}

static MeloPlayerState
melo_player_file_get_state (MeloPlayer *player)
{
  return (MELO_PLAYER_FILE (player))->priv->status->state;
}

static gchar *
melo_player_file_get_name (MeloPlayer *player)
{
  MeloPlayerFilePrivate *priv = (MELO_PLAYER_FILE (player))->priv;
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
melo_player_file_get_pos (MeloPlayer *player, gint *duration)
{
  MeloPlayerFilePrivate *priv = (MELO_PLAYER_FILE (player))->priv;
  gint64 pos;

  /* Get duration */
  if (duration)
    *duration = priv->status->duration;

  /* Get length */
  if (!gst_element_query_position (priv->src, GST_FORMAT_TIME, &pos))
    pos = 0;

  return pos / 1000000;
}

static MeloPlayerStatus *
melo_player_file_get_status (MeloPlayer *player)
{
  MeloPlayerFilePrivate *priv = (MELO_PLAYER_FILE (player))->priv;
  MeloPlayerStatus *status;

  /* Lock player mutex */
  g_mutex_lock (&priv->mutex);

  /* Copy status */
  status = melo_player_status_ref (priv->status);
  status->pos = melo_player_file_get_pos (player, NULL);

  /* Unlock player mutex */
  g_mutex_unlock (&priv->mutex);

  return status;
}

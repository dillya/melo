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

#include <melo/melo_cover.h>

#define MELO_LOG_TAG "file_player"
#include <melo/melo_log.h>

#include "melo_file_player.h"

struct _MeloFilePlayer {
  GObject parent_instance;

  GstElement *pipeline;
  GstElement *src;
  guint bus_id;
};

MELO_DEFINE_PLAYER (MeloFilePlayer, melo_file_player)

static gboolean bus_cb (GstBus *bus, GstMessage *msg, gpointer data);
static void pad_added_cb (GstElement *src, GstPad *pad, GstElement *sink);

static bool melo_file_player_play (MeloPlayer *player, const char *path);
static bool melo_file_player_set_state (
    MeloPlayer *player, MeloPlayerState state);
static bool melo_file_player_set_position (
    MeloPlayer *player, unsigned int position);
static unsigned int melo_file_player_get_position (MeloPlayer *player);
static char *melo_file_player_get_asset (MeloPlayer *player, const char *id);

static void
melo_file_player_finalize (GObject *object)
{
  MeloFilePlayer *player = MELO_FILE_PLAYER (object);

  /* Remove bus watcher */
  g_source_remove (player->bus_id);

  /* Stop and release pipeline */
  gst_element_set_state (player->pipeline, GST_STATE_NULL);
  gst_object_unref (player->pipeline);

  /* Chain finalize */
  G_OBJECT_CLASS (melo_file_player_parent_class)->finalize (object);
}

static void
melo_file_player_class_init (MeloFilePlayerClass *klass)
{
  MeloPlayerClass *parent_class = MELO_PLAYER_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  /* Setup callbacks */
  parent_class->play = melo_file_player_play;
  parent_class->set_state = melo_file_player_set_state;
  parent_class->set_position = melo_file_player_set_position;
  parent_class->get_position = melo_file_player_get_position;
  parent_class->get_asset = melo_file_player_get_asset;

  /* Set finalize */
  object_class->finalize = melo_file_player_finalize;
}

static void
melo_file_player_init (MeloFilePlayer *self)
{
  GstElement *sink;
  GstCaps *caps;
  GstBus *bus;

  /* Create pipeline */
  self->pipeline = gst_pipeline_new (MELO_FILE_PLAYER_ID "_pipeline");
  self->src =
      gst_element_factory_make ("uridecodebin", MELO_FILE_PLAYER_ID "_src");
  sink = melo_player_get_sink (MELO_PLAYER (self), MELO_FILE_PLAYER_ID "_sink");
  gst_bin_add_many (GST_BIN (self->pipeline), self->src, sink, NULL);

  /* Handle only audio tracks */
  caps = gst_caps_from_string ("audio/x-raw(ANY)");
  g_object_set (self->src, "caps", caps, "expose-all-streams", FALSE, NULL);
  gst_caps_unref (caps);

  /* Add signal handler on new pad */
  g_signal_connect (self->src, "pad-added", G_CALLBACK (pad_added_cb), sink);

  /* Add a message handler */
  bus = gst_pipeline_get_bus (GST_PIPELINE (self->pipeline));
  self->bus_id = gst_bus_add_watch (bus, bus_cb, self);
  gst_object_unref (bus);
}

MeloFilePlayer *
melo_file_player_new ()
{
  return g_object_new (MELO_TYPE_FILE_PLAYER, "id", MELO_FILE_PLAYER_ID, "name",
      "Files", "description",
      "Play any media files (audio and/or video) from local or network devices",
      "icon", "fa:folder-open", NULL);
}

static gboolean
bus_cb (GstBus *bus, GstMessage *msg, gpointer user_data)
{
  MeloFilePlayer *fplayer = MELO_FILE_PLAYER (user_data);
  MeloPlayer *player = MELO_PLAYER (fplayer);

  /* Process bus message */
  switch (GST_MESSAGE_TYPE (msg)) {
  case GST_MESSAGE_DURATION_CHANGED:
  case GST_MESSAGE_ASYNC_DONE: {
    gint64 position = 0, duration = 0;

    /* Get position and duration */
    gst_element_query_position (fplayer->pipeline, GST_FORMAT_TIME, &position);
    gst_element_query_duration (fplayer->src, GST_FORMAT_TIME, &duration);

    /* Update player */
    melo_player_update_duration (
        player, position / 1000000, duration / 1000000);
    break;
  }
  case GST_MESSAGE_TAG: {
    GstTagList *tag_list;
    MeloTags *tags;

    /* Get tag list from message */
    gst_message_parse_tag (msg, &tag_list);

    /* Generate new tags */
    tags = melo_tags_new_from_taglist (G_OBJECT (player), tag_list);
    melo_player_update_tags (player, tags);

    /* Free tag list */
    gst_tag_list_unref (tag_list);
    break;
  }
  case GST_MESSAGE_STREAM_START:
    /* Playback is started */
    melo_player_update_status (
        player, MELO_PLAYER_STATE_PLAYING, MELO_PLAYER_STREAM_STATE_NONE, 0);
    break;
  case GST_MESSAGE_BUFFERING: {
    MeloPlayerStreamState state = MELO_PLAYER_STREAM_STATE_NONE;
    gint percent;

    /* Get current buffer state */
    gst_message_parse_buffering (msg, &percent);

    /* Still buffering */
    if (percent < 100)
      state = MELO_PLAYER_STREAM_STATE_BUFFERING;

    /* Update status */
    melo_player_update_stream_state (player, state, percent);
    break;
  }
  case GST_MESSAGE_ERROR: {
    GError *error;

    /* Stop pipeline on error */
    gst_element_set_state (fplayer->pipeline, GST_STATE_NULL);
    melo_player_update_state (player, MELO_PLAYER_STATE_STOPPED);

    /* Set error message */
    gst_message_parse_error (msg, &error, NULL);
    melo_player_error (player, error->message);
    g_error_free (error);
    break;
  }
  case GST_MESSAGE_EOS:
    /* Stop playing */
    gst_element_set_state (fplayer->pipeline, GST_STATE_NULL);
    melo_player_eos (player);
    break;
  default:
    break;
  }

  return TRUE;
}

static void
pad_added_cb (GstElement *src, GstPad *pad, GstElement *sink)
{
  GstStructure *str;
  GstPad *sink_pad;
  GstCaps *caps;

  /* Get sink pad from sink element */
  sink_pad = gst_element_get_static_pad (sink, "sink");
  if (GST_PAD_IS_LINKED (sink_pad)) {
    MELO_LOGE ("sink pad already linked");
    g_object_unref (sink_pad);
    return;
  }

  /* Only select audio pad */
  caps = gst_pad_query_caps (pad, NULL);
  str = gst_caps_get_structure (caps, 0);
  if (!g_strrstr (gst_structure_get_name (str), "audio")) {
    MELO_LOGW ("no audio sink pad");
    gst_object_unref (sink_pad);
    gst_caps_unref (caps);
    return;
  }
  gst_caps_unref (caps);

  /* Link elements */
  gst_pad_link (pad, sink_pad);
  g_object_unref (sink_pad);
}

static bool
melo_file_player_play (MeloPlayer *player, const char *path)
{
  MeloFilePlayer *fplayer = MELO_FILE_PLAYER (player);

  /* Stop previously playing file */
  gst_element_set_state (fplayer->pipeline, GST_STATE_NULL);

  /* Set new URI */
  g_object_set (fplayer->src, "uri", path, NULL);

  /* Start playing */
  gst_element_set_state (fplayer->pipeline, GST_STATE_PLAYING);

  return true;
}

static bool
melo_file_player_set_state (MeloPlayer *player, MeloPlayerState state)
{
  MeloFilePlayer *fplayer = MELO_FILE_PLAYER (player);

  if (state == MELO_PLAYER_STATE_PLAYING)
    gst_element_set_state (fplayer->pipeline, GST_STATE_PLAYING);
  else if (state == MELO_PLAYER_STATE_PAUSED)
    gst_element_set_state (fplayer->pipeline, GST_STATE_PAUSED);
  else
    gst_element_set_state (fplayer->pipeline, GST_STATE_NULL);

  return true;
}

static bool
melo_file_player_set_position (MeloPlayer *player, unsigned int position)
{
  MeloFilePlayer *fplayer = MELO_FILE_PLAYER (player);
  gint64 pos = (gint64) position * 1000000;

  /* Seek to new position */
  return gst_element_seek (fplayer->pipeline, 1.0, GST_FORMAT_TIME,
      GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_SET, pos, GST_SEEK_TYPE_NONE,
      GST_CLOCK_TIME_NONE);
}

static unsigned int
melo_file_player_get_position (MeloPlayer *player)
{
  MeloFilePlayer *fplayer = MELO_FILE_PLAYER (player);
  gint64 value;

  /* Get current position */
  if (!gst_element_query_position (fplayer->pipeline, GST_FORMAT_TIME, &value))
    return 0;

  return value / 1000000;
}

static char *
melo_file_player_get_asset (MeloPlayer *player, const char *id)
{
  return melo_cover_cache_get_path (id);
}

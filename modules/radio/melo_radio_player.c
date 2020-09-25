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

#include <melo/melo_http_client.h>

#define MELO_LOG_TAG "radio_player"
#include <melo/melo_log.h>

#include "melo_radio_player.h"

struct _MeloRadioPlayer {
  GObject parent_instance;

  GstElement *pipeline;
  GstElement *src;
  guint bus_id;
};

MELO_DEFINE_PLAYER (MeloRadioPlayer, melo_radio_player)

static gboolean bus_cb (GstBus *bus, GstMessage *msg, gpointer data);
static void pad_added_cb (GstElement *src, GstPad *pad, GstElement *sink);

static bool melo_radio_player_play (MeloPlayer *player, const char *path);
static bool melo_radio_player_set_state (
    MeloPlayer *player, MeloPlayerState state);
static unsigned int melo_radio_player_get_position (MeloPlayer *player);

static void
melo_radio_player_finalize (GObject *object)
{
  MeloRadioPlayer *player = MELO_RADIO_PLAYER (object);

  /* Remove bus watcher */
  g_source_remove (player->bus_id);

  /* Stop and release pipeline */
  gst_element_set_state (player->pipeline, GST_STATE_NULL);
  gst_object_unref (player->pipeline);

  /* Chain finalize */
  G_OBJECT_CLASS (melo_radio_player_parent_class)->finalize (object);
}

static void
melo_radio_player_class_init (MeloRadioPlayerClass *klass)
{
  MeloPlayerClass *parent_class = MELO_PLAYER_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  /* Setup callbacks */
  parent_class->play = melo_radio_player_play;
  parent_class->set_state = melo_radio_player_set_state;
  parent_class->get_position = melo_radio_player_get_position;

  /* Set finalize */
  object_class->finalize = melo_radio_player_finalize;
}

static void
melo_radio_player_init (MeloRadioPlayer *self)
{
  GstElement *sink;
  GstBus *bus;

  /* Create pipeline */
  self->pipeline = gst_pipeline_new (MELO_RADIO_PLAYER_ID "_pipeline");
  self->src =
      gst_element_factory_make ("uridecodebin", MELO_RADIO_PLAYER_ID "_src");
  sink =
      melo_player_get_sink (MELO_PLAYER (self), MELO_RADIO_PLAYER_ID "_sink");
  gst_bin_add_many (GST_BIN (self->pipeline), self->src, sink, NULL);

  /* Add signal handler on new pad */
  g_signal_connect (self->src, "pad-added", G_CALLBACK (pad_added_cb), sink);

  /* Add a message handler */
  bus = gst_pipeline_get_bus (GST_PIPELINE (self->pipeline));
  self->bus_id = gst_bus_add_watch (bus, bus_cb, self);
  gst_object_unref (bus);
}

MeloRadioPlayer *
melo_radio_player_new ()
{
  return g_object_new (MELO_TYPE_RADIO_PLAYER, "id", MELO_RADIO_PLAYER_ID,
      "name", "Radio", "description",
      "Play any webradio stream (shoutcast / icecast)", "icon",
      "fa:broadcast-tower", NULL);
}

static gboolean
bus_cb (GstBus *bus, GstMessage *msg, gpointer user_data)
{
  MeloRadioPlayer *rplayer = MELO_RADIO_PLAYER (user_data);
  MeloPlayer *player = MELO_PLAYER (rplayer);

  /* Process bus message */
  switch (GST_MESSAGE_TYPE (msg)) {
  case GST_MESSAGE_TAG: {
    GstTagList *tag_list;
    MeloTags *tags;
    char *title;

    /* Get tag list from message */
    gst_message_parse_tag (msg, &tag_list);

    /* Get title */
    if (gst_tag_list_get_string (tag_list, GST_TAG_TITLE, &title)) {
      char *artist;

      MELO_LOGD ("radio title: %s", title);

      /* Find title / artist separator */
      artist = strstr (title, " - ");
      if (artist) {
        *artist = '\0';
        artist += 3;
      }

      /* Create new tags */
      tags = melo_tags_new ();
      if (tags) {
        melo_tags_set_title (tags, title);
        melo_tags_set_artist (tags, artist ? artist : "");

        /* Update player tags */
        melo_player_update_media (
            player, NULL, tags, MELO_TAGS_MERGE_FLAG_SKIP_COVER);
      }
    }

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
    gst_element_set_state (rplayer->pipeline, GST_STATE_NULL);
    melo_player_update_state (player, MELO_PLAYER_STATE_STOPPED);

    /* Set error message */
    gst_message_parse_error (msg, &error, NULL);
    melo_player_error (player, error->message);
    g_error_free (error);
    break;
  }
  case GST_MESSAGE_EOS:
    /* Stop playing */
    gst_element_set_state (rplayer->pipeline, GST_STATE_NULL);
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

static void
m3u_cb (MeloHttpClient *client, unsigned int code, const char *data,
    size_t size, void *user_data)
{
  MeloRadioPlayer *player = user_data;
  bool ret = false;

  /* Parse response */
  if (code == 200 && data && size) {
    const char *uri = data;
    unsigned int len = 0;

    /* Get first line from file */
    while (size--) {
      /* End of line */
      if (*data == '\n') {
        if (*uri != '#')
          break;
        len = 0;
        uri = data + 1;
      } else if (*data == '\0')
        break;
      data++;
      len++;
    }
    uri = g_strndup (uri, len);

    /* Play URI */
    if (uri) {
      /* Set new radio URI */
      g_object_set (player->src, "uri", uri, NULL);

      /* Start playing */
      gst_element_set_state (player->pipeline, GST_STATE_PLAYING);
      ret = true;
    }
  }

  /* Cannot play radio */
  if (!ret)
    melo_player_error (MELO_PLAYER (player), "failed to get m3u file");

  /* Release client */
  g_object_unref (client);
}

static bool
melo_radio_player_play (MeloPlayer *player, const char *path)
{
  MeloRadioPlayer *rplayer = MELO_RADIO_PLAYER (player);

  /* Stop previously playing radio */
  gst_element_set_state (rplayer->pipeline, GST_STATE_NULL);

  /* Check for m3u files */
  if (g_str_has_suffix (path, ".m3u")) {
    MeloHttpClient *client;

    /* Create HTTP client */
    client = melo_http_client_new (NULL);
    if (!client)
      return false;

    /* Get M3U file */
    if (!melo_http_client_get (client, path, m3u_cb, player)) {
      g_object_unref (client);
      return false;
    }
  } else {
    /* Set new radio URI */
    g_object_set (rplayer->src, "uri", path, NULL);

    /* Start playing */
    gst_element_set_state (rplayer->pipeline, GST_STATE_PLAYING);
  }

  return true;
}

static bool
melo_radio_player_set_state (MeloPlayer *player, MeloPlayerState state)
{
  MeloRadioPlayer *rplayer = MELO_RADIO_PLAYER (player);

  if (state == MELO_PLAYER_STATE_PLAYING)
    gst_element_set_state (rplayer->pipeline, GST_STATE_PLAYING);
  else if (state == MELO_PLAYER_STATE_PAUSED)
    gst_element_set_state (rplayer->pipeline, GST_STATE_PAUSED);
  else
    gst_element_set_state (rplayer->pipeline, GST_STATE_NULL);

  return true;
}

static unsigned int
melo_radio_player_get_position (MeloPlayer *player)
{
  MeloRadioPlayer *rplayer = MELO_RADIO_PLAYER (player);
  gint64 value;

  /* Get current position */
  if (!gst_element_query_position (rplayer->pipeline, GST_FORMAT_TIME, &value))
    return 0;

  return value / 1000000;
}

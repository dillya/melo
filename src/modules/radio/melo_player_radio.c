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

#include <string.h>

#include <gst/gst.h>

#include "melo_event.h"
#include "melo_player_radio.h"

static gboolean bus_call (GstBus *bus, GstMessage *msg, gpointer data);
static void pad_added_handler (GstElement *src, GstPad *pad, GstElement *sink);

static gboolean melo_player_radio_load (MeloPlayer *player, const gchar *path,
                                       const gchar *name, MeloTags *tags,
                                       gboolean insert, gboolean stopped);
static gboolean melo_player_radio_play (MeloPlayer *player, const gchar *path,
                                       const gchar *name, MeloTags *tags,
                                       gboolean insert);
static MeloPlayerState melo_player_radio_set_state (MeloPlayer *player,
                                                   MeloPlayerState state);
static gint melo_player_radio_set_pos (MeloPlayer *player, gint pos);
static gdouble melo_player_radio_set_volume (MeloPlayer *player,
                                             gdouble volume);
static gboolean melo_player_radio_set_mute (MeloPlayer *player, gboolean mute);

static MeloPlayerState melo_player_radio_get_state (MeloPlayer *player);
static gchar *melo_player_radio_get_name (MeloPlayer *player);
static gint melo_player_radio_get_pos (MeloPlayer *player, gint *duration);
static gdouble melo_player_radio_get_volume (MeloPlayer *player);
static MeloPlayerStatus *melo_player_radio_get_status (MeloPlayer *player);
static gboolean melo_player_radio_get_cover (MeloPlayer *player, GBytes **data,
                                             gchar **type);

struct _MeloPlayerRadioPrivate {
  GMutex mutex;
  MeloPlayerStatus *status;
  gboolean load;

  /* Gstreamer pipeline */
  GstElement *pipeline;
  GstElement *src;
  GstElement *vol;
  guint bus_watch_id;
  gchar *title;

  /* Browser tags */
  MeloTags *btags;
};

G_DEFINE_TYPE_WITH_PRIVATE (MeloPlayerRadio, melo_player_radio, MELO_TYPE_PLAYER)

static void
melo_player_radio_finalize (GObject *gobject)
{
  MeloPlayerRadio *pradio = MELO_PLAYER_RADIO (gobject);
  MeloPlayerRadioPrivate *priv =
                                melo_player_radio_get_instance_private (pradio);

  /* Stop pipeline */
  gst_element_set_state (priv->pipeline, GST_STATE_NULL);

  /* Remove message handler */
  g_source_remove (priv->bus_watch_id);

  /* Free gstreamer pipeline */
  g_object_unref (priv->pipeline);

  /* Unref browser tags */
  if (priv->btags)
    melo_tags_unref (priv->btags);

  /* Free current title */
  g_free (priv->title);

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
  pclass->load = melo_player_radio_load;
  pclass->play = melo_player_radio_play;
  pclass->set_state = melo_player_radio_set_state;
  pclass->set_pos = melo_player_radio_set_pos;
  pclass->set_volume = melo_player_radio_set_volume;
  pclass->set_mute = melo_player_radio_set_mute;

  /* Status */
  pclass->get_state = melo_player_radio_get_state;
  pclass->get_name = melo_player_radio_get_name;
  pclass->get_pos = melo_player_radio_get_pos;
  pclass->get_volume = melo_player_radio_get_volume;
  pclass->get_status = melo_player_radio_get_status;
  pclass->get_cover = melo_player_radio_get_cover;

  /* Add custom finalize() function */
  object_class->finalize = melo_player_radio_finalize;
}

static void
melo_player_radio_init (MeloPlayerRadio *self)
{
  MeloPlayerRadioPrivate *priv = melo_player_radio_get_instance_private (self);
  GstElement *convert, *sink;
  GstBus *bus;

  self->priv = priv;

  /* Init player mutex */
  g_mutex_init (&priv->mutex);

  /* Create new status handler */
  priv->status = melo_player_status_new (NULL, MELO_PLAYER_STATE_NONE, NULL);
  priv->status->volume = 1.0;

  /* Create pipeline */
  priv->pipeline = gst_pipeline_new ("radio_player_pipeline");
  priv->src = gst_element_factory_make ("uridecodebin",
                                        "radio_player_uridecodebin");
  convert = gst_element_factory_make ("audioconvert",
                                      "file_player_audioconvert");
  priv->vol = gst_element_factory_make ("volume", "radio_player_volume");
  sink = gst_element_factory_make ("autoaudiosink",
                                   "radio_player_autoaudiosink");
  gst_bin_add_many (GST_BIN (priv->pipeline), priv->src, convert, priv->vol,
                    sink, NULL);
  gst_element_link_many (convert, priv->vol, sink, NULL);

  /* Add signal handler on new pad */
  g_signal_connect(priv->src, "pad-added",
                   G_CALLBACK (pad_added_handler), convert);

  /* Add a message handler */
  bus = gst_pipeline_get_bus (GST_PIPELINE (priv->pipeline));
  priv->bus_watch_id = gst_bus_add_watch (bus, bus_call, self);
  gst_object_unref (bus);
}

static gboolean
bus_call (GstBus *bus, GstMessage *msg, gpointer data)
{
  MeloPlayerRadio *pradio = MELO_PLAYER_RADIO (data);
  MeloPlayer *player = MELO_PLAYER (pradio);
  MeloPlayerRadioPrivate *priv = pradio->priv;
  GError *error;

  /* Process bus message */
  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_TAG: {
      GstTagList *tags;
      MeloTags *mtags;
      gchar *artist, *title;

      /* Get tag list from message */
      gst_message_parse_tag (msg, &tags);

      /* Lock player mutex */
      g_mutex_lock (&priv->mutex);

      /* Fill MeloTags with GstTagList */
      mtags = melo_tags_new_from_gst_tag_list (tags, MELO_TAGS_FIELDS_FULL);

      /* Merge with browser tags */
      if (priv->btags)
        melo_tags_merge (mtags, priv->btags);

      /* Set cover URL */
      if (melo_tags_has_cover (mtags))
        melo_tags_set_cover_url (mtags, G_OBJECT (pradio), NULL, NULL);

      /* New title */
      if (mtags->title && g_strcmp0 (priv->title, mtags->title)) {
        /* Save new title */
        g_free (priv->title);
        priv->title = g_strdup (mtags->title);

        /* Split title */
        if (!mtags->artist) {
          /* Get title space */
          artist = mtags->title;
          title = strstr (mtags->title, " - ");
          if (title) {
            mtags->title = g_strdup (title + 3);
            mtags->artist = g_strndup (artist, title - artist);
            g_free (artist);
          }
        }

        /* Add title to playlist */
        melo_playlist_add (player->playlist, NULL, mtags->title, mtags, TRUE);
      }

      /* Set tags to player status */
      melo_player_status_take_tags (priv->status, mtags, TRUE);

      /* Unlock player mutex */
      g_mutex_unlock (&priv->mutex);

      /* Free tag list */
      gst_tag_list_unref (tags);
      break;
    }
    case GST_MESSAGE_STREAM_START:
      /* Playback is started */
      g_mutex_lock (&priv->mutex);
      melo_player_status_set_state (priv->status,
                                    priv->load ? MELO_PLAYER_STATE_PAUSED :
                                                 MELO_PLAYER_STATE_PLAYING);
      g_mutex_unlock (&priv->mutex);
      break;
    case GST_MESSAGE_BUFFERING: {
      gint percent;

      /* Get current buffer state */
      gst_message_parse_buffering (msg, &percent);

      /* Update status */
      g_mutex_lock (&priv->mutex);
      if (percent < 100)
        melo_player_status_set_buffering (priv->status,
                               priv->load ? MELO_PLAYER_STATE_PAUSED_BUFFERING :
                                            MELO_PLAYER_STATE_BUFFERING,
                               percent);
      else
        melo_player_status_set_state (priv->status,
                                      priv->load ? MELO_PLAYER_STATE_PAUSED :
                                                   MELO_PLAYER_STATE_PLAYING);
      g_mutex_unlock (&priv->mutex);

      break;
    }
    case GST_MESSAGE_EOS:
      /* Stop playing */
      g_mutex_lock (&priv->mutex);
      gst_element_set_state (priv->pipeline, GST_STATE_NULL);
      melo_player_status_set_state (priv->status, MELO_PLAYER_STATE_STOPPED);
      g_mutex_unlock (&priv->mutex);
      break;
    case GST_MESSAGE_ERROR:
      /* Lock player mutex */
      g_mutex_lock (&priv->mutex);

      /* Update error message */
      gst_message_parse_error (msg, &error, NULL);
      melo_player_status_set_error (priv->status, error->message, TRUE);
      g_error_free (error);

      /* Unlock player mutex */
      g_mutex_unlock (&priv->mutex);
      break;
    default:
      ;
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
melo_player_radio_setup (MeloPlayer *player, const gchar *path,
                         const gchar *name, MeloTags *tags, gboolean insert,
                         MeloPlayerState state)
{
  MeloPlayerRadioPrivate *priv = (MELO_PLAYER_RADIO (player))->priv;

  /* Set default name */
  if (!name)
    name = "Unknown radio";

  /* Stop pipeline */
  gst_element_set_state (priv->pipeline, GST_STATE_READY);

  /* Replace status */
  if (priv->btags) {
    melo_tags_unref (priv->btags);
    priv->btags = NULL;
  }
  g_free (priv->title);
  priv->title = NULL;
  melo_player_status_unref (priv->status);
  melo_playlist_empty (player->playlist);
  priv->status = melo_player_status_new (player, state, name);
  g_object_get (priv->vol, "volume", &priv->status->volume, NULL);
  if (tags) {
    priv->btags = melo_tags_ref (tags);
    melo_player_status_set_tags (priv->status, tags, FALSE);
  }

  /* Send new player status event */
  melo_event_player_status (melo_player_get_id (player),
                            melo_player_status_ref (priv->status));


  /* Set new location to src element */
  g_object_set (priv->src, "uri", path, NULL);
  if (state == MELO_PLAYER_STATE_LOADING) {
    priv->load = FALSE;
    gst_element_set_state (priv->pipeline, GST_STATE_PLAYING);
  } else if (state == MELO_PLAYER_STATE_PAUSED_LOADING) {
    priv->load = TRUE;
    gst_element_set_state (priv->pipeline, GST_STATE_PAUSED);
  }

  return TRUE;
}

static gboolean
melo_player_radio_load (MeloPlayer *player, const gchar *path,
                        const gchar *name, MeloTags *tags, gboolean insert,
                        gboolean stopped)
{
  MeloPlayerRadioPrivate *priv = (MELO_PLAYER_RADIO (player))->priv;
  gboolean ret;

  /* Lock player mutex */
  g_mutex_lock (&priv->mutex);

  /* Setup pipeline in paused or stopped state */
  ret = melo_player_radio_setup (player, path, name, tags, insert,
                                 stopped ? MELO_PLAYER_STATE_STOPPED :
                                           MELO_PLAYER_STATE_PAUSED_LOADING);

  /* Unlock player mutex */
  g_mutex_unlock (&priv->mutex);

  return ret;
}

static gboolean
melo_player_radio_play (MeloPlayer *player, const gchar *path,
                        const gchar *name, MeloTags *tags, gboolean insert)
{
  MeloPlayerRadioPrivate *priv = (MELO_PLAYER_RADIO (player))->priv;
  gboolean ret;

  /* Lock player mutex */
  g_mutex_lock (&priv->mutex);

  /* Setup pipeline and play */
  ret = melo_player_radio_setup (player, path, name, tags, insert,
                                 MELO_PLAYER_STATE_LOADING);

  /* Unlock player mutex */
  g_mutex_unlock (&priv->mutex);

  return ret;
}

static MeloPlayerState
melo_player_radio_set_state (MeloPlayer *player, MeloPlayerState state)
{
  MeloPlayerRadioPrivate *priv = (MELO_PLAYER_RADIO (player))->priv;

  /* Lock player mutex */
  g_mutex_lock (&priv->mutex);

  if (state == MELO_PLAYER_STATE_NONE) {
    gst_element_set_state (priv->pipeline, GST_STATE_NULL);
    melo_player_status_unref (priv->status);
    priv->status = melo_player_status_new (player, MELO_PLAYER_STATE_NONE,
                                           NULL);
  } else if (state == MELO_PLAYER_STATE_PLAYING)
    gst_element_set_state (priv->pipeline, GST_STATE_PLAYING);
  else if (state == MELO_PLAYER_STATE_PAUSED)
    gst_element_set_state (priv->pipeline, GST_STATE_PAUSED);
  else if (state == MELO_PLAYER_STATE_STOPPED)
    gst_element_set_state (priv->pipeline, GST_STATE_NULL);
  else
    state = priv->status->state;
  melo_player_status_set_state (priv->status, state);
  priv->load = FALSE;

  /* Unlock player mutex */
  g_mutex_unlock (&priv->mutex);

  return state;
}

static gint
melo_player_radio_set_pos (MeloPlayer *player, gint pos)
{
  return -1;
}

static gdouble
melo_player_radio_set_volume (MeloPlayer *player, gdouble volume)
{
  MeloPlayerRadioPrivate *priv = (MELO_PLAYER_RADIO (player))->priv;

  /* Lock player mutex */
  g_mutex_lock (&priv->mutex);

  /* Set volume */
  melo_player_status_set_volume (priv->status, volume);

  /* Unlock player mutex */
  g_mutex_unlock (&priv->mutex);

  /* Set pipeline volume */
  g_object_set (priv->vol, "volume", volume, NULL);

  return volume;
}

static gboolean
melo_player_radio_set_mute (MeloPlayer *player, gboolean mute)
{
  MeloPlayerRadioPrivate *priv = (MELO_PLAYER_RADIO (player))->priv;

  /* Lock player mutex */
  g_mutex_lock (&priv->mutex);

  /* Set mute */
  melo_player_status_set_mute (priv->status, mute);

  /* Unlock player mutex */
  g_mutex_unlock (&priv->mutex);

  /* Mute pipeline */
  g_object_set (priv->vol, "mute", mute, NULL);

  return mute;
}

static MeloPlayerState
melo_player_radio_get_state (MeloPlayer *player)
{
  MeloPlayerRadioPrivate *priv = (MELO_PLAYER_RADIO (player))->priv;
  MeloPlayerState state;

  /* Lock player mutex */
  g_mutex_lock (&priv->mutex);

  /* Get state */
  state = priv->status->state;

  /* Unlock player mutex */
  g_mutex_unlock (&priv->mutex);

  return state;
}

static gchar *
melo_player_radio_get_name (MeloPlayer *player)
{
  MeloPlayerRadioPrivate *priv = (MELO_PLAYER_RADIO (player))->priv;
  gchar *name = NULL;

  /* Lock player mutex */
  g_mutex_lock (&priv->mutex);

  /* Copy name */
  name = melo_player_status_get_name (priv->status);

  /* Unlock player mutex */
  g_mutex_unlock (&priv->mutex);

  return name;
}

static gint
melo_player_radio_get_pos (MeloPlayer *player, gint *duration)
{
  MeloPlayerRadioPrivate *priv = (MELO_PLAYER_RADIO (player))->priv;
  gint64 pos;

  /* Lock player mutex */
  g_mutex_lock (&priv->mutex);

  /* Get duration */
  if (duration)
    *duration = priv->status->duration;

  /* Unlock player mutex */
  g_mutex_unlock (&priv->mutex);

  /* Get length */
  if (!gst_element_query_position (priv->src, GST_FORMAT_TIME, &pos))
    pos = 0;

  return pos / 1000000;
}

static gdouble
melo_player_radio_get_volume (MeloPlayer *player)
{
  MeloPlayerRadioPrivate *priv = (MELO_PLAYER_RADIO (player))->priv;
  gdouble volume;

  /* Lock player mutex */
  g_mutex_lock (&priv->mutex);

  /* Get duration */
  volume = priv->status->volume;

  /* Unlock player mutex */
  g_mutex_unlock (&priv->mutex);

  return volume;
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

  /* Update position */
  status->pos = melo_player_radio_get_pos (player, NULL);

  return status;
}

static gboolean
melo_player_radio_get_cover (MeloPlayer *player, GBytes **data, gchar **type)
{
  MeloPlayerRadioPrivate *priv = (MELO_PLAYER_RADIO (player))->priv;
  MeloTags *tags;

  /* Lock player mutex */
  g_mutex_lock (&priv->mutex);

  /* Copy status */
  tags = melo_player_status_get_tags (priv->status);
  if (tags) {
    *data = melo_tags_get_cover (tags, type);
    melo_tags_unref (tags);
  }

  /* Unlock player mutex */
  g_mutex_unlock (&priv->mutex);

  return TRUE;
}

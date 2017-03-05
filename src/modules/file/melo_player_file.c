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

static gboolean melo_player_file_add (MeloPlayer *player, const gchar *path,
                                      const gchar *name, MeloTags *tags);
static gboolean melo_player_file_load (MeloPlayer *player, const gchar *path,
                                       const gchar *name, MeloTags *tags,
                                       gboolean insert, gboolean stopped);
static gboolean melo_player_file_play (MeloPlayer *player, const gchar *path,
                                       const gchar *name, MeloTags *tags,
                                       gboolean insert);
static gboolean melo_player_file_prev (MeloPlayer *player);
static gboolean melo_player_file_next (MeloPlayer *player);
static MeloPlayerState melo_player_file_set_state (MeloPlayer *player,
                                                   MeloPlayerState state);
static gint melo_player_file_set_pos (MeloPlayer *player, gint pos);
static gdouble melo_player_file_set_volume (MeloPlayer *player, gdouble volume);
static gboolean melo_player_file_set_mute (MeloPlayer *player, gboolean mute);

static MeloPlayerState melo_player_file_get_state (MeloPlayer *player);
static gchar *melo_player_file_get_name (MeloPlayer *player);
static gint melo_player_file_get_pos (MeloPlayer *player, gint *duration);
static gdouble melo_player_file_get_volume (MeloPlayer *player);
static MeloPlayerStatus *melo_player_file_get_status (MeloPlayer *player);
static gboolean melo_player_file_get_cover (MeloPlayer *player, GBytes **data,
                                            gchar **type);

struct _MeloPlayerFilePrivate {
  GMutex mutex;

  /* Status */
  MeloPlayerStatus *status;
  gchar *uri;
  gboolean load;

  /* Gstreamer pipeline */
  GstElement *pipeline;
  GstElement *src;
  GstElement *vol;
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
  melo_player_status_unref (priv->status);

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

  /* Control */
  pclass->add = melo_player_file_add;
  pclass->load = melo_player_file_load;
  pclass->play = melo_player_file_play;
  pclass->set_state = melo_player_file_set_state;
  pclass->prev = melo_player_file_prev;
  pclass->next = melo_player_file_next;
  pclass->set_pos = melo_player_file_set_pos;
  pclass->set_volume = melo_player_file_set_volume;
  pclass->set_mute = melo_player_file_set_mute;

  /* Status */
  pclass->get_state = melo_player_file_get_state;
  pclass->get_name = melo_player_file_get_name;
  pclass->get_pos = melo_player_file_get_pos;
  pclass->get_volume = melo_player_file_get_volume;
  pclass->get_status = melo_player_file_get_status;
  pclass->get_cover = melo_player_file_get_cover;

  /* Add custom finalize() function */
  object_class->finalize = melo_player_file_finalize;
}

static void
melo_player_file_init (MeloPlayerFile *self)
{
  MeloPlayerFilePrivate *priv = melo_player_file_get_instance_private (self);
  GstElement *convert, *sink;
  GstBus *bus;

  self->priv = priv;
  priv->uri = NULL;

  /* Init player mutex */
  g_mutex_init (&priv->mutex);

  /* Create new status handler */
  priv->status = melo_player_status_new (MELO_PLAYER_STATE_NONE, NULL);
  priv->status->volume = 1.0;

  /* Create pipeline */
  priv->pipeline = gst_pipeline_new ("file_player_pipeline");
  priv->src = gst_element_factory_make ("uridecodebin",
                                        "file_player_uridecodebin");
  convert = gst_element_factory_make ("audioconvert",
                                      "file_player_audioconvert");
  priv->vol = gst_element_factory_make ("volume", "file_player_volume");
  sink = gst_element_factory_make ("autoaudiosink",
                                   "file_player_autoaudiosink");
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
  MeloPlayerFile *pfile = MELO_PLAYER_FILE (data);
  MeloPlayerFilePrivate *priv = pfile->priv;
  MeloPlayer *player = MELO_PLAYER (pfile);
  GError *error;

  /* Process bus message */
  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_DURATION_CHANGED:
    case GST_MESSAGE_ASYNC_DONE: {
      gint64 duration;

      /* Get duration */
      if (gst_element_query_duration (priv->src, GST_FORMAT_TIME, &duration)) {
        g_mutex_lock (&priv->mutex);
        priv->status->duration = duration / 1000000;
        g_mutex_unlock (&priv->mutex);
      }

      break;
    }
    case GST_MESSAGE_TAG: {
      MeloTags *mtags, *otags;
      GstTagList *tags;

      /* Get tag list from message */
      gst_message_parse_tag (msg, &tags);

      /* Lock player mutex */
      g_mutex_lock (&priv->mutex);

      /* Fill MeloTags with GstTagList */
      mtags = melo_tags_new_from_gst_tag_list (tags, MELO_TAGS_FIELDS_FULL);

      /* Merge with old tags */
      otags = melo_player_status_get_tags (priv->status);
      if (otags) {
        melo_tags_merge (mtags, otags);
        melo_tags_unref (otags);
      }

      /* Set tags to status */
      if (melo_tags_has_cover (mtags))
        melo_tags_set_cover_url (mtags, G_OBJECT (pfile), NULL, NULL);
      melo_player_status_take_tags (priv->status, mtags);

      /* Unlock player mutex */
      g_mutex_unlock (&priv->mutex);

      /* Free tag list */
      gst_tag_list_unref (tags);
      break;
    }
    case GST_MESSAGE_STREAM_START:
      /* Playback is started */
      g_mutex_lock (&priv->mutex);
      priv->status->state = priv->load ? MELO_PLAYER_STATE_PAUSED :
                                         MELO_PLAYER_STATE_PLAYING;
      g_mutex_unlock (&priv->mutex);
      break;
    case GST_MESSAGE_BUFFERING: {
      gint percent;

      /* Get current buffer state */
      gst_message_parse_buffering (msg, &percent);

      /* Update status */
      g_mutex_lock (&priv->mutex);
      if (percent < 100)
        priv->status->state = priv->load ? MELO_PLAYER_STATE_PAUSED_BUFFERING  :
                                           MELO_PLAYER_STATE_BUFFERING;
      else
        priv->status->state = priv->load ? MELO_PLAYER_STATE_PAUSED :
                                           MELO_PLAYER_STATE_PLAYING;
      priv->status->buffer_percent = percent;
      g_mutex_unlock (&priv->mutex);
      break;
    }
    case GST_MESSAGE_EOS:
      /* Play next media */
      if (!melo_player_file_next (player)) {
        /* Stop playing */
        g_mutex_lock (&priv->mutex);
        gst_element_set_state (priv->pipeline, GST_STATE_NULL);
        priv->status->state = MELO_PLAYER_STATE_STOPPED;
        g_mutex_unlock (&priv->mutex);
      }
      break;

    case GST_MESSAGE_ERROR:
      /* Lock player mutex */
      g_mutex_lock (&priv->mutex);

      /* End of stream */
      priv->status->state = MELO_PLAYER_STATE_ERROR;

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
melo_player_file_add (MeloPlayer *player, const gchar *path, const gchar *name,
                      MeloTags *tags)
{
  gchar *_name = NULL;

  if (!player->playlist)
    return FALSE;

  /* Extract file name from URI */
  if (!name) {
    gchar *escaped = escaped = g_path_get_basename (path);
    _name = g_uri_unescape_string (escaped, NULL);
    g_free (escaped);
    name = _name;
  }

  /* Add URI to playlist */
  melo_playlist_add (player->playlist, path, name, tags, FALSE);
  g_free (_name);

  return TRUE;
}

static gboolean
melo_player_file_setup (MeloPlayer *player, const gchar *path,
                        const gchar *name, MeloTags *tags, gboolean insert,
                        MeloPlayerState state)
{
  MeloPlayerFilePrivate *priv = (MELO_PLAYER_FILE (player))->priv;
  gchar *_name = NULL;

  /* Stop pipeline */
  gst_element_set_state (priv->pipeline, GST_STATE_READY);
  melo_player_status_unref (priv->status);

  /* Replace URI */
  g_free (priv->uri);
  priv->uri = g_strdup (path);

  /* Create new status */
  if (!name) {
    gchar *escaped = escaped = g_path_get_basename (priv->uri);
    _name = g_uri_unescape_string (escaped, NULL);
    g_free (escaped);
    name = _name;
  }
  priv->status = melo_player_status_new (state, name);
  g_object_get (priv->vol, "volume", &priv->status->volume, NULL);
  if (tags)
    melo_player_status_set_tags (priv->status, tags);

  /* Set new location to src element */
  g_object_set (priv->src, "uri", path, NULL);
  if (state == MELO_PLAYER_STATE_LOADING) {
    priv->load = FALSE;
    gst_element_set_state (priv->pipeline, GST_STATE_PLAYING);
  } else if (state == MELO_PLAYER_STATE_PAUSED_LOADING) {
    priv->load = TRUE;
    gst_element_set_state (priv->pipeline, GST_STATE_PAUSED);
  }

  /* Add new file to playlist */
  if (insert && player->playlist)
    melo_playlist_add (player->playlist, path, name, tags, TRUE);
  g_free (_name);

  return TRUE;
}

static gboolean
melo_player_file_load (MeloPlayer *player, const gchar *path, const gchar *name,
                       MeloTags *tags, gboolean insert, gboolean stopped)
{
  MeloPlayerFilePrivate *priv = (MELO_PLAYER_FILE (player))->priv;
  gboolean ret;

  /* Lock player mutex */
  g_mutex_lock (&priv->mutex);

  /* Load media file in paused or stopped state */
  ret = melo_player_file_setup (player, path, name, tags, insert,
                                stopped ? MELO_PLAYER_STATE_STOPPED :
                                          MELO_PLAYER_STATE_PAUSED_LOADING);

  /* Unlock player mutex */
  g_mutex_unlock (&priv->mutex);

  return ret;
}

static gboolean
melo_player_file_play (MeloPlayer *player, const gchar *path, const gchar *name,
                       MeloTags *tags, gboolean insert)
{
  MeloPlayerFilePrivate *priv = (MELO_PLAYER_FILE (player))->priv;
  gboolean ret;

  /* Lock player mutex */
  g_mutex_lock (&priv->mutex);

  /* Play media file */
  ret = melo_player_file_setup (player, path, name, tags, insert,
                                MELO_PLAYER_STATE_LOADING);

  /* Unlock player mutex */
  g_mutex_unlock (&priv->mutex);

  return ret;
}

static gboolean
melo_player_file_prev (MeloPlayer *player)
{
  MeloTags *tags = NULL;
  gchar *name = NULL;
  gboolean ret;
  gchar *path;

  g_return_val_if_fail (player->playlist, FALSE);

  /* Get URI for previous media */
  path = melo_playlist_get_prev (player->playlist, &name, &tags, TRUE);
  if (!path)
    return FALSE;

  /* Play file */
  ret = melo_player_file_play (player, path, name, tags, FALSE);
  melo_tags_unref (tags);
  g_free (name);
  g_free (path);

  return ret;
}

static gboolean
melo_player_file_next (MeloPlayer *player)
{
  MeloTags *tags = NULL;
  gchar *name = NULL;
  gboolean ret;
  gchar *path;

  g_return_val_if_fail (player->playlist, FALSE);

  /* Get URI for next media */
  path = melo_playlist_get_next (player->playlist, &name, &tags, TRUE);
  if (!path)
    return FALSE;

  /* Play file */
  ret = melo_player_file_play (player, path, name, tags, FALSE);
  melo_tags_unref (tags);
  g_free (name);
  g_free (path);

  return ret;
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
  priv->load = FALSE;

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

static gdouble
melo_player_file_set_volume (MeloPlayer *player, gdouble volume)
{
  MeloPlayerFilePrivate *priv = (MELO_PLAYER_FILE (player))->priv;

  /* Lock player mutex */
  g_mutex_lock (&priv->mutex);

  /* Set volume */
  priv->status->volume = volume;

  /* Unlock player mutex */
  g_mutex_unlock (&priv->mutex);

  /* Set pipeline volume */
  g_object_set (priv->vol, "volume", volume, NULL);

  return volume;
}

static gboolean
melo_player_file_set_mute (MeloPlayer *player, gboolean mute)
{
  MeloPlayerFilePrivate *priv = (MELO_PLAYER_FILE (player))->priv;

  /* Lock player mutex */
  g_mutex_lock (&priv->mutex);

  /* Set mute */
  priv->status->mute = mute;

  /* Unlock player mutex */
  g_mutex_unlock (&priv->mutex);

  /* Mute pipeline */
  g_object_set (priv->vol, "mute", mute, NULL);

  return mute;
}

static MeloPlayerState
melo_player_file_get_state (MeloPlayer *player)
{
  MeloPlayerFilePrivate *priv = (MELO_PLAYER_FILE (player))->priv;
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

  /* Lock player mutex */
  g_mutex_lock (&priv->mutex);

  /* Get duration */
  if (duration)
    *duration = priv->status->duration;

  /* Unlock player mutex */
  g_mutex_unlock (&priv->mutex);

  /* Get length */
  if (!gst_element_query_position (priv->pipeline, GST_FORMAT_TIME, &pos))
    pos = 0;

  return pos / 1000000;
}

static gdouble
melo_player_file_get_volume (MeloPlayer *player)
{
  MeloPlayerFilePrivate *priv = (MELO_PLAYER_FILE (player))->priv;
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
melo_player_file_get_status (MeloPlayer *player)
{
  MeloPlayerFilePrivate *priv = (MELO_PLAYER_FILE (player))->priv;
  MeloPlayerStatus *status;

  /* Lock player mutex */
  g_mutex_lock (&priv->mutex);

  /* Copy status */
  status = melo_player_status_ref (priv->status);

  /* Unlock player mutex */
  g_mutex_unlock (&priv->mutex);

  /* Update position */
  status->pos = melo_player_file_get_pos (player, NULL);

  /* Update playlist status */
  if (player->playlist) {
    status->has_prev = melo_playlist_has_prev (player->playlist);
    status->has_next = melo_playlist_has_next (player->playlist);
  }

  return status;
}

static gboolean
melo_player_file_get_cover (MeloPlayer *player, GBytes **data, gchar **type)
{
  MeloPlayerFilePrivate *priv = (MELO_PLAYER_FILE (player))->priv;
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

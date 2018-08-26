/*
 * melo_sink.c: Global audio sink for players
 *
 * Copyright (C) 2017 Alexandre Dilly <dillya@sparod.com>
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

#include "melo_sink.h"

/**
 * SECTION:melo_sink
 * @title: MeloSink
 * @short_description: Audio sink for #MeloPlayer
 *
 * #MeloSink provides a full set of functions to construct and manage an audio
 * sink for #MeloPlayer and GStreamer pipelines.
 *
 * The main purpose of this object is to provide a common set of audio sinks for
 * all players instantiated in Melo, to control global settings and individual
 * settings through a common interface.
 * By default, when a #MeloSink is created, it is attached to a #MeloPlayer and
 * it will update the output settings of the player transparently. Moreover,
 * when output settings are updated (like sample-rate), all sinks are updated
 * according to the new values.
 * A main volume control is also available in #MeloSink, to control the final
 * output volume.
 *
 * The function melo_sink_get_gst_sink() is intended to provide a sink
 * compatible #GstElement to be embedded in a full audio pipeline. The audio
 * mixing and control is then hided by the #MeloSink implementation.
 *
 * In addition to provide a common interface for all audio sinks, the #MeloSink
 * embed a mechanism to save and restore each individual volume / mute settings
 * of the #MeloSink instances. This settings are saved to a key file, 10 seconds
 * after value update in order to reduce file I/O.
 *
 * Before any #MeloPlayer instantiation, the melo_sink_main_init() must be
 * called once in order to initialize internal mixer and main audio sink. After
 * user, when all instances of #MeloPlayer have been released, the function
 * melo_sink_main_release() should be called.
 */

/* Main audio mixer pipeline */
G_LOCK_DEFINE_STATIC (melo_sink_mutex);
static gdouble melo_sink_volume = 1.0;
static gboolean melo_sink_mute;
static GstCaps *melo_sink_caps;
static GHashTable *melo_sink_hash;
static GList *melo_sink_list;
static GKeyFile *melo_sink_store;
static gchar *melo_sink_store_file;
static guint melo_sink_store_timer;

struct _MeloSinkPrivate {
  /* Associated player */
  MeloPlayer *player;
  gchar *name;
  gchar *id;

  /* Gstreamer elements */
  GstElement *sink;
  GstElement *convert;
  GstElement *resample;
  GstElement *filter;
  GstElement *audiosink;

  /* Volume control */
  GstElement *volume;
  gdouble vol;
  gboolean mute;
};

G_DEFINE_TYPE_WITH_PRIVATE (MeloSink, melo_sink, G_TYPE_OBJECT)

static void
melo_sink_finalize (GObject *gobject)
{
  MeloSink *sink = MELO_SINK (gobject);
  MeloSinkPrivate *priv = melo_sink_get_instance_private (sink);

  /* Lock main pipeline */
  G_LOCK (melo_sink_mutex);

  /* Release sink */
  gst_object_unref (priv->sink);

  /* Remove sink from main list */
  melo_sink_list = g_list_remove (melo_sink_list, sink);
  g_hash_table_remove (melo_sink_hash, priv->id);

  /* Free strings */
  g_free (priv->name);
  g_free (priv->id);

  /* Unlock main pipeline */
  G_UNLOCK (melo_sink_mutex);

  /* Chain up to the parent class */
  G_OBJECT_CLASS (melo_sink_parent_class)->finalize (gobject);
}

static void
melo_sink_class_init (MeloSinkClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  /* Add custom finalize() function */
  object_class->finalize = melo_sink_finalize;
}

static void
melo_sink_init (MeloSink *self)
{
  MeloSinkPrivate *priv = melo_sink_get_instance_private (self);

  self->priv = priv;
}

static inline gboolean
melo_sink_is_initialized (void)
{
  return melo_sink_hash ? TRUE : FALSE;
}

/**
 * melo_sink_new:
 * @player: a #MeloPlayer on which to attach
 * @id: a string containing the sink ID
 * @name: a string containing the display name of the sink
 *
 * Instantiate a new #MeloSink object attached to the provided @player. The
 * melo_player_set_volume(), melo_player_get_volume(), melo_player_set_mute()
 * and melo_player_get_mute() are automatically bound on the new #MeloSink
 * instance.
 *
 * If a #MeloSink has been already instantiated with the same ID, the volume and
 * mute settings are restored and applied.
 *
 * Returns: (transfer full): the new #MeloSink instance or %NULL if failed.
 */
MeloSink *
melo_sink_new (MeloPlayer *player, const gchar *id, const gchar *name)
{
  MeloSinkPrivate *priv;
  MeloSink *sink;
  GstPad *gpad, *pad;

  /* Lock main context access */
  G_LOCK (melo_sink_mutex);

  /* Main context is not initialized */
  if (!melo_sink_is_initialized ())
    goto failed;

  /* Check ID */
  if (!g_strcmp0 (id, "main") || g_hash_table_lookup (melo_sink_hash, id))
    goto failed;

  /* Create a new sink object */
  sink = g_object_new (MELO_TYPE_SINK, NULL);
  if (!sink)
    goto failed;

  /* Init private structure */
  priv = sink->priv;
  priv->vol = 1.0;
  priv->player = player;
  priv->id = g_strdup (id);
  priv->name = g_strdup (name);

  /* Create sink bin and conversion, volume and audiosink elements */
  priv->sink = gst_bin_new (id);
  priv->convert = gst_element_factory_make ("audioconvert", NULL);
  priv->resample = gst_element_factory_make ("audioresample", NULL);
  priv->volume = gst_element_factory_make ("volume", NULL);
  priv->filter = gst_element_factory_make ("capsfilter", NULL);
  priv->audiosink = gst_element_factory_make ("autoaudiosink", NULL);
  if (!priv->sink || !priv->convert || !priv->resample || !priv->volume ||
      !priv->filter || !priv->audiosink) {
    gst_object_unref (priv->sink);
    gst_object_unref (priv->convert);
    gst_object_unref (priv->resample);
    gst_object_unref (priv->volume);
    gst_object_unref (priv->filter);
    gst_object_unref (priv->audiosink);
    g_object_unref (sink);
    goto failed;
  }

  /* Setup caps for audio sink */
  g_object_set (priv->filter, "caps", melo_sink_caps, NULL);

  /* Restore volume and mute from storage file */
  if (melo_sink_store) {
    GError *err = NULL;
    gdouble volume;
    gboolean mute;

    /* Restore volume */
    volume = g_key_file_get_double (melo_sink_store, id, "volume", &err);
    if (!err)
      priv->vol = volume;
    g_clear_error (&err);

    /* Restore mute */
    mute = g_key_file_get_boolean (melo_sink_store, id, "mute", &err);
    if (!err)
      priv->mute = mute;
    g_clear_error (&err);

    /* Update player status */
    if (priv->player) {
      melo_player_set_status_volume (priv->player, priv->vol);
      melo_player_set_status_mute (priv->player, priv->mute);
    }
  }

  /* Setup volume */
  g_object_set (priv->volume, "volume", priv->vol * melo_sink_volume, "mute",
                priv->mute || melo_sink_mute, NULL);

  /* Add and connect convert -> resample -> volume -> audiosink to sink bin */
  gst_bin_add_many (GST_BIN (priv->sink), priv->convert, priv->resample,
                    priv->volume, priv->filter, priv->audiosink, NULL);
  gst_element_link_many (priv->convert, priv->resample, priv->volume,
                         priv->filter, priv->audiosink, NULL);

  /* Create sink pad on bin and connect to sink pad of audioconver */
  pad = gst_element_get_static_pad (priv->convert, "sink");
  gpad = gst_ghost_pad_new ("sink", pad);
  gst_element_add_pad (priv->sink, gpad);
  gst_object_unref (pad);

  /* Add sink to global sink list */
  melo_sink_list = g_list_prepend (melo_sink_list, sink);
  g_hash_table_insert (melo_sink_hash, priv->id, sink);

  /* Unlock main context access */
  G_UNLOCK (melo_sink_mutex);

  return sink;

failed:
  G_UNLOCK (melo_sink_mutex);
  return NULL;
}

/**
 * melo_sink_get_id:
 * @sink: the sink
 *
 * Get the #MeloSink ID.
 *
 * Returns: the ID of the #MeloSink.
 */
const gchar *
melo_sink_get_id (MeloSink *sink)
{
  return sink->priv->id;
}

/**
 * melo_sink_get_name:
 * @sink: the sink
 *
 * Get the #MeloSink name.
 *
 * Returns: the name of the #MeloSink.
 */
const gchar *
melo_sink_get_name (MeloSink *sink)
{
  return sink->priv->name;
}

/**
 * melo_sink_get_gst_sink:
 * @sink: the sink
 *
 * Get a #GstElement audio sink to use inside a Gstreamer audio pipeline.
 *
 * Returns: (transfer full): a new reference of a #GstElement representing the
 * audio sink. After use, the gst_object_unref() should be called.
 */
GstElement *
melo_sink_get_gst_sink (MeloSink *sink)
{
  g_return_val_if_fail (sink, NULL);
  return gst_object_ref (sink->priv->sink);
}

/**
 * melo_sink_get_sync:
 * @sink: the sink
 *
 * Get the sync flag status of the audio sink.
 *
 * Returns: %TRUE if the clock synchronization is enabled on the audio sink,
 * %FALSE otherwise.
 */
gboolean
melo_sink_get_sync (MeloSink *sink)
{
  gboolean enable;

  /* Get sync */
  g_object_get (G_OBJECT (sink->priv->audiosink), "sync", &enable, NULL);

  return enable;
}

/**
 * melo_sink_set_sync:
 * @sink: the sink
 * @enable: set %TRUE to enable clock synchronization
 *
 * Set the sync flag status of the audio sink. If @enable is set to %TRUE, the
 * sink will synchronize its clock with the sound card clock.
 */
void
melo_sink_set_sync (MeloSink *sink, gboolean enable)
{
  /* Set sync */
  g_object_set (G_OBJECT (sink->priv->audiosink), "sync", enable, NULL);
}

/**
 * melo_sink_get_volume:
 * @sink: the sink
 *
 * Get current volume of the sink.
 *
 * Returns: the current volume applied on the sink.
 */
gdouble
melo_sink_get_volume (MeloSink *sink)
{
  if (!sink)
    return melo_sink_get_main_volume ();
  return sink->priv->vol;
}

static gboolean
melo_sink_update_store_file_func (gpointer user_data)
{
  G_LOCK (melo_sink_mutex);
  g_key_file_save_to_file (melo_sink_store, melo_sink_store_file, NULL);
  melo_sink_store_timer = 0;
  G_UNLOCK (melo_sink_mutex);

  return FALSE;
}

static void
melo_sink_update_store_file (void)
{
  if (!melo_sink_store_timer)
    g_timeout_add_seconds (10, melo_sink_update_store_file_func, NULL);
}

/**
 * melo_sink_set_volume:
 * @sink: the sink
 * @volume: the volume to use
 *
 * Set a new volume value on the sink.
 *
 * Returns: the actual volume value applied on the sink.
 */
gdouble
melo_sink_set_volume (MeloSink *sink, gdouble volume)
{
  MeloSinkPrivate *priv;

  /* Set main volume */
  if (!sink)
    return melo_sink_set_main_volume (volume);

  /* Set volume */
  priv = sink->priv;
  priv->vol = volume;
  g_object_set (priv->volume, "volume", volume * melo_sink_volume, NULL);

  /* Update player status */
  if (priv->player)
    melo_player_set_status_volume (priv->player, volume);

  /* Save volume */
  G_LOCK (melo_sink_mutex);
  if (melo_sink_store) {
    g_key_file_set_double (melo_sink_store, priv->id, "volume", volume);
    melo_sink_update_store_file ();
  }
  G_UNLOCK (melo_sink_mutex);

  return volume;
}

/**
 * melo_sink_get_mute:
 * @sink: the sink
 *
 * Get the current mute flag of the sink.
 *
 * Returns: the current mute flag applied on the sink.
 */
gboolean
melo_sink_get_mute (MeloSink *sink)
{
  if (!sink)
    return melo_sink_get_main_mute ();
  return sink->priv->mute;
}

/**
 * melo_sink_set_mute:
 * @sink: the sink
 * @mute: set %TRUE to mute the sink
 *
 * Set the mute flag of the sink.
 *
 * Returns: the actual mute flag applied on the sink.
 */
gboolean
melo_sink_set_mute (MeloSink *sink, gboolean mute)
{
  MeloSinkPrivate *priv;

  /* Set main mute */
  if (!sink)
    return melo_sink_set_main_mute (mute);

  /* Set mute */
  priv = sink->priv;
  priv->mute = mute;
  g_object_set (sink->priv->volume, "mute", mute || melo_sink_mute, NULL);

  /* Update player status */
  if (priv->player)
    melo_player_set_status_mute (priv->player, mute);

  /* Save mute */
  G_LOCK (melo_sink_mutex);
  if (melo_sink_store) {
    g_key_file_set_boolean (melo_sink_store, priv->id, "mute", mute);
    melo_sink_update_store_file ();
  }
  G_UNLOCK (melo_sink_mutex);

  return mute;
}

/* Main pipeline control */
static GstCaps *
melo_sink_gen_caps (gint rate, gint channels)
{
  return gst_caps_new_simple ("audio/x-raw",
                              "format", G_TYPE_STRING, "S32LE",
                              "layout", G_TYPE_STRING, "interleaved",
                              "rate", G_TYPE_INT, rate,
                              "channels", G_TYPE_INT, channels, NULL);
}

/**
 * melo_sink_main_init:
 * @rate: the sample rate to use for the sound card
 * @channels: the channel count to use for the sound card
 *
 * Initialize and configure the sound card and internal mixer with the settings
 * provided. This function must be called once before any #MeloPlayer
 * instantiation.
 *
 * Returns: %TRUE if initialization has been done with success, %FALSE
 * otherwise.
 */
gboolean
melo_sink_main_init (gint rate, gint channels)
{
  gchar *path;

  /* Lock main context access */
  G_LOCK (melo_sink_mutex);

  /* Main context already initialized  */
  if (melo_sink_is_initialized ())
    goto failed;

  /* Generate audio sink caps */
  melo_sink_caps = melo_sink_gen_caps (rate, channels);

  /* Create hash table */
  melo_sink_hash = g_hash_table_new (g_str_hash, g_str_equal);

  /* Prepare filename to prepare sinks parameters storage */
  melo_sink_store_file = g_strdup_printf ("%s/melo/sink_store.conf",
                                          g_get_user_config_dir ());
  path = g_path_get_dirname (melo_sink_store_file);
  g_mkdir_with_parents (path, 0700);
  g_free (path);

  /* Create key file for volume/mute storage */
  melo_sink_store = g_key_file_new ();
  if (melo_sink_store) {
    GError *err = NULL;
    gdouble volume;
    gboolean mute;

    /* Restore settings from file */
    g_key_file_load_from_file (melo_sink_store, melo_sink_store_file,
                               G_KEY_FILE_NONE, NULL);

    /* Restore volume */
    volume = g_key_file_get_double (melo_sink_store, "main", "volume", &err);
    if (!err)
      melo_sink_volume = volume;
    g_clear_error (&err);

    /* Restore mute */
    mute = g_key_file_get_boolean (melo_sink_store, "main", "mute", &err);
    if (!err)
      melo_sink_mute = mute;
    g_clear_error (&err);
  }

  /* Unlock main context access */
  G_UNLOCK (melo_sink_mutex);

  return TRUE;

failed:
  G_UNLOCK (melo_sink_mutex);
  return FALSE;
}

/**
 * melo_sink_main_release:
 *
 * Release internal mixer and sound card. It must be called at end of program,
 * after release of all #MeloPlayer instances using a #MeloSink.
 *
 * Returns: %TRUE if release has been done with success, %FALSE otherwise.
 */
gboolean
melo_sink_main_release ()
{
  /* Lock main context access */
  G_LOCK (melo_sink_mutex);

  /* Main context not initialized  or some sinks are still used */
  if (!melo_sink_is_initialized () || melo_sink_list) {
    G_UNLOCK (melo_sink_mutex);
    return FALSE;
  }

  /* Free caps */
  gst_caps_unref (melo_sink_caps);
  melo_sink_caps = NULL;

  /* Free hash table */
  g_hash_table_unref (melo_sink_hash);
  melo_sink_hash = NULL;

  /* Save sink store */
  if (melo_sink_store) {
    /* Stop source */
    if (melo_sink_store_timer)
      g_source_remove (melo_sink_store_timer);

    /* Save to file */
    g_key_file_save_to_file (melo_sink_store, melo_sink_store_file, NULL);
    g_key_file_unref (melo_sink_store);
    melo_sink_store = NULL;
  }
  g_free (melo_sink_store_file);
  melo_sink_store_file = NULL;

  /* Unlock main context access */
  G_UNLOCK (melo_sink_mutex);

  return TRUE;
}

/**
 * melo_sink_set_main_config:
 * @rate: the sample rate to use for the sound card
 * @channels: the channel count to use for the sound card
 *
 * Set a new configuration on the sound card. An incremental update will be
 * done on all #MeloSink instances and then Gstreamer pipeline using the
 * #GstElement objects provided by melo_sink_get_gst_sink().
 *
 * Returns: %TRUE if new configuration has been applied with success, %FALSE
 * otherwise.
 */
gboolean
melo_sink_set_main_config (gint rate, gint channels)
{
  gboolean ret = FALSE;
  GList *list;

  /* Lock main context access */
  G_LOCK (melo_sink_mutex);

  /* Get caps */
  if (melo_sink_caps) {
    gst_caps_unref (melo_sink_caps);
    melo_sink_caps = melo_sink_gen_caps (rate, channels);
    ret = TRUE;

    /* Update sink caps */
    for (list = melo_sink_list; list != NULL; list = list->next) {
      MeloSink *sink = (MeloSink *) list->data;
      g_object_set (sink->priv->filter, "caps", melo_sink_caps, NULL);
    }
  }

  /* Unlock main context access */
  G_UNLOCK (melo_sink_mutex);

  return ret;
}

/**
 * melo_sink_get_main_config:
 * @rate: a pointer to a #gint to store the current sample rate
 * @channels: a pointer to a #gint to store the current channel count
 *
 * Get current configuration applied on the sound card.
 *
 * Returns: %TRUE if configuration has been retrieved with success, %FALSE
 * otherwise.
 */
gboolean
melo_sink_get_main_config (gint *rate, gint *channels)
{
  gboolean ret = FALSE;

  /* Lock main context access */
  G_LOCK (melo_sink_mutex);

  /* Get caps */
  if (melo_sink_caps) {
    GstStructure *str = gst_caps_get_structure (melo_sink_caps, 0);

    /* Get configuration */
    ret = gst_structure_get_int (str, "rate", rate);
    ret |= gst_structure_get_int (str, "channels", channels);
  }

  /* Unlock main context access */
  G_UNLOCK (melo_sink_mutex);

  return ret;
}

/**
 * melo_sink_set_main_volume:
 *
 * Get the current global volume value.
 *
 * Returns: the current global volume value.
 */
gdouble
melo_sink_get_main_volume ()
{
  return melo_sink_volume;
}

/**
 * melo_sink_set_main_volume:
 * @volume: the global volume value to apply
 *
 * Set the global volume value on all sinks.
 *
 * Returns: the actual global volume value applied.
 */
gdouble
melo_sink_set_main_volume (gdouble volume)
{
  GList *list;

  /* Lock main context access */
  G_LOCK (melo_sink_mutex);

  /* Set new volume */
  melo_sink_volume = volume;

  /* Update all sinks */
  for (list = melo_sink_list; list != NULL; list = list->next) {
    MeloSink *sink = (MeloSink *) list->data;
    MeloSinkPrivate *priv = sink->priv;
    g_object_set (priv->volume, "volume", priv->vol * volume, NULL);
  }

  /* Save volume */
  if (melo_sink_store) {
    g_key_file_set_double (melo_sink_store, "main", "volume", volume);
    melo_sink_update_store_file ();
  }

  /* Unlock main context access */
  G_UNLOCK (melo_sink_mutex);

  return volume;
}

/**
 * melo_sink_set_main_mute:
 *
 * Get the current global mute flag.
 *
 * Returns: the current global mute flag.
 */
gboolean
melo_sink_get_main_mute ()
{
  return melo_sink_mute;
}

/**
 * melo_sink_set_main_mute:
 * @mute: set %TRUE to mute all sinks
 *
 * Set the global mute flag.
 *
 * Returns: the actual global mute flag applied.
 */
gboolean
melo_sink_set_main_mute (gboolean mute)
{
  GList *list;

  /* Lock main context access */
  G_LOCK (melo_sink_mutex);

  /* Set new volume */
  melo_sink_mute = mute;

  /* Update all sinks */
  for (list = melo_sink_list; list != NULL; list = list->next) {
    MeloSink *sink = (MeloSink *) list->data;
    MeloSinkPrivate *priv = sink->priv;
    g_object_set (priv->volume, "mute", priv->mute || mute, NULL);
  }

  /* Save mute */
  if (melo_sink_store) {
    g_key_file_set_double (melo_sink_store, "main", "mute", mute);
    melo_sink_update_store_file ();
  }

  /* Unlock main context access */
  G_UNLOCK (melo_sink_mutex);

  return mute;
}

/**
 * melo_sink_get_sink_by_id:
 * @id: the #MeloSink ID to retrieve
 *
 * Get an instance of the #MeloSink with its ID.
 *
 * Returns: (transfer full): the #MeloSink instance or %NULL if not found. Use
 * g_object_unref() after usage.
 */
MeloSink *
melo_sink_get_sink_by_id (const gchar *id)
{
  MeloSink *sink;

  /* Lock list access */
  G_LOCK (melo_sink_mutex);

  /* Find sink with its ID */
  sink = melo_sink_hash ? g_hash_table_lookup (melo_sink_hash, id) : NULL;
  if (sink)
    g_object_ref (sink);

  /* Unlock list access */
  G_UNLOCK (melo_sink_mutex);

  return sink;
}

/**
 * melo_sink_get_sink_list:
 *
 * Get a #GList of all #MeloSink registered.
 *
 * Returns: (transfer full): a #GList of all #MeloSink registered. You must
 * free list and its data when you are done with it. You can use
 * g_list_free_full() with g_object_unref() to do this.
 */
GList *
melo_sink_get_sink_list (void)
{
  GList *list;

  /* Lock sink list access */
  G_LOCK (melo_sink_mutex);

  /* Copy sink list */
  list = g_list_copy_deep (melo_sink_list, (GCopyFunc) g_object_ref, NULL);

  /* Unlock sink list access */
  G_UNLOCK (melo_sink_mutex);

  return list;
}

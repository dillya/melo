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

/* Main audio mixer pipeline */
G_LOCK_DEFINE_STATIC (melo_sink_mutex);
static gdouble melo_sink_volume = 1.0;
static gboolean melo_sink_mute;
static GstCaps *melo_sink_caps;
static GHashTable *melo_sink_hash;
static GList *melo_sink_list;

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

static gboolean bus_call (GstBus *bus, GstMessage *msg, gpointer data);

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

static gboolean
bus_call (GstBus *bus, GstMessage *msg, gpointer data)
{
  return TRUE;
}

static inline gboolean
melo_sink_is_initialized (void)
{
  return melo_sink_hash ? TRUE : FALSE;
}

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

  /* Setup volume */
  priv->vol = 1.0;
  g_object_set (priv->volume, "volume", priv->vol * melo_sink_volume, "mute",
                melo_sink_mute, NULL);

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

const gchar *
melo_sink_get_id (MeloSink *sink)
{
  return sink->priv->id;
}

const gchar *
melo_sink_get_name (MeloSink *sink)
{
  return sink->priv->name;
}

GstElement *
melo_sink_get_gst_sink (MeloSink *sink)
{
  g_return_val_if_fail (sink, NULL);
  return gst_object_ref (sink->priv->sink);
}

gboolean
melo_sink_get_sync (MeloSink *sink)
{
  gboolean enable;

  /* Get sync */
  g_object_get (G_OBJECT (sink->priv->audiosink), "sync", &enable, NULL);

  return enable;
}

void
melo_sink_set_sync (MeloSink *sink, gboolean enable)
{
  /* Set sync */
  g_object_set (G_OBJECT (sink->priv->audiosink), "sync", enable, NULL);
}

gdouble
melo_sink_get_volume (MeloSink *sink)
{
  if (!sink)
    return melo_sink_get_main_volume ();
  return sink->priv->vol;
}

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

  return volume;
}

gboolean
melo_sink_get_mute (MeloSink *sink)
{
  if (!sink)
    return melo_sink_get_main_mute ();
  return sink->priv->mute;
}

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

gboolean
melo_sink_main_init (gint rate, gint channels)
{

  /* Lock main context access */
  G_LOCK (melo_sink_mutex);

  /* Main context already initialized  */
  if (melo_sink_is_initialized ())
    goto failed;

  /* Generate audio sink caps */
  melo_sink_caps = melo_sink_gen_caps (rate, channels);

  /* Create hash table */
  melo_sink_hash = g_hash_table_new (g_str_hash, g_str_equal);

  /* Unlock main context access */
  G_UNLOCK (melo_sink_mutex);

  return TRUE;

failed:
  G_UNLOCK (melo_sink_mutex);
  return FALSE;
}

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

  /* Unlock main context access */
  G_UNLOCK (melo_sink_mutex);

  return TRUE;
}

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

gdouble
melo_sink_get_main_volume ()
{
  return melo_sink_volume;
}

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

  /* Unlock main context access */
  G_UNLOCK (melo_sink_mutex);

  return volume;
}

gboolean
melo_sink_get_main_mute ()
{
  return melo_sink_mute;
}

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

  /* Unlock main context access */
  G_UNLOCK (melo_sink_mutex);

  return mute;
}

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

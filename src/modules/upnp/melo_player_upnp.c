/*
 * melo_player_upnp.c: UPnP / DLNA renderer using Rygel
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

#include <rygel-renderer-gst.h>

#include "melo_sink.h"
#include "melo_player_upnp.h"

static MeloPlayerState melo_player_upnp_set_state (MeloPlayer *player,
                                                   MeloPlayerState state);
static gint melo_player_upnp_set_pos (MeloPlayer *player, gint pos);
static gdouble melo_player_upnp_set_volume (MeloPlayer *player, gdouble volume);

static gint melo_player_upnp_get_pos (MeloPlayer *player);

static void on_context_available (GUPnPContextManager *manager,
                                  GUPnPContext *context, gpointer user_data);
static void on_context_unavailable (GUPnPContextManager *manager,
                                    GUPnPContext *context, gpointer user_data);

struct _MeloPlayerUpnpPrivate {
  GMutex player_mutex;
  SoupSession *session;

  /* UPnP / DLNA Renderer */
  GUPnPContextManager *manager;
  RygelPlaybinRenderer *renderer;
  RygelMediaPlayer *player;
  MeloSink *sink;
  GList *ifaces;
};

G_DEFINE_TYPE_WITH_PRIVATE (MeloPlayerUpnp, melo_player_upnp, MELO_TYPE_PLAYER)

static void
melo_player_upnp_finalize (GObject *gobject)
{
  MeloPlayerUpnp *pup = MELO_PLAYER_UPNP (gobject);
  MeloPlayerUpnpPrivate *priv = melo_player_upnp_get_instance_private (pup);

  /* Free UPnP context manager */
  g_object_unref (priv->manager);

  /* Free interfaces list */
  if (priv->ifaces)
    g_list_free (priv->ifaces);

  /* Free Soup session */
  g_object_unref (priv->session);

  /* Clear mutex */
  g_mutex_clear (&priv->player_mutex);

  /* Free UPnP renderer */
  if (priv->renderer) {
    g_object_unref (priv->player);
    g_object_unref (priv->renderer);
  }

  /* Chain up to the parent class */
  G_OBJECT_CLASS (melo_player_upnp_parent_class)->finalize (gobject);
}

static void
melo_player_upnp_class_init (MeloPlayerUpnpClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MeloPlayerClass *pclass = MELO_PLAYER_CLASS (klass);

  /* Control */
  pclass->set_state = melo_player_upnp_set_state;
  pclass->set_pos = melo_player_upnp_set_pos;
  pclass->set_volume = melo_player_upnp_set_volume;

  /* Status */
  pclass->get_pos = melo_player_upnp_get_pos;

  /* Add custom finalize() function */
  object_class->finalize = melo_player_upnp_finalize;
}

static void
melo_player_upnp_init (MeloPlayerUpnp *self)
{
  MeloPlayerUpnpPrivate *priv = melo_player_upnp_get_instance_private (self);

  self->priv = priv;

  /* Init player and status mutex */
  g_mutex_init (&priv->player_mutex);

  /* Create a new UPnP context manager */
  priv->manager = gupnp_context_manager_create (0);
  g_signal_connect (priv->manager, "context-available",
                    (GCallback) on_context_available, priv);
  g_signal_connect (priv->manager, "context-unavailable",
                    (GCallback) on_context_unavailable, priv);
  /* Create a new Soup session */
  priv->session = soup_session_new_with_options (
                                SOUP_SESSION_USER_AGENT, "Melo",
                                NULL);
}

static MeloPlayerState
melo_player_upnp_set_state (MeloPlayer *player, MeloPlayerState state)
{
  MeloPlayerUpnpPrivate *priv = (MELO_PLAYER_UPNP (player))->priv;
  const gchar *pstate;

  /* Lock player mutex */
  g_mutex_lock (&priv->player_mutex);

  /* Set state */
  if (priv->player) {
    switch (state) {
    case MELO_PLAYER_STATE_NONE:
      pstate = "EOS";
      break;
    case MELO_PLAYER_STATE_PLAYING:
      pstate = "PLAYING";
      break;
    case MELO_PLAYER_STATE_PAUSED:
      pstate = "PAUSED_PLAYBACK";
      break;
    case MELO_PLAYER_STATE_STOPPED:
    default:
      pstate = "STOPPED";
    }

    /* Set playback state */
    rygel_media_player_set_playback_state (priv->player, pstate);
    melo_player_set_status_state (player, state);
  }

  /* Unlock player mutex */
  g_mutex_unlock (&priv->player_mutex);

  return state;
}

static gint
melo_player_upnp_set_pos (MeloPlayer *player, gint pos)
{
  MeloPlayerUpnpPrivate *priv = (MELO_PLAYER_UPNP (player))->priv;

  /* Lock player mutex */
  g_mutex_lock (&priv->player_mutex);

  /* Get position */
  if (priv->player)
    rygel_media_player_seek (priv->player, pos * 1000);

  /* Unlock player mutex */
  g_mutex_unlock (&priv->player_mutex);

 return pos;
}

static gdouble
melo_player_upnp_set_volume (MeloPlayer *player, gdouble volume)
{
  MeloPlayerUpnpPrivate *priv = (MELO_PLAYER_UPNP (player))->priv;

  /* Lock player mutex */
  g_mutex_lock (&priv->player_mutex);

  /* Set volume */
  if (priv->player)
    rygel_media_player_set_volume (priv->player, volume);

  /* Unlock player mutex */
  g_mutex_unlock (&priv->player_mutex);

  return volume;
}

static gint
melo_player_upnp_get_pos (MeloPlayer *player)
{
  MeloPlayerUpnpPrivate *priv = (MELO_PLAYER_UPNP (player))->priv;
  guint32 pos = 0;

  /* Lock player mutex */
  g_mutex_lock (&priv->player_mutex);

  /* Get position */
  if (priv->player)
    pos = rygel_media_player_get_position (priv->player);

  /* Unlock player mutex */
  g_mutex_unlock (&priv->player_mutex);

  return pos / 1000;
}

static void
on_context_available (GUPnPContextManager *manager, GUPnPContext *context,
                      gpointer user_data)
{
  MeloPlayerUpnpPrivate *priv = (MeloPlayerUpnpPrivate *) user_data;
  GSSDPClient *client = GSSDP_CLIENT (context);
  const gchar *iface;

  /* Lock renderer access */
  g_mutex_lock (&priv->player_mutex);

  /* Get interface and add to list */
  iface = gssdp_client_get_interface (client);
  if (!g_list_find_custom (priv->ifaces, iface, (GCompareFunc) g_strcmp0)) {
    priv->ifaces = g_list_prepend (priv->ifaces, (gpointer *) iface);

    /* Add interface to renderer */
    if (priv->renderer)
      rygel_media_device_add_interface (RYGEL_MEDIA_DEVICE (priv->renderer),
                                        iface);
  }

  /* Unlock renderer access */
  g_mutex_unlock (&priv->player_mutex);
}

static void
on_context_unavailable (GUPnPContextManager *manager, GUPnPContext *context,
                        gpointer user_data)
{
  MeloPlayerUpnpPrivate *priv = (MeloPlayerUpnpPrivate *) user_data;
  GSSDPClient *client = GSSDP_CLIENT (context);
  const gchar *iface;
  GList *l;

  /* Lock renderer access */
  g_mutex_lock (&priv->player_mutex);

  /* Get interface and remove from list */
  iface = gssdp_client_get_interface (client);
  for (l = priv->ifaces; l != NULL; l = l->next) {
    if (!g_strcmp0 ((const gchar *) l->data, iface)) {
      /* Remove interface to renderer */
      if (priv->renderer)
        rygel_media_device_remove_interface (
                                            RYGEL_MEDIA_DEVICE (priv->renderer),
                                            iface);
      /* Free interface */
      priv->ifaces = g_list_delete_link (priv->ifaces, l);
      break;
    }
  }

  /* Unlock renderer access */
  g_mutex_unlock (&priv->player_mutex);
}

static void
on_request_done (SoupSession *session, SoupMessage *msg, gpointer user_data)
{
  MeloPlayerUpnp *up = MELO_PLAYER_UPNP (user_data);
  MeloPlayerUpnpPrivate *priv = up->priv;
  MeloPlayer *player = MELO_PLAYER (up);
  GBytes *img = NULL;
  const gchar *type;
  MeloTags *tags;

  /* Get content type */
  type = soup_message_headers_get_one (msg->response_headers, "Content-Type");

  /* Get image */
  g_object_get (msg, "response-body-data", &img, NULL);

  /* Get tags from status */
  tags = melo_player_get_tags (player);

  /* Set cover in tags */
  if (tags) {
    melo_tags_take_cover (tags, img, type);
    melo_tags_set_cover_url (tags, G_OBJECT (up), NULL, NULL);
    melo_player_take_status_tags (player, tags);
  }
}

static void
on_object_available (GUPnPDIDLLiteParser *parser, GUPnPDIDLLiteObject *object,
                     gpointer user_data)
{
  MeloPlayerUpnp *up = MELO_PLAYER_UPNP (user_data);
  MeloPlayerUpnpPrivate *priv = up->priv;
  MeloPlayer *player = MELO_PLAYER (up);
  const gchar *img;
  SoupMessage *msg;
  MeloTags *tags;

  /* Generate a new tags */
  tags = melo_tags_new ();
  if (!tags)
    return;

  /* Fill with basic tags */
  tags->title = g_strdup (gupnp_didl_lite_object_get_title (object));
  tags->artist = g_strdup (gupnp_didl_lite_object_get_artist (object));
  tags->album = g_strdup (gupnp_didl_lite_object_get_album (object));
  tags->genre = g_strdup (gupnp_didl_lite_object_get_genre (object));

  /* Update tags in status */
  melo_player_take_status_tags (player, tags);

  /* Get image */
  img = gupnp_didl_lite_object_get_album_art (object);
  if (img) {
    msg = soup_message_new ("GET", img);
    soup_session_queue_message (priv->session, msg, on_request_done, up);
  }
}

static void
on_notify (GObject *object, GParamSpec *pspec, gpointer user_data)
{
  RygelMediaPlayer *player = RYGEL_MEDIA_PLAYER (object);
  MeloPlayerUpnp *up = MELO_PLAYER_UPNP (user_data);
  MeloPlayerUpnpPrivate *priv = up->priv;
  MeloPlayer *play = MELO_PLAYER (up);

  /* Parse property changes */
  if (!g_strcmp0 (pspec->name, "playback-state")) {
    MeloPlayerState state;
    const gchar *pstate;
    guint32 pos = 0;

    /* Get new state */
    pstate = rygel_media_player_get_playback_state (player);

    /* Set new state */
    if (!g_strcmp0 (pstate, "PLAYING"))
      state = MELO_PLAYER_STATE_PLAYING;
    else if (!g_strcmp0 (pstate, "PAUSED_PLAYBACK"))
      state = MELO_PLAYER_STATE_PAUSED;
    else if (!g_strcmp0 (pstate, "STOPPED"))
      state = MELO_PLAYER_STATE_STOPPED;
    else
      state = MELO_PLAYER_STATE_NONE;
    melo_player_set_status_state (play, state);

    /* Get position */
    pos = rygel_media_player_get_position (player);
    melo_player_set_status_pos (play, pos / 1000);
  } else if (!g_strcmp0 (pspec->name, "duration")) {
    /* Get duration */
    melo_player_set_status_duration (play,
                               rygel_media_player_get_duration (player) / 1000);
  } else if (!g_strcmp0 (pspec->name, "volume")) {
    gdouble volume;

    /* Get volume */
    volume = rygel_media_player_get_volume (priv->player);
    melo_player_set_status_volume (play, volume);
  } else if (!g_strcmp0 (pspec->name, "metadata")) {
    GUPnPDIDLLiteParser *parser;
    gchar *meta;

    /* Get metatdata DIDLLite information */
    meta = rygel_media_player_get_metadata (player);

    /* Parse meta with GUPnP parser */
    parser = gupnp_didl_lite_parser_new ();
    g_signal_connect (parser, "object-available",
                      (GCallback)on_object_available, up);
    gupnp_didl_lite_parser_parse_didl (parser, meta, NULL);
    g_object_unref (parser);
  }
}

gboolean
melo_player_upnp_start (MeloPlayerUpnp *up, const gchar *name)
{
  MeloPlayerUpnpPrivate *priv = up->priv;
  MeloPlayer *player = MELO_PLAYER (up);
  GstElement *playbin, *sink;
  RygelPlugin *plugin;
  gboolean ret = FALSE;
  gchar *sink_name;
  GList *l;

  /* Lock renderer access */
  g_mutex_lock (&priv->player_mutex);

  if (priv->renderer)
    goto end;

  /* Set default name */
  if (!name)
    name = "Melo";

  /* Create a new UPnP renderer */
  priv->renderer = rygel_playbin_renderer_new (name);
  if (!priv->renderer)
    goto end;

  /* Get UPnP player */
  plugin = rygel_media_device_get_plugin (RYGEL_MEDIA_DEVICE (priv->renderer));
  priv->player = rygel_media_renderer_plugin_get_player (
                                          RYGEL_MEDIA_RENDERER_PLUGIN (plugin));

  /* Register property notifications on player */
  g_signal_connect (priv->player, "notify", (GCallback) on_notify, up);

  /* Get gstreamer playbin */
  playbin = rygel_playbin_renderer_get_playbin (priv->renderer);
  sink_name = g_strjoin ("_", melo_player_get_id (player), "sink", NULL);

  /* Use Melo audio output */
  priv->sink = melo_sink_new (MELO_PLAYER (up), sink_name,
                              melo_player_get_name (player));
  sink = melo_sink_get_gst_sink (priv->sink);
  g_object_set (G_OBJECT (playbin), "audio-sink", sink, NULL);

  /* Disable video output */
  sink = gst_element_factory_make ("fakesink", NULL);
  g_object_set (G_OBJECT (playbin), "video-sink", sink, NULL);

  /* Release gstreamer playbin */
  g_object_unref (playbin);
  g_free (sink_name);

  /* Setup interfaces */
  for (l = priv->ifaces; l != NULL; l = l->next) {
    rygel_media_device_add_interface (RYGEL_MEDIA_DEVICE (priv->renderer),
                                      (const gchar *) l->data);
  }

  ret = TRUE;

end:
  /* Unlock renderer access */
  g_mutex_unlock (&priv->player_mutex);

  return ret;
}

void
melo_player_upnp_stop (MeloPlayerUpnp *up)
{
  MeloPlayerUpnpPrivate *priv = up->priv;

  /* Lock renderer access */
  g_mutex_lock (&priv->player_mutex);

  if (priv->renderer) {
    /* Stop and free UPnP renderer */
    g_object_unref (priv->player);
    g_object_unref (priv->renderer);
    priv->renderer = NULL;
    priv->player = NULL;
  }

  /* Free Melo audio sink */
  g_object_unref (priv->sink);

  /* Unlock renderer access */
  g_mutex_unlock (&priv->player_mutex);

  return;
}

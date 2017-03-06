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

#include "melo_event.h"
#include "melo_player_upnp.h"

static MeloPlayerState melo_player_upnp_set_state (MeloPlayer *player,
                                                   MeloPlayerState state);
static gint melo_player_upnp_set_pos (MeloPlayer *player, gint pos);
static gdouble melo_player_upnp_set_volume (MeloPlayer *player, gdouble volume);

static MeloPlayerState melo_player_upnp_get_state (MeloPlayer *player);
static gchar *melo_player_upnp_get_name (MeloPlayer *player);
static gint melo_player_upnp_get_pos (MeloPlayer *player, gint *duration);
static gdouble melo_player_upnp_get_volume (MeloPlayer *player);
static MeloPlayerStatus *melo_player_upnp_get_status (MeloPlayer *player);
static gboolean melo_player_upnp_get_cover (MeloPlayer *player, GBytes **data,
                                            gchar **type);

static void on_context_available (GUPnPContextManager *manager,
                                  GUPnPContext *context, gpointer user_data);
static void on_context_unavailable (GUPnPContextManager *manager,
                                    GUPnPContext *context, gpointer user_data);

struct _MeloPlayerUpnpPrivate {
  GMutex status_mutex;
  GMutex player_mutex;
  MeloPlayerStatus *status;
  SoupSession *session;

  /* UPnP / DLNA Renderer */
  GUPnPContextManager *manager;
  RygelPlaybinRenderer *renderer;
  RygelMediaPlayer *player;
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

  /* Free status */
  melo_player_status_unref (priv->status);

  /* Free Soup session */
  g_object_unref (priv->session);

  /* Clear mutex */
  g_mutex_clear (&priv->player_mutex);
  g_mutex_clear (&priv->status_mutex);

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
  pclass->get_state = melo_player_upnp_get_state;
  pclass->get_name = melo_player_upnp_get_name;
  pclass->get_pos = melo_player_upnp_get_pos;
  pclass->get_volume = melo_player_upnp_get_volume;
  pclass->get_status = melo_player_upnp_get_status;
  pclass->get_cover = melo_player_upnp_get_cover;

  /* Add custom finalize() function */
  object_class->finalize = melo_player_upnp_finalize;
}

static void
melo_player_upnp_init (MeloPlayerUpnp *self)
{
  MeloPlayerUpnpPrivate *priv = melo_player_upnp_get_instance_private (self);

  self->priv = priv;

  /* Init player and status mutex */
  g_mutex_init (&priv->status_mutex);
  g_mutex_init (&priv->player_mutex);

  /* Create new status handler */
  priv->status = melo_player_status_new (NULL, MELO_PLAYER_STATE_NONE, NULL);

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
    melo_player_status_set_state (priv->status, state);
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

static MeloPlayerState
melo_player_upnp_get_state (MeloPlayer *player)
{
  MeloPlayerUpnpPrivate *priv = (MELO_PLAYER_UPNP (player))->priv;
  MeloPlayerState state;

  /* Lock status mutex */
  g_mutex_lock (&priv->status_mutex);

  /* Get state */
  state = priv->status->state;

  /* Unlock status mutex */
  g_mutex_unlock (&priv->status_mutex);

  return state;
}

static gchar *
melo_player_upnp_get_name (MeloPlayer *player)
{
  MeloPlayerUpnpPrivate *priv = (MELO_PLAYER_UPNP (player))->priv;
  gchar *name = NULL;

  /* Lock status mutex */
  g_mutex_lock (&priv->status_mutex);

  /* Copy name */
  name = melo_player_status_get_name (priv->status);

  /* Unlock status mutex */
  g_mutex_unlock (&priv->status_mutex);

  return name;
}

static gint
melo_player_upnp_get_pos (MeloPlayer *player, gint *duration)
{
  MeloPlayerUpnpPrivate *priv = (MELO_PLAYER_UPNP (player))->priv;
  guint32 pos = 0;

  /* Lock status mutex */
  g_mutex_lock (&priv->status_mutex);

  /* Get duration */
  if (duration)
    *duration = priv->status->duration;

  /* Unlock status mutex */
  g_mutex_unlock (&priv->status_mutex);

  /* Lock player mutex */
  g_mutex_lock (&priv->player_mutex);

  /* Get position */
  if (priv->player)
    pos = rygel_media_player_get_position (priv->player);

  /* Unlock player mutex */
  g_mutex_unlock (&priv->player_mutex);

  return pos / 1000;
}

static gdouble
melo_player_upnp_get_volume (MeloPlayer *player)
{
  MeloPlayerUpnpPrivate *priv = (MELO_PLAYER_UPNP (player))->priv;
  gdouble volume;

  /* Lock player mutex */
  g_mutex_lock (&priv->player_mutex);

  /* Get volume */
  if (priv->player)
    volume = rygel_media_player_get_volume (priv->player);

  /* Unlock player mutex */
  g_mutex_unlock (&priv->player_mutex);

  return volume;
}

static MeloPlayerStatus *
melo_player_upnp_get_status (MeloPlayer *player)
{
  MeloPlayerUpnpPrivate *priv = (MELO_PLAYER_UPNP (player))->priv;
  MeloPlayerStatus *status;

  /* Lock status mutex */
  g_mutex_lock (&priv->status_mutex);

  /* Copy status */
  status = melo_player_status_ref (priv->status);
  status->volume = melo_player_upnp_get_volume (player);

  /* Unlock status mutex */
  g_mutex_unlock (&priv->status_mutex);

  /* Update position */
  status->pos = melo_player_upnp_get_pos (player, NULL);

  return status;
}

static gboolean
melo_player_upnp_get_cover (MeloPlayer *player, GBytes **data, gchar **type)
{
  MeloPlayerUpnpPrivate *priv = (MELO_PLAYER_UPNP (player))->priv;
  MeloTags *tags;

  /* Lock status mutex */
  g_mutex_lock (&priv->status_mutex);

  /* Copy status */
  tags = melo_player_status_get_tags (priv->status);
  if (tags) {
    *data = melo_tags_get_cover (tags, type);
    melo_tags_unref (tags);
  }

  /* Unlock status mutex */
  g_mutex_unlock (&priv->status_mutex);

  return TRUE;
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
  GBytes *img = NULL;
  const gchar *type;
  MeloTags *tags;

  /* Get content type */
  type = soup_message_headers_get_one (msg->response_headers, "Content-Type");

  /* Get image */
  g_object_get (msg, "response-body-data", &img, NULL);

  /* Lock status */
  g_mutex_lock (&priv->status_mutex);

  /* Get tags from status */
  tags = melo_player_status_get_tags (priv->status);

  /* Set cover in tags */
  if (tags) {
    melo_tags_take_cover (tags, img, type);
    melo_tags_set_cover_url (tags, G_OBJECT (up), NULL, NULL);
    melo_player_status_take_tags (priv->status, tags, TRUE);
  }

  /* Unlock status */
  g_mutex_unlock (&priv->status_mutex);
}

static void
on_object_available (GUPnPDIDLLiteParser *parser, GUPnPDIDLLiteObject *object,
                     gpointer user_data)
{
  MeloPlayerUpnp *up = MELO_PLAYER_UPNP (user_data);
  MeloPlayerUpnpPrivate *priv = up->priv;
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
  melo_player_status_take_tags (priv->status, tags, TRUE);

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

  /* Lock player mutex */
  g_mutex_lock (&priv->status_mutex);

  /* Parse property changes */
  if (!g_strcmp0 (pspec->name, "playback-state")) {
    MeloPlayerState state;
    const gchar *pstate;

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
    melo_player_status_set_state (priv->status, state);
  } else if (!g_strcmp0 (pspec->name, "duration")) {
    /* Get duration */
    melo_player_status_set_duration (priv->status,
                               rygel_media_player_get_duration (player) / 1000);
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

  /* Unlock player mutex */
  g_mutex_unlock (&priv->status_mutex);
}

gboolean
melo_player_upnp_start (MeloPlayerUpnp *up, const gchar *name)
{
  MeloPlayerUpnpPrivate *priv = up->priv;
  GstElement *playbin, *sink;
  RygelPlugin *plugin;
  gboolean ret = FALSE;
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

  /* Disable video output */
  playbin = rygel_playbin_renderer_get_playbin (priv->renderer);
  sink = gst_element_factory_make ("fakesink", NULL);
  g_object_set (G_OBJECT (playbin), "video-sink", sink, NULL);
  g_object_unref (playbin);

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

  /* Unlock renderer access */
  g_mutex_unlock (&priv->player_mutex);

  return;
}

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

#include <config.h>

#include <rygel-renderer-gst.h>

#include <melo/melo_playlist.h>

#define MELO_LOG_TAG "upnp_player"
#include <melo/melo_log.h>

#include "melo_upnp_player.h"

#ifndef MELO_UPNP_PLAYER_ICON_JPEG
#define MELO_UPNP_PLAYER_ICON_JPEG "/usr/share/melo/icons/128x128/melo.jpg"
#endif
#ifndef MELO_UPNP_PLAYER_ICON_PNG
#define MELO_UPNP_PLAYER_ICON_PNG "/usr/share/melo/icons/128x128/melo.png"
#endif

#define MELO_UPNP_PLAYER_ICON_WIDTH 128
#define MELO_UPNP_PLAYER_ICON_HEIGHT 128
#define MELO_UPNP_PLAYER_ICON_DEPTH 24

struct _MeloUpnpPlayer {
  GObject parent_instance;

  /* UPnP / DLNA Renderer */
  GUPnPContextManager *manager;
  RygelPlaybinRenderer *renderer;
  RygelMediaPlayer *player;
  GList *ifaces;

  /* Internal player status */
  bool started;
  bool eos;
  MeloTags *tags;

  /* Settings */
  MeloSettingsEntry *enable;
  MeloSettingsEntry *name;
};

MELO_DEFINE_PLAYER (MeloUpnpPlayer, melo_upnp_player)

static void melo_upnp_player_settings (
    MeloPlayer *player, MeloSettings *settings);
static bool melo_upnp_player_set_state (
    MeloPlayer *player, MeloPlayerState state);
static bool melo_upnp_player_set_position (
    MeloPlayer *player, unsigned int position);
static unsigned int melo_upnp_player_get_position (MeloPlayer *player);
static char *melo_upnp_player_get_asset (MeloPlayer *player, const char *id);

static void context_available_cb (
    GUPnPContextManager *manager, GUPnPContext *context, gpointer user_data);
static void context_unavailable_cb (
    GUPnPContextManager *manager, GUPnPContext *context, gpointer user_data);

static bool settings_update_cb (MeloSettings *settings,
    MeloSettingsGroup *group, char **error, void *user_data);

static bool melo_upnp_player_start (MeloUpnpPlayer *player, const char *name);
static void melo_upnp_player_stop (MeloUpnpPlayer *player);

static void melo_upnp_player_constructed (GObject *object);

static void
melo_upnp_player_finalize (GObject *object)
{
  MeloUpnpPlayer *player = MELO_UPNP_PLAYER (object);

  /* Stop renderer */
  melo_upnp_player_stop (player);

  /* Free tags */
  melo_tags_unref (player->tags);

  /* Free UPnP context manager */
  g_object_unref (player->manager);

  /* Free interfaces list */
  if (player->ifaces)
    g_list_free (player->ifaces);

  /* Chain finalize */
  G_OBJECT_CLASS (melo_upnp_player_parent_class)->finalize (object);
}

static void
melo_upnp_player_class_init (MeloUpnpPlayerClass *klass)
{
  MeloPlayerClass *parent_class = MELO_PLAYER_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  /* Setup callbacks */
  parent_class->settings = melo_upnp_player_settings;
  parent_class->set_state = melo_upnp_player_set_state;
  parent_class->set_position = melo_upnp_player_set_position;
  parent_class->get_position = melo_upnp_player_get_position;
  parent_class->get_asset = melo_upnp_player_get_asset;

  /* Set finalize */
  object_class->constructed = melo_upnp_player_constructed;
  object_class->finalize = melo_upnp_player_finalize;
}

static void
melo_upnp_player_init (MeloUpnpPlayer *self)
{
  /* Create a new UPnP context manager */
  self->manager = gupnp_context_manager_create (0);
  g_signal_connect (self->manager, "context-available",
      (GCallback) context_available_cb, self);
  g_signal_connect (self->manager, "context-unavailable",
      (GCallback) context_unavailable_cb, self);
}

static void
melo_upnp_player_constructed (GObject *object)
{
  /* Chain constructed */
  G_OBJECT_CLASS (melo_upnp_player_parent_class)->constructed (object);

  /* Start UPnP player */
  settings_update_cb (NULL, NULL, NULL, MELO_UPNP_PLAYER (object));
}

MeloUpnpPlayer *
melo_upnp_player_new ()
{
  return g_object_new (MELO_TYPE_UPNP_PLAYER, "id", MELO_UPNP_PLAYER_ID, "name",
      "UPnP / DLNA", "description",
      "Play any media wireless on Melo with UPnP / DLNA", "icon",
      MELO_UPNP_PLAYER_ICON, NULL);
}

static bool
settings_update_cb (MeloSettings *settings, MeloSettingsGroup *group,
    char **error, void *user_data)
{
  MeloUpnpPlayer *player = MELO_UPNP_PLAYER (user_data);
  const char *name = NULL, *old_name = NULL;
  bool en = false;

  /* Get enable status and name */
  melo_settings_entry_get_boolean (player->enable, &en, NULL);
  melo_settings_entry_get_string (player->name, &name, &old_name);

  /* Set default name */
  if (!name || *name == '\0')
    name = "Melo";

  /* Stop to set new name */
  if (g_strcmp0 (name, old_name) && en)
    melo_upnp_player_stop (player);

  /* Start / stop UPnP server */
  if (en)
    melo_upnp_player_start (player, name);
  else
    melo_upnp_player_stop (player);

  return true;
}

static void
melo_upnp_player_settings (MeloPlayer *player, MeloSettings *settings)
{
  MeloUpnpPlayer *uplayer = MELO_UPNP_PLAYER (player);
  MeloSettingsGroup *group;

  /* Create global group */
  group = melo_settings_add_group (
      settings, "global", "Global", NULL, settings_update_cb, uplayer);
  uplayer->enable = melo_settings_group_add_boolean (group, "enable",
      "Enable UPnP / DLNA service", "Enable UPnP / DLNA service", true, NULL,
      MELO_SETTINGS_FLAG_NONE);
  uplayer->name = melo_settings_group_add_string (group, "name", "Device name",
      "Name of UPnP / DLNA renderer", "Melo", NULL, MELO_SETTINGS_FLAG_NONE);
}

static bool
melo_upnp_player_set_state (MeloPlayer *player, MeloPlayerState state)
{
  MeloUpnpPlayer *uplayer = MELO_UPNP_PLAYER (player);
  const gchar *pstate;

  if (!uplayer->player)
    return false;

  /* Set state */
  switch (state) {
  case MELO_PLAYER_STATE_NONE:
    uplayer->started = false;
    pstate = "STOPPED";
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
  rygel_media_player_set_playback_state (uplayer->player, pstate);
  melo_player_update_state (player, state);

  return true;
}

static bool
melo_upnp_player_set_position (MeloPlayer *player, unsigned int position)
{
  MeloUpnpPlayer *uplayer = MELO_UPNP_PLAYER (player);

  if (!uplayer->player)
    return false;

  /* Set position */
  rygel_media_player_seek (uplayer->player, position * 1000);

  return true;
}

static unsigned int
melo_upnp_player_get_position (MeloPlayer *player)
{
  MeloUpnpPlayer *uplayer = MELO_UPNP_PLAYER (player);
  guint32 pos = 0;

  /* Get position */
  if (!uplayer->player)
    pos = rygel_media_player_get_position (uplayer->player);

  return pos / 1000;
}

static char *
melo_upnp_player_get_asset (MeloPlayer *player, const char *id)
{
  return g_strdup (id);
}

static void
context_available_cb (
    GUPnPContextManager *manager, GUPnPContext *context, gpointer user_data)
{
  MeloUpnpPlayer *player = MELO_UPNP_PLAYER (user_data);
  GSSDPClient *client = GSSDP_CLIENT (context);
  const gchar *iface;

  /* Get interface and add to list */
  iface = gssdp_client_get_interface (client);
  if (!g_list_find_custom (player->ifaces, iface, (GCompareFunc) g_strcmp0)) {
    player->ifaces = g_list_prepend (player->ifaces, (gpointer *) iface);

    /* Add interface to renderer */
    if (player->renderer)
      rygel_media_device_add_interface (
          RYGEL_MEDIA_DEVICE (player->renderer), iface);
  }
}

static void
context_unavailable_cb (
    GUPnPContextManager *manager, GUPnPContext *context, gpointer user_data)
{
  MeloUpnpPlayer *player = MELO_UPNP_PLAYER (user_data);
  GSSDPClient *client = GSSDP_CLIENT (context);
  const gchar *iface;
  GList *l;

  /* Get interface and remove from list */
  iface = gssdp_client_get_interface (client);
  for (l = player->ifaces; l != NULL; l = l->next) {
    if (!g_strcmp0 ((const gchar *) l->data, iface)) {
      /* Remove interface to renderer */
      if (player->renderer)
        rygel_media_device_remove_interface (
            RYGEL_MEDIA_DEVICE (player->renderer), iface);
      /* Free interface */
      player->ifaces = g_list_delete_link (player->ifaces, l);
      break;
    }
  }
}

static void
object_available_cb (GUPnPDIDLLiteParser *parser, GUPnPDIDLLiteObject *object,
    gpointer user_data)
{
  MeloUpnpPlayer *uplayer = MELO_UPNP_PLAYER (user_data);
  MeloPlayer *player = MELO_PLAYER (uplayer);
  MeloTags *tags;

  /* Generate a new tags */
  tags = melo_tags_new ();
  if (!tags)
    return;

  /* Set tags */
  melo_tags_set_title (
      tags, g_strdup (gupnp_didl_lite_object_get_title (object)));
  melo_tags_set_artist (
      tags, g_strdup (gupnp_didl_lite_object_get_artist (object)));
  melo_tags_set_album (
      tags, g_strdup (gupnp_didl_lite_object_get_album (object)));
  melo_tags_set_genre (
      tags, g_strdup (gupnp_didl_lite_object_get_genre (object)));
  melo_tags_set_cover (
      tags, G_OBJECT (player), gupnp_didl_lite_object_get_album_art (object));

  /* Save new metadata (pending for next 'uri' event */
  melo_tags_unref (uplayer->tags);
  uplayer->tags = tags;
}

static void
notify_cb (GObject *object, GParamSpec *pspec, gpointer user_data)
{
  RygelMediaPlayer *player = RYGEL_MEDIA_PLAYER (object);
  MeloUpnpPlayer *uplayer = MELO_UPNP_PLAYER (user_data);

  /* Parse property changes */
  if (!g_strcmp0 (pspec->name, "playback-state")) {
    MeloPlayerState state = MELO_PLAYER_STATE_NONE;
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
    else if (!g_strcmp0 (pstate, "EOS"))
      uplayer->eos = true;

    /* End of stream detected */
    if (uplayer->eos && state == MELO_PLAYER_STATE_STOPPED) {
      melo_player_eos (MELO_PLAYER (uplayer));
      uplayer->eos = false;
    }

    /* Update only with valid state */
    if (state != MELO_PLAYER_STATE_NONE)
      melo_player_update_status (
          MELO_PLAYER (uplayer), state, MELO_PLAYER_STREAM_STATE_NONE, 0);

    /* Get position */
    melo_player_update_position (
        MELO_PLAYER (uplayer), rygel_media_player_get_position (player) / 1000);
  } else if (!g_strcmp0 (pspec->name, "duration")) {
    /* Get duration */
    melo_player_update_duration (MELO_PLAYER (uplayer),
        rygel_media_player_get_position (player) / 1000,
        rygel_media_player_get_duration (player) / 1000);
  } else if (!g_strcmp0 (pspec->name, "volume")) {
    gdouble volume;

    /* Get volume */
    volume = rygel_media_player_get_volume (uplayer->player);
    melo_player_update_volume (MELO_PLAYER (uplayer), volume, false);
  } else if (!g_strcmp0 (pspec->name, "metadata")) {
    GUPnPDIDLLiteParser *parser;
    gchar *meta;

    /* Get metatdata DIDLLite information */
    meta = rygel_media_player_get_metadata (player);

    /* Parse meta with GUPnP parser */
    parser = gupnp_didl_lite_parser_new ();
    g_signal_connect (
        parser, "object-available", (GCallback) object_available_cb, uplayer);
    gupnp_didl_lite_parser_parse_didl (parser, meta, NULL);
    g_object_unref (parser);
  } else if (!g_strcmp0 (pspec->name, "uri")) {
    /* First media to play */
    if (!uplayer->started) {
      MeloPlaylistEntry *entry;
      MeloTags *tags;

      /* Set player icon */
      tags = melo_tags_new ();
      melo_tags_set_cover (tags, NULL, MELO_UPNP_PLAYER_ICON);

      /* Add new UPnP / DLNA entry to playlist */
      entry = melo_playlist_entry_new (
          MELO_UPNP_PLAYER_ID, NULL, "UPNP / DNLA player", tags);
      melo_playlist_play_entry (entry);
      uplayer->started = true;
    }

    /* Add new media to player */
    if (uplayer->tags)
      melo_player_update_media (MELO_PLAYER (uplayer), NULL, uplayer->tags,
          MELO_TAGS_MERGE_FLAG_NONE);
    uplayer->tags = NULL;
  }
}

static bool
melo_upnp_player_start (MeloUpnpPlayer *player, const char *name)
{
  GstElement *playbin, *sink;
  RygelPlugin *plugin;
  RygelIconInfo *icon;
  GList *l;

  /* Already started */
  if (player->renderer)
    return true;

  MELO_LOGI ("start renderer");

  /* Create a new UPnP renderer */
  player->renderer = rygel_playbin_renderer_new (name);
  if (!player->renderer) {
    MELO_LOGE ("failed to create UPnP renderer");
    return false;
  }

  /* Get rygel plugin */
  plugin =
      rygel_media_device_get_plugin (RYGEL_MEDIA_DEVICE (player->renderer));

  /* Set JPEG icon */
  icon = rygel_icon_info_new ("image/jpeg", "jpg");
  icon->uri = "file://" MELO_UPNP_PLAYER_ICON_JPEG;
  icon->width = MELO_UPNP_PLAYER_ICON_WIDTH;
  icon->height = MELO_UPNP_PLAYER_ICON_HEIGHT;
  icon->depth = MELO_UPNP_PLAYER_ICON_DEPTH;
  rygel_plugin_add_icon (plugin, icon);

  /* Set PNG icon */
  icon = rygel_icon_info_new ("image/png", "png");
  icon->uri = "file://" MELO_UPNP_PLAYER_ICON_PNG;
  icon->width = MELO_UPNP_PLAYER_ICON_WIDTH;
  icon->height = MELO_UPNP_PLAYER_ICON_HEIGHT;
  icon->depth = MELO_UPNP_PLAYER_ICON_DEPTH;
  rygel_plugin_add_icon (plugin, icon);

  /* UPnP player */
  player->player = rygel_media_renderer_plugin_get_player (
      RYGEL_MEDIA_RENDERER_PLUGIN (plugin));

  /* Capture notifications from player */
  g_signal_connect (player->player, "notify", (GCallback) notify_cb, player);

  /* Get gstreamer playbin */
  playbin = rygel_playbin_renderer_get_playbin (player->renderer);

  /* Use MeloPlayer sink for Rygel player audio sink */
  sink = melo_player_get_sink (MELO_PLAYER (player), NULL);
  g_object_set (G_OBJECT (playbin), "audio-sink", sink, NULL);

  /* Disable video output */
  sink = gst_element_factory_make ("fakesink", NULL);
  g_object_set (G_OBJECT (playbin), "video-sink", sink, NULL);

  /* Release gstreamer playbin */
  g_object_unref (playbin);

  /* Setup interfaces */
  for (l = player->ifaces; l != NULL; l = l->next)
    rygel_media_device_add_interface (
        RYGEL_MEDIA_DEVICE (player->renderer), (const gchar *) l->data);

  return true;
}

static void
melo_upnp_player_stop (MeloUpnpPlayer *player)
{
  /* Already stopped */
  if (!player->renderer)
    return;

  MELO_LOGI ("stop renderer");

  /* Stop player */
  melo_upnp_player_set_state (MELO_PLAYER (player), MELO_PLAYER_STATE_NONE);

  /* Stop and free UPnP renderer */
  g_object_unref (player->player);
  g_object_unref (player->renderer);
  player->renderer = NULL;
  player->player = NULL;

  return;
}

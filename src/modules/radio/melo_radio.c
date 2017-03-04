/*
 * melo_radio.c: Radio module for radio / webradio playing
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

#include "melo_radio.h"
#include "melo_browser_radio.h"
#include "melo_player_radio.h"
#include "melo_playlist_simple.h"

/* Module radio info */
static MeloModuleInfo melo_radio_info = {
  .name = "Radio",
  .description = "Play radio and webradio arround the world",
  .config_id = NULL,
};

static const MeloModuleInfo *melo_radio_get_info (MeloModule *module);
static void melo_radio_register_browser (MeloModule *module,
                                         MeloBrowser *browser);

struct _MeloRadioPrivate {
  MeloBrowser *radios;
  MeloPlayer *player;
  MeloPlaylist *playlist;
};

G_DEFINE_TYPE_WITH_PRIVATE (MeloRadio, melo_radio, MELO_TYPE_MODULE)

static void
melo_radio_finalize (GObject *gobject)
{
  MeloRadioPrivate *priv =
                         melo_radio_get_instance_private (MELO_RADIO (gobject));

  if (priv->playlist)
    g_object_unref (priv->playlist);

  if (priv->player) {
    melo_module_unregister_player (MELO_MODULE (gobject), "radio_player");
    g_object_unref (priv->player);
  }

  if (priv->radios) {
    melo_module_unregister_browser (MELO_MODULE (gobject), "radio_radios");
    g_object_unref (priv->radios);
  }

  /* Chain up to the parent class */
  G_OBJECT_CLASS (melo_radio_parent_class)->finalize (gobject);
}

static void
melo_radio_class_init (MeloRadioClass *klass)
{
  MeloModuleClass *mclass = MELO_MODULE_CLASS (klass);
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  mclass->get_info = melo_radio_get_info;
  mclass->register_browser = melo_radio_register_browser;

  /* Add custom finalize() function */
  oclass->finalize = melo_radio_finalize;
}

static void
melo_radio_init (MeloRadio *self)
{
  MeloRadioPrivate *priv = melo_radio_get_instance_private (self);

  self->priv = priv;
  priv->radios = melo_browser_new (MELO_TYPE_BROWSER_RADIO, "radio_radios");
  priv->player = melo_player_new (MELO_TYPE_PLAYER_RADIO, "radio_player",
                                  melo_radio_info.name);
  priv->playlist = melo_playlist_new (MELO_TYPE_PLAYLIST_SIMPLE,
                                      "radio_playlist");

  if (!priv->radios || !priv->player || !priv->player)
    return;

  /* Setup playlist with cover URL override */
  g_object_set (G_OBJECT (priv->playlist), "override-cover-url", TRUE, NULL);

  /* Register browser and player */
  melo_module_register_browser (MELO_MODULE (self), priv->radios);
  melo_module_register_player (MELO_MODULE (self), priv->player);

  /* Create links between browser, player and playlist */
  melo_player_set_playlist (priv->player, priv->playlist);
}

static void
melo_radio_register_browser (MeloModule *module, MeloBrowser *browser)
{
  MeloRadioPrivate *priv = (MELO_RADIO (module))->priv;

  /* Attach player to new browser */
  melo_browser_set_player (browser, priv->player);
}

static const MeloModuleInfo *
melo_radio_get_info (MeloModule *module)
{
  return &melo_radio_info;
}

/*
 * melo_upnp.c: UPnP / DLNA renderer module
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

#include "melo_player_upnp.h"
#include "melo_config_upnp.h"
#include "melo_upnp.h"

/* Module upnp info */
static MeloModuleInfo melo_upnp_info = {
  .name = "UPnP / DLNA",
  .description = "Play any media wireless on Melo with UPnP / DLNA",
  .config_id = "upnp",
};

static const MeloModuleInfo *melo_upnp_get_info (MeloModule *module);

struct _MeloUpnpPrivate {
  MeloPlayer *player;
  GMutex mutex;
  MeloConfig *config;
  gchar *name;
};

G_DEFINE_TYPE_WITH_PRIVATE (MeloUpnp, melo_upnp, MELO_TYPE_MODULE)

static void
melo_upnp_finalize (GObject *gobject)
{
  MeloUpnpPrivate *priv = melo_upnp_get_instance_private (MELO_UPNP (gobject));

  /* Free UPnP player */
  if (priv->player) {
    melo_module_unregister_player (MELO_MODULE (gobject), "upnp_player");
    g_object_unref (priv->player);
  }

  /* Free name */
  g_free (priv->name);

  /* Clear mutex */
  g_mutex_clear (&priv->mutex);

  /* Save and free configuration */
  melo_config_save_to_def_file (priv->config);
  g_object_unref (priv->config);

  /* Chain up to the parent class */
  G_OBJECT_CLASS (melo_upnp_parent_class)->finalize (gobject);
}

static void
melo_upnp_class_init (MeloUpnpClass *klass)
{
  MeloModuleClass *mclass = MELO_MODULE_CLASS (klass);
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  mclass->get_info = melo_upnp_get_info;

  /* Add custom finalize() function */
  oclass->finalize = melo_upnp_finalize;
}

static void
melo_upnp_init (MeloUpnp *self)
{
  MeloUpnpPrivate *priv = melo_upnp_get_instance_private (self);

  self->priv = priv;

  /* Init mutex */
  g_mutex_init (&priv->mutex);

  /* Load configuration */
  priv->config = melo_config_upnp_new ();
  if (!melo_config_load_from_def_file (priv->config))
    melo_config_load_default (priv->config);

  /* Get name and port from configuration */
  if (!melo_config_get_string (priv->config, "general", "name", &priv->name))
    priv->name = g_strdup ("Melo");

  /* Create and register UPnP player */
  priv->player = melo_player_new (MELO_TYPE_PLAYER_UPNP, "upnp_player",
                                  melo_upnp_info.name);
  melo_module_register_player (MELO_MODULE (self), priv->player);

  /* Start UPnP renderer */
  melo_player_upnp_start (MELO_PLAYER_UPNP (priv->player), priv->name);

  /* Add config handler for update */
  melo_config_set_update_callback (priv->config, "general",
                                   melo_config_upnp_update, self);
}

static const MeloModuleInfo *
melo_upnp_get_info (MeloModule *module)
{
  return &melo_upnp_info;
}

gboolean
melo_upnp_set_name (MeloUpnp *up, const gchar *name)
{
  MeloUpnpPrivate *priv = up->priv;

  /* Replace name */
  g_free (priv->name);
  priv->name = g_strdup (name);

  /* Restart player with new name */
  melo_player_upnp_stop (MELO_PLAYER_UPNP (priv->player));
  melo_player_upnp_start (MELO_PLAYER_UPNP (priv->player), name);

  return TRUE;
}

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

#include <stddef.h>

#include <melo/melo_module.h>

#define MELO_LOG_TAG "melo_upnp"
#include <melo/melo_log.h>

#include "melo_upnp_player.h"

#define MELO_UPNP_ID "com.sparod.upnp"

static MeloUpnpPlayer *player;

static void
melo_upnp_enable (void)
{
  /* Create UPnP player */
  player = melo_upnp_player_new ();
}

static void
melo_upnp_disable (void)
{
  /* Release UPnP player */
  g_object_unref (player);
}

static const char *melo_upnp_player_list[] = {MELO_UPNP_PLAYER_ID, NULL};

const MeloModule MELO_MODULE_SYM = {
    .id = MELO_UPNP_ID,
    .version = MELO_VERSION (1, 0, 0),
    .api_version = MELO_API_VERSION,

    .name = "UPnP",
    .description = "UPnP / DLNA module to play medias from network.",

    .browser_list = NULL,
    .player_list = melo_upnp_player_list,

    .enable_cb = melo_upnp_enable,
    .disable_cb = melo_upnp_disable,
};

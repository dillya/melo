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

#define MELO_LOG_TAG "melo_radio"
#include <melo/melo_log.h>

#include "melo_radio_browser.h"
#include "melo_radio_player.h"

#define MELO_RADIO_ID "com.sparod.radio"

static MeloRadioBrowser *browser;
static MeloRadioPlayer *player;

static void
melo_radio_enable (void)
{
  /* Create radio browser */
  browser = melo_radio_browser_new ();

  /* Create radio player */
  player = melo_radio_player_new ();
}

static void
melo_radio_disable (void)
{
  /* Release radio player */
  g_object_unref (player);

  /* Release radio browser */
  g_object_unref (browser);
}

static const char *melo_radio_browser_list[] = {MELO_RADIO_BROWSER_ID, NULL};
static const char *melo_radio_player_list[] = {MELO_RADIO_PLAYER_ID, NULL};

const MeloModule MELO_MODULE_SYM = {
    .id = MELO_RADIO_ID,
    .version = MELO_VERSION (1, 0, 0),
    .api_version = MELO_API_VERSION,

    .name = "Radio",
    .description = "Browse and play all radios from radio directories.",

    .browser_list = melo_radio_browser_list,
    .player_list = melo_radio_player_list,

    .enable_cb = melo_radio_enable,
    .disable_cb = melo_radio_disable,
};

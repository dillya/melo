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

#define MELO_LOG_TAG "melo_file"
#include <melo/melo_log.h>

#include "melo_file_browser.h"
#include "melo_file_player.h"

#define MELO_FILE_ID "com.sparod.file"

static MeloFileBrowser *browser;
static MeloFilePlayer *player;

static void
melo_file_enable (void)
{
  /* Create file browser */
  browser = melo_file_browser_new ();

  /* Create file player */
  player = melo_file_player_new ();
}

static void
melo_file_disable (void)
{
  /* Release file player */
  g_object_unref (player);

  /* Release file browser */
  g_object_unref (browser);
}

static const char *melo_file_browser_list[] = {MELO_FILE_BROWSER_ID, NULL};
static const char *melo_file_player_list[] = {MELO_FILE_PLAYER_ID, NULL};

const MeloModule MELO_MODULE_SYM = {
    .id = MELO_FILE_ID,
    .version = MELO_VERSION (1, 0, 0),
    .api_version = MELO_API_VERSION,

    .name = "File",
    .description = "Browse and play all files from device and local network.",

    .browser_list = melo_file_browser_list,
    .player_list = melo_file_player_list,

    .enable_cb = melo_file_enable,
    .disable_cb = melo_file_disable,
};

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

#include <gst/gst.h>

#include "melo/melo.h"
#include "melo/melo_cover.h"
#include "melo/melo_log.h"
#include "melo/melo_playlist.h"

#include "melo_library_browser.h"
#include "melo_library_priv.h"
#include "melo_player_priv.h"

/* Default playlist */
static MeloPlaylist *def_playlist;

/* Library browser */
static MeloLibraryBrowser *lib_browser;

/**
 * melo_init:
 * @argc: (inout) (allow-none): pointer to application's argc
 * @argv: (inout) (array length=argc) (allow-none): pointer to application's
 * argv
 *
 * Initializes the Melo library internal data and dependencies as Gstreamer.
 */
void
melo_init (int *argc, char ***argv)
{
  /* First initialize log sub-system */
  melo_log_init ();

  /* Initialize gstreamer */
  gst_init (argc, argv);

  /* Initialize cover cache */
  melo_cover_cache_init ();

  /* Initialize player settings */
  melo_player_settings_init ();

  /* Initialize library */
  melo_library_init ();

  /* Create default playlist */
  def_playlist = melo_playlist_new (NULL);

  /* Create library browser */
  lib_browser = melo_library_browser_new ();
}

/**
 * melo_deinit:
 *
 * Clean up any resources created by Melo library in melo_init() and its
 * dependencies initialization.
 */
void
melo_deinit (void)
{
  /* Destroy library browser */
  g_object_unref (lib_browser);

  /* Destroy default playlist */
  g_object_unref (def_playlist);

  /* Close library */
  melo_library_deinit ();

  /* Release player settings */
  melo_player_settings_deinit ();

  /* Clean cover cache */
  melo_cover_cache_deinit ();

  /* Clean gstreamer */
  gst_deinit ();
}

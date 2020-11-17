/*
 * Copyright (C) 2020 Alexandre Dilly <dillya@sparod.com>
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA.
 */

#include <melo/melo_browser.h>
#include <melo/melo_cover.h>
#include <melo/melo_player.h>

#define MELO_LOG_TAG "melo_asset"
#include <melo/melo_log.h>

#include "asset.h"

/**
 * asset_cb:
 * @server: the #MeloHttpServer
 * @connection: the #MeloHttpServerConnection
 * @path: the path of the request
 * @user_data: data pointer associated to the callback
 *
 * Asset HTTP server request callback.
 */
void
asset_cb (MeloHttpServer *server, MeloHttpServerConnection *connection,
    const char *path, void *user_data)
{
  char *uri = NULL;

  /* Invalid root path */
  if (strncmp (path, "/asset/", 7))
    return;
  path += 7;

  /* Parse path */
  if (!strncmp (path, "browser/", 8)) {
    const char *p;
    char *id;

    /* Move to path start */
    path += 8;

    /* Get browser ID */
    p = strchr (path, '/');
    if (!p)
      return;
    id = strndup (path, p - path);
    path = p + 1;

    /* Get browser asset */
    uri = melo_browser_get_asset (id, path);
    g_free (id);
  } else if (!strncmp (path, "player/", 7)) {
    const char *p;
    char *id;

    /* Move to path start */
    path += 7;

    /* Get player ID */
    p = strchr (path, '/');
    if (!p)
      return;
    id = strndup (path, p - path);
    path = p + 1;

    /* Get player asset */
    uri = melo_player_get_asset (id, path);
    g_free (id);
  } else
    uri = melo_cover_cache_get_path (path);

  /* Serve asset */
  if (uri) {
    /* Serve as file or URL */
    if (*uri == '/')
      melo_http_server_connection_send_file (connection, uri);
    else
      melo_http_server_connection_send_url (connection, uri);
    g_free (uri);
  }
}

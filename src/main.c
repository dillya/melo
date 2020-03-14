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

#include <glib.h>
#ifdef G_OS_UNIX
#include <glib-unix.h>
#endif

#include <config.h>

#include <melo/melo.h>
#include <melo/melo_http_server.h>
#include <melo/melo_module.h>

#define MELO_LOG_TAG "melo"
#include <melo/melo_log.h>

#include "asset.h"
#include "websocket.h"

/** Default web UI path (for HTTP server). */
#ifndef MELO_WEB_UI_PATH
#define MELO_WEB_UI_PATH "/usr/share/melo/ui"
#endif

#ifdef G_OS_UNIX
static gboolean
melo_sigint_handler (gpointer user_data)
{
  GMainLoop *loop = (GMainLoop *) user_data;

  /* Quit main loop and quit program */
  g_main_loop_quit (loop);

  return G_SOURCE_REMOVE;
}
#endif

int
main (int argc, char *argv[])
{
  MeloHttpServer *http_server;
  GMainLoop *loop;
  int ret = -1;

  /* Initialize Melo library */
  melo_init (&argc, &argv);

  /* Load modules */
  melo_module_load ();

  /* Create HTTP server for remote control */
  http_server = melo_http_server_new ();
  if (!http_server) {
    MELO_LOGE ("failed to create main HTTP server");
    goto end;
  }

  /* Add file handler to HTTP server */
  if (!melo_http_server_add_file_handler (
          http_server, NULL, MELO_WEB_UI_PATH)) {
    MELO_LOGE ("failed to add file handler to HTTP server");
    g_object_unref (http_server);
    goto end;
  }

  /* Add asset handler to HTTP server */
  if (!melo_http_server_add_handler (http_server, "/asset/", asset_cb, NULL)) {
    MELO_LOGE ("failed to add asset handler to HTTP server");
    goto end;
  }

  /* Add main websocket handler to HTTP server */
  if (!melo_http_server_add_websocket_handler (http_server, "/api/event/", NULL,
          NULL, websocket_event_cb, NULL, NULL) ||
      !melo_http_server_add_websocket_handler (http_server, "/api/request/",
          NULL, NULL, websocket_conn_request_cb, websocket_request_cb, NULL)) {
    MELO_LOGE ("failed to add websocket handlers to HTTP server");
    g_object_unref (http_server);
    goto end;
  }

  /* Start HTTP server */
  if (!melo_http_server_start (http_server, 8080, 0)) {
    MELO_LOGE ("failed to start HTTP server");
    g_object_unref (http_server);
    goto end;
  }

  /* Start main loop */
  loop = g_main_loop_new (NULL, FALSE);

#ifdef G_OS_UNIX
  g_unix_signal_add (SIGINT, (GSourceFunc) melo_sigint_handler, loop);
#endif

  /* Run main loop */
  g_main_loop_run (loop);

  /* Free main loop */
  g_main_loop_unref (loop);

  /* Stop HTTP server */
  melo_http_server_stop (http_server);

  /* Remove file handler from HTTP server */
  melo_http_server_remove_handler (http_server, NULL);

  /* Destroy HTTP server */
  g_object_unref (http_server);

  /* Exit successfully */
  ret = 0;

end:
  /* Unload modules */
  melo_module_unload ();

  /* Clean Melo library */
  melo_deinit ();

  return ret;
}

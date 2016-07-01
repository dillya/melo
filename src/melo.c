/*
 * melo.c: main entry point of Melo program
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>
#ifdef G_OS_UNIX
#include <glib-unix.h>
#endif

#include "melo_file.h"
#include "melo_httpd.h"
#include "melo_player_jsonrpc.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef G_OS_UNIX
gboolean
melo_sigint_handler (gpointer user_data)
{
  GMainLoop *loop = (GMainLoop *) user_data;

  /* SIGINT capture: stop program */
  g_main_loop_quit (loop);

  return G_SOURCE_REMOVE;
}
#endif

int
main (int argc, char *argv[])
{
  /* Command line opetions */
  gboolean verbose = FALSE;
  GOptionEntry options[] = {
    {"verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose, "Be verbose", NULL},
    {NULL}
  };
  GOptionContext *ctx;
  GError *err = NULL;
  /* HTTP server */
  MeloHTTPD *server;
  /* Main loop */
  GMainLoop *loop;

  /* Create option context parser */
  ctx = g_option_context_new ("");

  /* Add main entries to context */
  g_option_context_add_main_entries (ctx, options, NULL);

  /* Parse command line */
  if (!g_option_context_parse (ctx, &argc, &argv, &err)) {
    g_printerr ("Option parsion failed: %s\n", err->message);
    g_clear_error (&err);
    g_option_context_free (ctx);
    return -1;
  }

  /* Free option context */
  g_option_context_free (ctx);

  /* Register standard JSON-RPC methods */
  melo_module_register_methods ();
  melo_browser_register_methods ();
  melo_player_register_methods ();

  /* Register built-in modules */
  melo_module_register (MELO_TYPE_FILE, "file");

  /* Create and start HTTP server */
  server = melo_httpd_new ();
  if (!melo_httpd_start (server, 8080))
    goto end;

  /* Start main loop */
  loop = g_main_loop_new (NULL, FALSE);

#ifdef G_OS_UNIX
  /* Install a signal handler on SIGINT */
  g_unix_signal_add (SIGINT, melo_sigint_handler, loop);
#endif

  /* Run main loop */
  g_main_loop_run (loop);

  /* End of loop: free main loop */
  g_main_loop_unref (loop);

end:
  /* Stop and Free HTTP server */
  melo_httpd_stop (server);
  g_object_unref (server);

  /* Unregister built-in modules */
  melo_module_unregister ("file");

  /* Unregister standard JSON-RPC methods */
  melo_player_unregister_methods ();
  melo_browser_unregister_methods ();
  melo_module_unregister_methods ();

  return 0;
}

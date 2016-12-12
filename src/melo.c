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

#include "melo.h"
#include "melo_plugin.h"
#include "melo_config_main.h"

#include "melo_config_jsonrpc.h"
#include "melo_module_jsonrpc.h"
#include "melo_browser_jsonrpc.h"
#include "melo_player_jsonrpc.h"
#include "melo_playlist_jsonrpc.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if HAVE_LIBNM_GLIB
#include "melo_network.h"
#endif

#if HAVE_MELO_MODULE_FILE
#include "modules/file/melo_file.h"
#endif
#if HAVE_MELO_MODULE_RADIO
#include "modules/radio/melo_radio.h"
#endif
#if HAVE_MELO_MODULE_UPNP
#include "modules/upnp/melo_upnp.h"
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
  gboolean daemonize = FALSE;
  GOptionEntry options[] = {
    {"daemon", 'd', 0, G_OPTION_ARG_NONE, &daemonize, "Run as daemon", NULL},
    {"verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose, "Be verbose", NULL},
    {NULL}
  };
  GOptionContext *ctx;
  GError *err = NULL;
#if HAVE_LIBNM_GLIB
  /* Network configuration */
  MeloNetwork *net;
#endif
  /* Main configuration */
  MeloConfig *config;
  /* Melo context */
  MeloContext context;
  /* Main loop */
  GMainLoop *loop;

  /* Create option context parser */
  ctx = g_option_context_new ("");

  /* Add main entries to context */
  g_option_context_add_main_entries (ctx, options, NULL);

  /* Add gstreamer group to context */
  g_option_context_add_group (ctx, gst_init_get_option_group ());

  /* Parse command line */
  if (!g_option_context_parse (ctx, &argc, &argv, &err)) {
    g_printerr ("Option parsion failed: %s\n", err->message);
    g_clear_error (&err);
    g_option_context_free (ctx);
    return -1;
  }

  /* Free option context */
  g_option_context_free (ctx);

  /* Daemonize */
  if (daemonize && daemon (1, 0))
      return -1;

  /* Load configuration */
  config = melo_config_main_new ();
  if (!melo_config_load_from_def_file (config))
    melo_config_load_default (config);

  /* Get name */
  if (!melo_config_get_string (config, "general", "name", &context.name) ||
      !context.name)
    context.name = g_strdup ("Melo");

  /* Get HTTP server port */
  if (!melo_config_get_integer (config, "http", "port", &context.port))
    context.port = 8080;

  /* Register standard JSON-RPC methods */
  melo_config_register_methods ();
  melo_module_register_methods ();
  melo_browser_register_methods ();
  melo_player_register_methods ();
  melo_playlist_register_methods ();

#if HAVE_LIBNM_GLIB
  /* Add network controler and register its JSON-RPC methods */
  net = melo_network_new ();
  melo_network_jsonrpc_register_methods (net);
#endif

  /* Register built-in modules */
#if HAVE_MELO_MODULE_FILE
  melo_module_register (MELO_TYPE_FILE, "file");
#endif
#if HAVE_MELO_MODULE_RADIO
  melo_module_register (MELO_TYPE_RADIO, "radio");
#endif
#if HAVE_MELO_MODULE_UPNP
  melo_module_register (MELO_TYPE_UPNP, "upnp");
#endif

  /* Load plugins */
  melo_plugin_load_all (TRUE);

  /* Create and start HTTP server */
  context.server = melo_httpd_new ();
  if (!melo_httpd_start (context.server, context.port, context.name))
    goto end;

  /* Load HTTP server configuration */
  melo_config_main_load_http (config, context.server);

  /* Add config handler for general section */
  melo_config_set_check_callback (config, "general",
                                  melo_config_main_check_general, &context);
  melo_config_set_update_callback (config, "general",
                                  melo_config_main_update_general, &context);

  /* Add config handler for HTTP server */
  melo_config_set_check_callback (config, "http", melo_config_main_check_http,
                                  context.server);
  melo_config_set_update_callback (config, "http", melo_config_main_update_http,
                                   context.server);

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
  melo_httpd_stop (context.server);
  g_object_unref (context.server);

  /* Unload plugins */
  melo_plugin_unload_all ();

  /* Unregister built-in modules */
#if HAVE_MELO_MODULE_UPNP
  melo_module_unregister ("upnp");
#endif
#if HAVE_MELO_MODULE_RADIO
  melo_module_unregister ("radio");
#endif
#if HAVE_MELO_MODULE_FILE
  melo_module_unregister ("file");
#endif

#if HAVE_LIBNM_GLIB
  /* Unregister network controler and its JSON-RPC methods */
  melo_network_jsonrpc_unregister_methods ();
  g_object_unref (net);
#endif

  /* Unregister standard JSON-RPC methods */
  melo_playlist_unregister_methods ();
  melo_player_unregister_methods ();
  melo_browser_unregister_methods ();
  melo_module_unregister_methods ();
  melo_config_unregister_methods ();

  /* Save configuration */
  melo_config_save_to_def_file (config);

  /* Free configuration */
  g_object_unref (config);

  return 0;
}

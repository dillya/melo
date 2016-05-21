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

#include <glib.h>
#include <stdio.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
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

  return 0;
}

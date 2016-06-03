/*
 * melo_httpd.c: HTTP server for Melo remote control
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
#include <string.h>
#include <errno.h>

#include <glib.h>

#include "melo_httpd.h"
#include "melo_httpd_file.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

SoupServer *
melo_httpd_new (guint port)
{
  SoupServer *server;
  GError *err = NULL;
  gboolean res;

  /* Create a new HTTP server */
  server = soup_server_new (0, NULL);

  /* Start listening */
  res = soup_server_listen_all (server, port, 0, &err);
  if (res == FALSE) {
    g_clear_error (&err);
    g_object_unref(&server);
    return NULL;
  }

  /* Add a default handler */
  soup_server_add_handler (server, NULL, melo_httpd_file_handler, NULL,
                           NULL);

  return server;
}

void
melo_httpd_free (SoupServer *server)
{
  /* Disconnect all remaining clients */
  soup_server_disconnect (server);

  /* Free HTTP server */
  g_object_unref (server);
}

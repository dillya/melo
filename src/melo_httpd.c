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
#include "melo_httpd_jsonrpc.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

struct _MeloHTTPDPrivate {
  SoupServer *server;

  /* Thread pools */
  GThreadPool *jsonrpc_pool;
};

G_DEFINE_TYPE_WITH_PRIVATE (MeloHTTPD, melo_httpd, G_TYPE_OBJECT)

static void
melo_httpd_finalize (GObject *gobject)
{
  MeloHTTPDPrivate *priv =
                         melo_httpd_get_instance_private (MELO_HTTPD (gobject));

  /* Free HTTP server */
  g_object_unref (priv->server);

  /* Free thread pools */
  g_thread_pool_free (priv->jsonrpc_pool, TRUE, FALSE);

  /* Chain up to the parent class */
  G_OBJECT_CLASS (melo_httpd_parent_class)->finalize (gobject);
}

static void
melo_httpd_class_init (MeloHTTPDClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  /* Add custom finalize() function */
  object_class->finalize = melo_httpd_finalize;
}

static void
melo_httpd_init (MeloHTTPD *self)
{
  MeloHTTPDPrivate *priv = melo_httpd_get_instance_private (self);

  self->priv = priv;

  /* Create a new HTTP server */
  priv->server = soup_server_new (0, NULL);

  /* Init thread pools */
  priv->jsonrpc_pool = g_thread_pool_new (melo_httpd_jsonrpc_thread_handler,
                                          priv->server, 10, FALSE, NULL);
}

gboolean
melo_httpd_start (MeloHTTPD *httpd, guint port)
{
  MeloHTTPDPrivate *priv = httpd->priv;
  SoupServer *server = priv->server;
  GError *err = NULL;
  gboolean res;

  /* Start listening */
  res = soup_server_listen_all (server, port, 0, &err);
  if (res == FALSE) {
    g_clear_error (&err);
    return FALSE;
  }

  /* Add a default handler */
  soup_server_add_handler (server, NULL, melo_httpd_file_handler, NULL,
                           NULL);

  /* Add an handler for JSON-RPC */
  soup_server_add_handler (server, "/rpc", melo_httpd_jsonrpc_handler,
                           priv->jsonrpc_pool, NULL);

  return TRUE;
}

void
melo_httpd_stop (MeloHTTPD *httpd)
{
  /* Disconnect all remaining clients */
  soup_server_disconnect (httpd->priv->server);
}

MeloHTTPD *
melo_httpd_new (void)
{
  return g_object_new (MELO_TYPE_HTTPD, NULL);
}

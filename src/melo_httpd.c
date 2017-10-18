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

#include "melo_tags.h"
#include "melo_avahi.h"
#include "melo_httpd.h"
#include "melo_httpd_file.h"
#include "melo_httpd_cover.h"
#include "melo_httpd_jsonrpc.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define MELO_HTTPD_REALM "Melo"

static gboolean melo_httpd_basic_auth_callback (SoupAuthDomain *auth_domain,
                                                SoupMessage *msg,
                                                const char *username,
                                                const char *password,
                                                gpointer data);

struct _MeloHTTPDPrivate {
  GMutex mutex;
  SoupServer *server;

  /* Avahi client */
  MeloAvahi *avahi;
  const MeloAvahiService *http_service;

  /* Authentication */
  SoupAuthDomain *auth_domain;
  gboolean auth_enabled;
  gchar *username;
  gchar *password;

  /* Thread pools */
  GThreadPool *jsonrpc_pool;
  GThreadPool *cover_pool;
};

G_DEFINE_TYPE_WITH_PRIVATE (MeloHTTPD, melo_httpd, G_TYPE_OBJECT)

static void
melo_httpd_finalize (GObject *gobject)
{
  MeloHTTPDPrivate *priv =
                         melo_httpd_get_instance_private (MELO_HTTPD (gobject));

  /* Free cover URL base */
  melo_tags_set_cover_url_base (NULL);

  /* Free avahi client */
  if (priv->avahi)
    g_object_unref (priv->avahi);

  /* Free HTTP server */
  g_object_unref (priv->server);

  /* Free thread pools */
  g_thread_pool_free (priv->jsonrpc_pool, TRUE, FALSE);
  g_thread_pool_free (priv->cover_pool, TRUE, FALSE);

  /* free authentication */
  g_object_unref (priv->auth_domain);
  g_free (priv->username);
  g_free (priv->password);

  /* Clear mutex */
  g_mutex_clear (&priv->mutex);

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
  priv->username = NULL;
  priv->password = NULL;

  /* Init mutex */
  g_mutex_init (&priv->mutex);

  /* Create a new HTTP server */
  priv->server = soup_server_new (0, NULL);

  /* Create a basic authentication domain
   * Note: only /version can be accessed without credentials
   */
  priv->auth_domain = soup_auth_domain_basic_new (
                          SOUP_AUTH_DOMAIN_REALM, MELO_HTTPD_REALM,
                          SOUP_AUTH_DOMAIN_ADD_PATH, "",
                          SOUP_AUTH_DOMAIN_REMOVE_PATH, "/version",
                          SOUP_AUTH_DOMAIN_BASIC_AUTH_CALLBACK,
                              melo_httpd_basic_auth_callback,
                          SOUP_AUTH_DOMAIN_BASIC_AUTH_DATA, priv,
                          NULL);
  priv->auth_enabled = FALSE;

  /* Init thread pools */
  priv->jsonrpc_pool = g_thread_pool_new (melo_httpd_jsonrpc_thread_handler,
                                          priv->server, 10, FALSE, NULL);
  priv->cover_pool = g_thread_pool_new (melo_httpd_cover_thread_handler,
                                        priv->server, 10, FALSE, NULL);

  /* Create an avahi client */
  priv->avahi = melo_avahi_new ();
}

MeloHTTPD *
melo_httpd_new (void)
{
  return g_object_new (MELO_TYPE_HTTPD, NULL);
}

static void
melo_httpd_version_handler (SoupServer *server, SoupMessage *msg,
                            const char *path, GHashTable *query,
                            SoupClientContext *client, gpointer user_data)
{
  static const char *version = PACKAGE_STRING;

  /* Allow requests from Sparod website */
  soup_message_headers_append (msg->response_headers,
                               "Access-Control-Allow-Origin",
                               "http://sparod.com");

  /* Set response with PACKAGE_STRING which contains name and version */
  soup_message_set_status (msg, SOUP_STATUS_OK);
  soup_message_set_response (msg, "text/plain", SOUP_MEMORY_STATIC,
                             version, strlen (version));
}

gboolean
melo_httpd_start (MeloHTTPD *httpd, guint port, const gchar *name)
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

  /* Add an handler for version */
  soup_server_add_handler (server, "/version", melo_httpd_version_handler, NULL,
                           NULL);

  /* Add an handler for JSON-RPC */
  soup_server_add_handler (server, "/rpc", melo_httpd_jsonrpc_handler,
                           priv->jsonrpc_pool, NULL);

  /* Add an handler for covers */
  soup_server_add_handler (server, "/cover", melo_httpd_cover_handler,
                           priv->cover_pool, NULL);

  /* Set cover URL base */
  melo_tags_set_cover_url_base ("cover");

  /* Add avahi service */
  if (priv->avahi)
    priv->http_service = melo_avahi_add_service (priv->avahi, name,
                                                 "_http._tcp", port, NULL);

  return TRUE;
}

void
melo_httpd_stop (MeloHTTPD *httpd)
{
  MeloHTTPDPrivate *priv = httpd->priv;

  /* Disconnect all remaining clients */
  soup_server_disconnect (priv->server);

  /* Remove avahi service */
  if (priv->avahi)
    melo_avahi_remove_service (priv->avahi, priv->http_service);
}

void
melo_httpd_set_name (MeloHTTPD *httpd, const gchar *name)
{
  MeloHTTPDPrivate *priv = httpd->priv;

  /* Update avahi name service */
  if (priv->avahi && priv->http_service)
    melo_avahi_update_service (priv->avahi, priv->http_service, name, NULL, 0,
                               FALSE);
}

void
melo_httpd_auth_enable (MeloHTTPD *httpd)
{
   if (httpd->priv->auth_enabled)
     return;
  httpd->priv->auth_enabled = TRUE;

  /* Add authentication domain */
  soup_server_add_auth_domain (httpd->priv->server, httpd->priv->auth_domain);
}

void
melo_httpd_auth_disable (MeloHTTPD *httpd)
{
   if (!httpd->priv->auth_enabled)
     return;
  httpd->priv->auth_enabled = FALSE;

  /* Remove authentication domain */
  soup_server_remove_auth_domain (httpd->priv->server,
                                  httpd->priv->auth_domain);
}

void
melo_httpd_auth_set_username (MeloHTTPD *httpd, const gchar *username)
{
  g_mutex_lock (&httpd->priv->mutex);
  g_free (httpd->priv->username);
  httpd->priv->username = g_strdup (username);
  g_mutex_unlock (&httpd->priv->mutex);
}

void
melo_httpd_auth_set_password (MeloHTTPD *httpd, const gchar *password)
{
  g_mutex_lock (&httpd->priv->mutex);
  g_free (httpd->priv->password);
  httpd->priv->password = g_strdup (password);
  g_mutex_unlock (&httpd->priv->mutex);
}

gchar *
melo_httpd_auth_get_username (MeloHTTPD *httpd)
{
  gchar *username;
  g_mutex_lock (&httpd->priv->mutex);
  username = g_strdup (httpd->priv->username);
  g_mutex_unlock (&httpd->priv->mutex);
  return username;
}

gchar *
melo_httpd_auth_get_password (MeloHTTPD *httpd)
{
  gchar *password;
  g_mutex_lock (&httpd->priv->mutex);
  password = g_strdup (httpd->priv->password);
  g_mutex_unlock (&httpd->priv->mutex);
  return password;
}

static gboolean
melo_httpd_basic_auth_callback (SoupAuthDomain *auth_domain, SoupMessage *msg,
                                const char *username, const char *password,
                                gpointer data)
{
  MeloHTTPDPrivate *priv = (MeloHTTPDPrivate *) data;
  gboolean ret = TRUE;

  /* Lock auth login */
  g_mutex_lock (&priv->mutex);

  if (priv->password)
    ret = !strcmp (password, priv->password) &&
          (!priv->username || !strcmp (username, priv->username));

  /* Unlock auth login */
  g_mutex_unlock (&priv->mutex);

  return ret;
}

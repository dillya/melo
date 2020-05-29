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

#include <libsoup/soup.h>

#define MELO_LOG_TAG "http_server"
#include "melo/melo_log.h"

#include "melo/melo_http_server.h"

#include "melo_http_server_file.h"
#include "melo_http_server_url.h"
#include "melo_websocket_priv.h"

struct _MeloHttpServer {
  /* Parent instance */
  GObject parent_instance;

  /* HTTP server */
  SoupServer *server;
  SoupSession *session;
};

struct _MeloHttpServerConnection {
  SoupServer *server;
  SoupSession *session;
  SoupMessage *msg;
  SoupClientContext *client;
};

G_DEFINE_TYPE (MeloHttpServer, melo_http_server, G_TYPE_OBJECT)

static void
melo_http_server_finalize (GObject *gobject)
{
  MeloHttpServer *self = MELO_HTTP_SERVER (gobject);

  /* Free HTTP server */
  g_object_unref (self->server);

  /* Free HTTP client */
  g_object_unref (self->session);

  /* Chain up to the parent class */
  G_OBJECT_CLASS (melo_http_server_parent_class)->finalize (gobject);
}

static void
melo_http_server_class_init (MeloHttpServerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  /* Attach finalize function */
  object_class->finalize = melo_http_server_finalize;
}

static void
melo_http_server_init (MeloHttpServer *self)
{
  /* Create a new HTTP server */
  self->server = soup_server_new (0, NULL);

  /* Create a new HTTP client */
  self->session = soup_session_new ();
}

/**
 * melo_http_server_new:
 *
 * Creates a new HTTP server.
 *
 * Returns: (transfer full): the new #MeloHttpServer.
 */
MeloHttpServer *
melo_http_server_new ()
{
  return g_object_new (MELO_TYPE_HTTP_SERVER, NULL);
}

typedef struct _MeloHttpServerData {
  MeloHttpServer *server;
  MeloHttpServerCb cb;
  void *user_data;
} MeloHttpServerData;

static void
melo_http_server_handler (SoupServer *server, SoupMessage *msg,
    const char *path, GHashTable *query, SoupClientContext *client,
    gpointer user_data)
{
  MeloHttpServerData *data = user_data;
  MeloHttpServerConnection conn = {
      .server = server,
      .session = data->server->session,
      .msg = msg,
      .client = client,
  };

  data->cb (data->server, &conn, path, data->user_data);
}

/**
 * melo_http_server_add_handler:
 * @srv: a #MeloHttpServer instance
 * @path: the top-level path for the handler, can be %NULL
 * @cb: the function called when a new request is received
 * @user_data: the data to pass to @cb
 *
 * This function adds an handler to capture a request and generate an HTTP
 * response.
 *
 * Returns: %true if the handler has been added, %false otherwise.
 */
bool
melo_http_server_add_handler (
    MeloHttpServer *srv, const char *path, MeloHttpServerCb cb, void *user_data)
{
  MeloHttpServerData *data;

  if (!srv || !cb)
    return false;

  /* Allocate new callback data */
  data = g_malloc (sizeof (*data));
  if (!data)
    return false;

  /* Save callback and user data */
  data->server = srv;
  data->cb = cb;
  data->user_data = user_data;

  /* Add file handler */
  soup_server_add_handler (
      srv->server, path, melo_http_server_handler, data, g_free);

  return true;
}

/**
 * melo_http_server_add_file_handler:
 * @srv: a #MeloHttpServer instance
 * @path: the top-level path for the handler, can be %NULL
 * @www_path: the root directory of file system to serve
 *
 * This function adds an handler to serve files from file system.
 *
 * Returns: %true if the handler has been added, %false otherwise.
 */
bool
melo_http_server_add_file_handler (
    MeloHttpServer *srv, const char *path, const char *www_path)
{
  gchar *root_path;

  if (!srv)
    return false;

  /* Root path is not specified */
  if (!www_path) {
    MELO_LOGE ("invalid www path");
    return false;
  }

  /* Copy root path */
  root_path = g_strdup (www_path);

  /* Add file handler */
  soup_server_add_handler (
      srv->server, path, melo_http_server_file_handler, root_path, g_free);

  return true;
}

static void
websocket_msg_cb (SoupWebsocketConnection *connection, gint type,
    GBytes *message, gpointer user_data)
{
  MeloWebsocket *ws = user_data;

  /* Signal user for message */
  melo_websocket_signal_message (ws, message);
}

static void
websocket_close_cb (SoupWebsocketConnection *connection, gpointer user_data)
{
  MeloWebsocket *ws = user_data;

  /* Signal user for disconnection */
  melo_websocket_signal_connection (ws, false);

  /* Release websocket */
  melo_websocket_destroy (ws);
}

static void
websocket_cb (SoupServer *server, SoupWebsocketConnection *connection,
    const char *path, SoupClientContext *client, gpointer user_data)
{
  MeloWebsocket *base_ws = user_data;
  MeloWebsocket *ws;

  /* Copy websocket */
  ws = melo_websocket_copy (base_ws);

  /* Set connection */
  melo_websocket_set_connection (ws, connection);

  /* Signal user for connection */
  melo_websocket_signal_connection (ws, true);

  /* Capture incoming messages and connection closed */
  g_signal_connect (connection, "message", G_CALLBACK (websocket_msg_cb), ws);
  g_signal_connect (connection, "closed", G_CALLBACK (websocket_close_cb), ws);
}

/**
 * melo_http_server_add_websocket_handler:
 * @srv: a #MeloHttpServer instance
 * @path: the path which will be handled, can be NULL
 * @origin: the origin which will only be accepted, can be NULL
 * @protocols: the protocol list which will only be accepted, can be
 *                      NULL
 * @conn_cb: the function called when a client connect or disconnect
 * @msg_cb: the function called when a new message is received
 * @user_data: the data to pass to @conn_cb and @msg_cb
 *
 * Adds a new websocket handler.
 *
 * Returns: %true if the handler has been added, %false otherwise.
 */
bool
melo_http_server_add_websocket_handler (MeloHttpServer *srv, const char *path,
    const char *origin, char **protocols, MeloWebsocketConnCb conn_cb,
    MeloWebsocketMsgCb msg_cb, void *user_data)
{
  MeloWebsocket *ws;

  if (!srv)
    return false;

  /* Create new websocket */
  ws = melo_websocket_new (conn_cb, msg_cb, user_data);
  if (!ws)
    return false;

  /* Add web-socket handler */
  soup_server_add_websocket_handler (srv->server, path, origin, protocols,
      websocket_cb, ws, (GDestroyNotify) melo_websocket_destroy);

  return true;
}

/**
 * melo_http_server_remove_handler:
 * @srv: a #MeloHttpServer instance
 * @path: the top-level path for the handler, can be NULL
 *
 * Removes an handler by top-level path.
 */
void
melo_http_server_remove_handler (MeloHttpServer *srv, const char *path)
{
  if (srv)
    soup_server_remove_handler (srv->server, path);
}

/**
 * melo_http_server_start:
 * @srv: a #MeloHttpServer instance
 * @http_port: the HTTP port to listen on, 0 for disabled
 * @https_port: the HTTPS port to listen on, 0 for disabled
 *
 * After this function returns successfully, the HTTP server is listening for
 * new incoming connections on the port(s) specified.
 *
 * Returns: %true if the server has started, %false otherwise.
 */
bool
melo_http_server_start (
    MeloHttpServer *srv, unsigned int http_port, unsigned int https_port)
{
  GError *err = NULL;
  gboolean res;

  /* Nothing to listen */
  if (!http_port && !https_port) {
    MELO_LOGE ("No port specified");
    return false;
  }

  /* Start listening for HTTP */
  if (http_port) {
    res = soup_server_listen_all (srv->server, http_port, 0, &err);
    if (res == FALSE) {
      MELO_LOGE ("failed to start HTTP server on port %u: %s", http_port,
          err->message);
      g_clear_error (&err);
      return false;
    }
  }

  /* Start listening for HTTPS */
  if (https_port) {
    res = soup_server_listen_all (
        srv->server, https_port, SOUP_SERVER_LISTEN_HTTPS, &err);
    if (res == FALSE) {
      MELO_LOGW ("failed to start HTTPS server on port %u: %s", https_port,
          err->message);
      g_clear_error (&err);
    }
  }

  return true;
}

/**
 * melo_http_server_stop:
 * @srv: a #MeloHttpServer instance
 *
 * All current sessions are closed and the HTTP server won't continue to listen
 * for new connections after this function returns.
 */
void
melo_http_server_stop (MeloHttpServer *srv)
{
  soup_server_disconnect (srv->server);
}

/**
 * melo_http_server_connection_send_file:
 * @conn: the #MeloHttpServerConnection
 * @path: the file path to send
 *
 * This function can be called from a #MeloHttpServerCb implementation to serve
 * a file pointed by @path as a response. If the file doesn't exists or an error
 * occurs, the accurate response and error will be generated.
 *
 * The content type and length are automatically set in response headers.
 */
void
melo_http_server_connection_send_file (
    MeloHttpServerConnection *conn, const char *path)
{
  melo_http_server_file_serve (conn->msg, conn->client, path);
}

/**
 * melo_http_server_connection_send_url:
 * @conn: the #MeloHttpServerConnection
 * @url: the URL to send
 *
 * This function can be called from a #MeloHttpServerCb implementation to serve
 * an URL set in @url. The server will handle the request asynchronously so this
 * function returns immediately.
 */
void
melo_http_server_connection_send_url (
    MeloHttpServerConnection *conn, const char *url)
{
  melo_http_server_url_serve (conn->server, conn->msg, conn->session, url);
}

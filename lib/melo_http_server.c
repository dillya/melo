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

#define MELO_HTTP_SERVER_REALM "melo"

struct _MeloHttpServer {
  /* Parent instance */
  GObject parent_instance;

  /* HTTP server */
  SoupServer *server;
  SoupSession *session;

  /* Authentication */
  SoupAuthDomain *auth;
  char *auth_password;
};

typedef struct _MeloHttpServerData {
  MeloHttpServer *server;
  MeloHttpServerCb header_cb;
  MeloHttpServerCb body_cb;
  MeloHttpServerCloseCb close_cb;
  void *user_data;
} MeloHttpServerData;

struct _MeloHttpServerConnection {
  MeloHttpServerData *data;
  char *path;

  SoupServer *server;
  SoupSession *session;
  SoupMessage *msg;
  SoupClientContext *client;

  struct {
    MeloHttpServerChunkCb cb;
    void *user_data;
  } body;

  void *user_data;
};

G_DEFINE_TYPE (MeloHttpServer, melo_http_server, G_TYPE_OBJECT)

static char *melo_http_server_auth_cb (SoupAuthDomain *domain, SoupMessage *msg,
    const char *username, gpointer user_data);

static void
melo_http_server_finalize (GObject *gobject)
{
  MeloHttpServer *self = MELO_HTTP_SERVER (gobject);

  /* Free HTTP server */
  g_object_unref (self->server);

  /* Free HTTP authentication domain */
  g_object_unref (self->auth);

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

  /* Create authentication domain */
  self->auth = soup_auth_domain_digest_new (SOUP_AUTH_DOMAIN_REALM,
      MELO_HTTP_SERVER_REALM, SOUP_AUTH_DOMAIN_ADD_PATH, "",
      SOUP_AUTH_DOMAIN_DIGEST_AUTH_CALLBACK, melo_http_server_auth_cb,
      SOUP_AUTH_DOMAIN_DIGEST_AUTH_DATA, self, NULL);

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

static void
got_chunk_cb (SoupMessage *msg, SoupBuffer *chunk, gpointer user_data)
{
  MeloHttpServerConnection *conn = user_data;

  /* Send body chunk */
  if (conn->body.cb) {
    GBytes *bytes;

    /* Create GBytes */
    bytes = soup_buffer_get_as_bytes (chunk);

    /* Send body chunk */
    conn->body.cb (conn, bytes, conn->body.user_data);
  }
}

static void
got_body_cb (SoupMessage *msg, gpointer user_data)
{
  MeloHttpServerConnection *conn = user_data;

  /* No handler registered */
  if (!conn->data->body_cb)
    return;

  /* Pause message and wait call to melo_http_server_connection_close() */
  soup_server_pause_message (conn->server, conn->msg);

  /* Call registered handler */
  conn->data->body_cb (
      conn->data->server, conn, conn->path, conn->data->user_data);
}

static void
finished_cb (SoupMessage *msg, gpointer user_data)
{
  MeloHttpServerConnection *conn = user_data;

  /* Close callback */
  if (conn->data->close_cb)
    conn->data->close_cb (conn->data->server, conn, conn->data->user_data);

  /* Release connection */
  g_object_unref (conn->msg);
  g_free (conn->path);
  free (conn);
}

static void
melo_http_server_handler (SoupServer *server, SoupMessage *msg,
    const char *path, GHashTable *query, SoupClientContext *client,
    gpointer user_data)
{
  MeloHttpServerData *data = user_data;
  MeloHttpServerConnection *conn;

  /* Create connection */
  conn = g_malloc0 (sizeof (*conn));
  if (!conn) {
    return;
  }

  /* Set connection context */
  conn->data = data;
  conn->server = server;
  conn->session = data->server->session;
  conn->msg = g_object_ref (msg);
  conn->client = client;

  /* Handle request */
  if (data->header_cb) {
    /* Catch 'go-body' event to handle request after body reception */
    g_signal_connect (msg, "got-body", G_CALLBACK (got_body_cb), conn);

    /* Save path */
    conn->path = g_strdup (path);

    /* Call registered handler */
    data->header_cb (data->server, conn, path, data->user_data);
  } else {
    /* Pause message and wait call to melo_http_server_connection_close() */
    soup_server_pause_message (server, msg);

    /* Call registered handler */
    data->body_cb (data->server, conn, path, data->user_data);
  }

  /* Catch 'finished' event to release connection */
  g_signal_connect (msg, "finished", G_CALLBACK (finished_cb), conn);
}

/**
 * melo_http_server_add_handler:
 * @srv: a #MeloHttpServer instance
 * @path: the top-level path for the handler, can be %NULL
 * @header_cb: the function called when a new request is received (after request
 *      headers reception and before request body reception)
 * @body_cb: the function called when a new request is received (after request
 *     body reception and before response body send)
 * @close_cb: the function called when a request has finished and connection is
 *     closing
 * @user_data: the data to pass to @header_cb, @body_cb and @close_cb
 *
 * This function adds an handler to capture a request and generate an HTTP
 * response. At least one of @header_cb and @body_cb must be defined to register
 * @path.
 *
 * The @header_cb can be used to handle the HTTP request just after request
 * header reception. This callback can also be used to start body capture as
 * chunk of data with melo_http_server_connection_capture_body().
 *
 * The @body_cb can be used to handle a generic HTTP request and start
 * generating the response (status, content-length, content-type and body data).
 * The melo_http_server_connection_send functions can be used for this purpose.
 *
 * Finally, the @close_cb can be used to catch finalization of the HTTP request
 * and connection close, to release all resources allocated during request
 * processing.
 *
 * Returns: %true if the handler has been added, %false otherwise.
 */
bool
melo_http_server_add_handler (MeloHttpServer *srv, const char *path,
    MeloHttpServerCb header_cb, MeloHttpServerCb body_cb,
    MeloHttpServerCloseCb close_cb, void *user_data)
{
  MeloHttpServerData *data;

  if (!srv || (!body_cb && !header_cb))
    return false;

  /* Allocate new callback data */
  data = g_malloc (sizeof (*data));
  if (!data)
    return false;

  /* Save callback and user data */
  data->server = srv;
  data->header_cb = header_cb;
  data->body_cb = body_cb;
  data->close_cb = close_cb;
  data->user_data = user_data;

  /* Add file handler */
  if (header_cb)
    soup_server_add_early_handler (
        srv->server, path, melo_http_server_handler, data, g_free);
  else
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
 * MeloHttpServerMethod:
 * @conn: the #MeloHttpServerConnection
 *
 * This function can be used to know the HTTP method of the current request
 * handled by @conn.
 *
 * Returns: the #MeloHttpServerMethod method of the current request.
 */
MeloHttpServerMethod
melo_http_server_connection_get_method (MeloHttpServerConnection *conn)
{
  if (conn) {
    if (conn->msg->method == SOUP_METHOD_GET)
      return MELO_HTTP_SERVER_METHOD_GET;
    else if (conn->msg->method == SOUP_METHOD_HEAD)
      return MELO_HTTP_SERVER_METHOD_HEAD;
    else if (conn->msg->method == SOUP_METHOD_POST)
      return MELO_HTTP_SERVER_METHOD_POST;
    else if (conn->msg->method == SOUP_METHOD_PUT)
      return MELO_HTTP_SERVER_METHOD_PUT;
    else if (conn->msg->method == SOUP_METHOD_DELETE)
      return MELO_HTTP_SERVER_METHOD_DELETE;
    else if (conn->msg->method == SOUP_METHOD_CONNECT)
      return MELO_HTTP_SERVER_METHOD_CONNECT;
    else if (conn->msg->method == SOUP_METHOD_OPTIONS)
      return MELO_HTTP_SERVER_METHOD_OPTIONS;
    else if (conn->msg->method == SOUP_METHOD_TRACE)
      return MELO_HTTP_SERVER_METHOD_TRACE;
  }
  return MELO_HTTP_SERVER_METHOD_UNKNOWN;
}

/**
 * melo_http_server_connection_get_content_length:
 * @conn: the #MeloHttpServerConnection
 *
 * This function can be used to get the request body content length of the
 * current request handled by @conn. If the size returned is negative, it means
 * the content length was not defined.
 *
 * Returns: the content length in bytes, -1 if the content length is unset.
 */
ssize_t
melo_http_server_connection_get_content_length (MeloHttpServerConnection *conn)
{
  if (!conn || soup_message_headers_get_encoding (
                   conn->msg->response_headers) != SOUP_ENCODING_CONTENT_LENGTH)
    return -1;
  return soup_message_headers_get_content_length (conn->msg->response_headers);
}

/**
 * melo_http_server_connection_set_user_data:
 * @conn: the #MeloHttpServerConnection
 * @user_data: the data to attach
 *
 * Attach a data to the connection. Before attaching a new data, be sure to free
 * the previous one.
 */
void
melo_http_server_connection_set_user_data (
    MeloHttpServerConnection *conn, void *user_data)
{
  if (conn)
    conn->user_data = user_data;
}

/**
 * melo_http_server_connection_get_user_data:
 * @conn: the #MeloHttpServerConnection
 *
 * Gets data attached to the connection.
 *
 * Returns: (transfer none): the data to attached to the connection or %NULL.
 */
void *
melo_http_server_connection_get_user_data (MeloHttpServerConnection *conn)
{
  return conn ? conn->user_data : NULL;
}

/**
 * melo_http_server_connection_capture_body:
 * @conn: the #MeloHttpServerConnection
 * @cb: the function called when a new chunk of request body data is available
 * @user_data: the data to pass to @cb
 *
 * This function enable request body capture as chunk of memory: @cb will be
 * called on each chunk of request body received.
 *
 * Returns: %true if the capture has been set successfully, %false otherwise.
 */
bool
melo_http_server_connection_capture_body (
    MeloHttpServerConnection *conn, MeloHttpServerChunkCb cb, void *user_data)
{
  if (!conn || conn->body.cb)
    return false;

  /* Disable body accumulation */
  soup_message_body_set_accumulate (conn->msg->request_body, false);

  /* Register 'got-chunk' callback */
  g_signal_connect (conn->msg, "got-chunk", G_CALLBACK (got_chunk_cb), conn);

  /* Set callback */
  conn->body.cb = cb;
  conn->body.user_data = user_data;

  return true;
}

/**
 * melo_http_server_connection_set_status:
 * @conn: the #MeloHttpServerConnection
 * @code: the HTTP response code
 *
 * This function can be used to set the HTTP status code for the current request
 * handled by @conn.
 *
 * It must be called before any call to melo_http_server_connection_send to
 * take place.
 */
void
melo_http_server_connection_set_status (
    MeloHttpServerConnection *conn, unsigned int code)
{
  if (conn)
    soup_message_set_status (conn->msg, code);
}

/**
 * melo_http_server_connection_set_content_type:
 * @conn: the #MeloHttpServerConnection
 * @mime_type: the content mime type
 *
 * This function can be used to set the response body content mime type of the
 * current request handled by @conn.
 *
 * It must be called before any call to melo_http_server_connection_send to
 * take place.
 */
void
melo_http_server_connection_set_content_type (
    MeloHttpServerConnection *conn, const char *mime_type)
{
  if (conn)
    soup_message_headers_set_content_type (
        conn->msg->response_headers, mime_type, NULL);
}

/**
 * melo_http_server_connection_set_content_length:
 * @conn: the #MeloHttpServerConnection
 * @len: the content length of the body response
 *
 * This function can be used to set the response body content length of the
 * current request handled by @conn.
 *
 * It must be called before any call to melo_http_server_connection_send to
 * take place.
 */
void
melo_http_server_connection_set_content_length (
    MeloHttpServerConnection *conn, size_t len)
{
  if (conn)
    soup_message_headers_set_content_length (conn->msg->response_headers, len);
}

/**
 * melo_http_server_connection_send_chunk:
 * @conn: the #MeloHttpServerConnection
 * @data: (transfer none) (array length=size): the chunk data to append to body
 * @size: the size of @data
 * @free_func: the function to call to release the data
 * @user_data: data to pass to @free_func
 *
 * This function can be called during and after call to the MeloHttpServerCb
 * callback, to append new chunk of data to the response body of the current
 * HTTP request handled by @conn.
 *
 * When no more data chunk has to be appended, the function
 * melo_http_server_connection_close() must be called to complete the body send
 * and finalize the connection.
 */
void
melo_http_server_connection_send_chunk (MeloHttpServerConnection *conn,
    const unsigned char *data, size_t len, GDestroyNotify free_func,
    void *user_data)
{
  SoupBuffer *chunk;

  /* No data to append */
  if (!conn || !data)
    return;

  /* Append chunk to body */
  chunk = soup_buffer_new_with_owner (data, len, user_data, free_func);
  soup_message_body_append_buffer (conn->msg->response_body, chunk);
  soup_server_unpause_message (conn->server, conn->msg);
}

/**
 * melo_http_server_connection_close:
 * @conn: the #MeloHttpServerConnection
 *
 * This function is used to complete the response body of the current HTTP
 * request handled by @conn and finalize the connection, when the function
 * melo_http_server_connection_send_chunk() has been called to feed the response
 * body. This function should not be called in any combination with other
 * melo_http_server_connection_send functions.
 */
void
melo_http_server_connection_close (MeloHttpServerConnection *conn)
{
  /* Complete body and close connection */
  if (conn) {
    soup_message_body_complete (conn->msg->response_body);
    soup_server_unpause_message (conn->server, conn->msg);
  }
}

/**
 * melo_http_server_connection_send:
 * @conn: the #MeloHttpServerConnection
 * @code: the HTTP response code
 * @data: (transfer none) (array length=size): the data to set to body
 * @size: the size of @data
 * @free_func: the function to call to release the data
 * @user_data: data to pass to @free_func
 *
 * This function can be used to set the response body message for the current
 * HTTP request handled by @conn.
 * The response code is set to @code and the content length is set with @len.
 * After this call, the connection is finalized.*
 *
 * This function is not compatible with melo_http_server_connection_close() and
 * melo_http_server_connection_send_chunk().
 */
void
melo_http_server_connection_send (MeloHttpServerConnection *conn,
    unsigned int code, const unsigned char *data, size_t len,
    GDestroyNotify free_func, void *user_data)
{
  if (!conn)
    return;

  /* Set headers */
  soup_message_set_status (conn->msg, code);
  soup_message_headers_set_content_length (conn->msg->response_headers, len);

  /* Set body */
  melo_http_server_connection_send_chunk (
      conn, data, len, free_func, user_data);
  melo_http_server_connection_close (conn);
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
  if (!conn || !path)
    return;

  soup_server_unpause_message (conn->server, conn->msg);
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
  if (!conn || !url)
    return;

  melo_http_server_url_serve (conn->server, conn->msg, conn->session, url);
}

/**
 * melo_http_server_set_auth:
 * @srv: a #MeloHttpServer instance
 * @enable: %true if authentication is enabled, %false otherwise
 * @username: (nullable): the user name to set for authentication
 * @password: (nullable): the password to set for authentication
 *
 * This function can be used to enable / disable the HTTP digest authentication
 * on the HTTP server.
 */
void
melo_http_server_set_auth (MeloHttpServer *srv, bool enable,
    const char *username, const char *password)
{
  if (!srv)
    return;

  g_free (srv->auth_password);

  if (enable) {
    srv->auth_password = soup_auth_domain_digest_encode_password (
        username, MELO_HTTP_SERVER_REALM, password);
    soup_server_add_auth_domain (srv->server, srv->auth);
  } else if (srv->auth_password != NULL) {
    soup_server_remove_auth_domain (srv->server, srv->auth);
    srv->auth_password = NULL;
  }
}

static char *
melo_http_server_auth_cb (SoupAuthDomain *domain, SoupMessage *msg,
    const char *username, gpointer user_data)
{
  MeloHttpServer *srv = user_data;

  return g_strdup (srv->auth_password);
}

/*
 * Copyright (C) 2016-2020 Alexandre Dilly <dillya@sparod.com>
 *
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

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#ifdef G_OS_WIN32
#include <Wincrypt.h>
#endif

#include <gio/gio.h>

#define MELO_LOG_TAG "rtsp_server"
#include "melo/melo_log.h"

#include "melo/melo_rtsp_server.h"

/**
 * SECTION:melo_rtsp_server
 * @title: MeloRtspServer
 * @short_description: Simple RTSP Server implementation
 *
 * The #MeloRtspServer class is intended to create a simple and lightweight RTSP
 * server instance in one or more #MeloModule. It is based on the GLib sockets
 * and main loop context which remove any need of a thread.
 *
 * Only three callbacks (#MeloRtspServerRequestCb, #MeloRtspServerReadCb and
 * #MeloRtspServerCloseCb) are required to handle any RTSP request and the
 * attachment of the RTSP server instance to a #GMainContext though a call to
 * melo_rtsp_server_attach().
 *
 * In addition to standard RTSP request handling, the basic and digest
 * authentication methods are available.
 *
 * Thanks to %MELO_RTSP_METHOD_UNKNOWN and melo_rtsp_get_method_name(), custom
 * RTSP methods can be handled with the #MeloRtspServer server and then
 * implement modified RTSP protocols.
 */

#define MELO_DEFAULT_MAX_USER 5
#define MELO_DEFAULT_BUFFER_SIZE 8192

typedef enum {
  MELO_RTSP_STATE_WAIT_HEADER = 0,
  MELO_RTSP_STATE_WAIT_BODY,
  MELO_RTSP_STATE_SEND_HEADER,
  MELO_RTSP_STATE_SEND_BODY
} MeloRtspSate;

struct _MeloRtspServerConnection {
  /* Parent */
  MeloRtspServer *parent;
  /* Client socket */
  GSocket *sock;
  /* RTSP status */
  MeloRtspSate state;
  /* RTSP variables */
  MeloRtspMethod method;
  const char *method_name;
  const char *url;
  GHashTable *headers;
  unsigned int seq;
  size_t content_length;
  size_t body_size;
  /* Server address */
  unsigned char server_ip[4];
  unsigned int server_port;
  /* Client address */
  char *hostname;
  char *ip_string;
  unsigned char ip[4];
  unsigned int port;
  /* Input buffer */
  char *buffer;
  size_t buffer_size;
  size_t buffer_len;
  /* Output buffer */
  char *out_buffer;
  size_t out_buffer_size;
  size_t out_buffer_len;
  /* Packet buffer (response) */
  unsigned char *packet;
  size_t packet_len;
  GDestroyNotify packet_free;
  /* User data */
  void *user_data;
  /* Digest auth */
  char *nonce;
};

struct _MeloRtspServer {
  /* Parent instance */
  GObject parent_instance;

  /* Server socket */
  GSocket *sock;
  GMainContext *context;
  /* Clients */
  GMutex mutex;
  int users;
  int max_user;
  GList *connections;
  /* Request handler */
  MeloRtspServerRequestCb request_cb;
  void *request_data;
  /* Read handler */
  MeloRtspServerReadCb read_cb;
  void *read_data;
  /* Close handler */
  MeloRtspServerCloseCb close_cb;
  void *close_data;
};

G_DEFINE_TYPE (MeloRtspServer, melo_rtsp_server, G_TYPE_OBJECT)

static void
melo_rtsp_server_finalize (GObject *gobject)
{
  MeloRtspServer *srv = MELO_RTSP_SERVER (gobject);

  /* Stop RTSP server */
  melo_rtsp_server_stop (srv);

  /* Clear mutex */
  g_mutex_clear (&srv->mutex);

  /* Chain up to the parent class */
  G_OBJECT_CLASS (melo_rtsp_server_parent_class)->finalize (gobject);
}

static void
melo_rtsp_server_class_init (MeloRtspServerClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  /* Add custom finalize() function */
  oclass->finalize = melo_rtsp_server_finalize;
}

static void
melo_rtsp_server_init (MeloRtspServer *self)
{
  /* Set default max user count */
  self->max_user = MELO_DEFAULT_MAX_USER;

  /* Init mutex */
  g_mutex_init (&self->mutex);
}

/**
 * melo_rtsp_server_new:
 *
 * Instantiate a new #MeloRtspServer object.
 *
 * Returns: (transfer full): the new #MeloRtspServer instance or %NULL if
 * failed.
 */
MeloRtspServer *
melo_rtsp_server_new (void)
{
  return g_object_new (MELO_TYPE_RTSP_SERVER, NULL);
}

/**
 * melo_rtsp_server_start:
 * @srv: a RTSP server handle
 * @port: the port number on which to listen
 *
 * Start a new RTSP server instance, listening on port number @port. If the port
 * is already in use, the function will return %false.
 *
 * Note: by default, the connection is kept alive.
 *
 * Returns: %true if RTSP server has been started successfully, %false
 * otherwise.
 */
bool
melo_rtsp_server_start (MeloRtspServer *srv, unsigned int port)
{
  GInetAddress *inet_addr;
  GSocketAddress *addr;
  bool ret;
  GError *err;

  /* Server is already started */
  if (srv->sock)
    return false;

  /* Open socket */
  srv->sock = g_socket_new (G_SOCKET_FAMILY_IPV4, G_SOCKET_TYPE_STREAM,
      G_SOCKET_PROTOCOL_DEFAULT, &err);
  if (!srv->sock) {
    MELO_LOGE ("failed to create socket: %s", err->message);
    g_clear_error (&err);
    return false;
  }

  /* Create a new address for bind */
  inet_addr = g_inet_address_new_any (G_SOCKET_FAMILY_IPV4);
  addr = g_inet_socket_address_new (inet_addr, port);
  g_object_unref (inet_addr);
  if (!addr) {
    MELO_LOGE ("failed to create socket address");
    goto failed;
  }

  /* Bind */
  ret = g_socket_bind (srv->sock, addr, TRUE, &err);
  g_object_unref (addr);
  if (!ret) {
    MELO_LOGE ("failed to bind socket: %s", err->message);
    g_clear_error (&err);
    goto failed;
  }

  /* Keep connection alive */
  g_socket_set_keepalive (srv->sock, TRUE);

  /* Setup non-blocking */
  g_socket_set_blocking (srv->sock, FALSE);

  /* Listen */
  if (!g_socket_listen (srv->sock, &err)) {
    MELO_LOGE ("failed to start listening: %s", err->message);
    g_clear_error (&err);
    goto failed;
  }

  return true;
failed:
  g_object_unref (srv->sock);
  srv->sock = NULL;
  return false;
}

/**
 * melo_rtsp_server_stop:
 * @srv: a RTSP server handle
 *
 * Stop the RTSP server handled by @srv. All connections are aborted and closed
 * before it returns.
 */
void
melo_rtsp_server_stop (MeloRtspServer *srv)
{
  GError *err;

  /* No server running */
  if (!srv->sock)
    return;

  /* Close server socker */
  if (!g_socket_close (srv->sock, &err)) {
    MELO_LOGE ("failed to close socket: %s", err->message);
    g_clear_error (&err);
    return;
  }

  /* Free socket */
  g_object_unref (srv->sock);
  srv->sock = NULL;

  /* Remove context */
  srv->context = NULL;
}

/**
 * melo_rtsp_server_set_request_callback:
 * @srv: a RTSP server handle
 * @callback: a #MeloRtspServerRequestCb callback
 * @user_data: a pointer to associate with the callback
 *
 * Set a #MeloRtspServerRequestCb callback which is called when a new RTSP
 * request is received by the server.
 */
void
melo_rtsp_server_set_request_callback (
    MeloRtspServer *srv, MeloRtspServerRequestCb cb, void *user_data)
{
  srv->request_cb = cb;
  srv->request_data = user_data;
}

/**
 * melo_rtsp_server_set_read_callback:
 * @srv: a RTSP server handle
 * @callback: a #MeloRtspServerReadCb callback
 * @user_data: a pointer to associate with the callback
 *
 * Set a #MeloRtspServerReadCb callback which is called when data are received
 * from a RTSP connection in the body request.
 */
void
melo_rtsp_server_set_read_callback (
    MeloRtspServer *srv, MeloRtspServerReadCb cb, void *user_data)
{
  srv->read_cb = cb;
  srv->read_data = user_data;
}

/**
 * melo_rtsp_server_set_close_callback:
 * @srv: a RTSP server handle
 * @callback: a #MeloRtspServerCloseCb callback
 * @user_data: a pointer to associate with the callback
 *
 * Set a #MeloRtspServerCloseCb callback which is called at request completion
 * or end a connection.
 */
void
melo_rtsp_server_set_close_callback (
    MeloRtspServer *srv, MeloRtspServerCloseCb cb, void *user_data)
{
  srv->close_cb = cb;
  srv->close_data = user_data;
}

static void
connection_close (MeloRtspServerConnection *conn)
{
  MeloRtspServer *srv = conn->parent;

  /* Close connection socket */
  if (conn->sock) {
    g_socket_close (conn->sock, NULL);
    g_object_unref (conn->sock);
  }

  /* Callback before closing connection socket */
  if (srv->close_cb)
    srv->close_cb (conn, srv->close_data, &conn->user_data);

  /* Lock connection list */
  g_mutex_lock (&srv->mutex);

  /* Remove connection from list */
  srv->connections = g_list_remove (srv->connections, conn);
  srv->users--;

  /* Unlock connection list */
  g_mutex_unlock (&srv->mutex);

  /* Free hash table for headers */
  g_hash_table_unref (conn->headers);

  /* Free buffers */
  g_slice_free1 (conn->out_buffer_size, conn->out_buffer);
  g_slice_free1 (conn->buffer_size, conn->buffer);
  if (conn->packet && conn->packet_free)
    conn->packet_free (conn->packet);

  /* Free nonce */
  g_free (conn->nonce);

  /* Free connection hostname */
  g_free (conn->hostname);

  /* Free connection IP */
  g_free (conn->ip_string);

  /* Free connection */
  g_slice_free (MeloRtspServerConnection, conn);
  g_object_unref (srv);
}

static inline bool
extract_string (const char **dest, char **src, size_t *len, const char *needle,
    size_t needle_len)
{
  char *end;

  /* Find end of string */
  end = g_strstr_len (*src, *len, needle);
  if (!end)
    return false;

  /* Set string */
  *len -= end - *src + needle_len;
  if (dest)
    *dest = *src;
  *src = end + needle_len;
  *end = '\0';

  return true;
}

static bool
parse_request (MeloRtspServerConnection *conn, size_t len)
{
  char *buf = conn->buffer;

  /* Get method name */
  if (!extract_string (&conn->method_name, &buf, &len, " ", 1))
    return false;

  /* Find method */
  if (!g_strcmp0 (conn->method_name, "OPTIONS"))
    conn->method = MELO_RTSP_METHOD_OPTIONS;
  else if (!g_strcmp0 (conn->method_name, "DESCRIBE"))
    conn->method = MELO_RTSP_METHOD_DESCRIBE;
  else if (!g_strcmp0 (conn->method_name, "ANNOUNCE"))
    conn->method = MELO_RTSP_METHOD_ANNOUNCE;
  else if (!g_strcmp0 (conn->method_name, "SETUP"))
    conn->method = MELO_RTSP_METHOD_SETUP;
  else if (!g_strcmp0 (conn->method_name, "PLAY"))
    conn->method = MELO_RTSP_METHOD_PLAY;
  else if (!g_strcmp0 (conn->method_name, "PAUSE"))
    conn->method = MELO_RTSP_METHOD_PAUSE;
  else if (!g_strcmp0 (conn->method_name, "TEARDOWN"))
    conn->method = MELO_RTSP_METHOD_TEARDOWN;
  else if (!g_strcmp0 (conn->method_name, "GET_PARAMETER"))
    conn->method = MELO_RTSP_METHOD_GET_PARAMETER;
  else if (!g_strcmp0 (conn->method_name, "SET_PARAMETER"))
    conn->method = MELO_RTSP_METHOD_SET_PARAMETER;
  else if (!g_strcmp0 (conn->method_name, "RECORD"))
    conn->method = MELO_RTSP_METHOD_RECORD;
  else
    conn->method = MELO_RTSP_METHOD_UNKNOWN;

  /* Get URL */
  if (!extract_string (&conn->url, &buf, &len, " ", 1))
    return false;

  /* Go to first header line (skip RTSP version) */
  if (!extract_string (NULL, &buf, &len, "\r\n", 2))
    return false;

  /* Parse all headers now */
  while (len > 2) {
    const char *name, *value;

    /* Get name */
    if (!extract_string (&name, &buf, &len, ": ", 2))
      return false;

    /* Get value */
    if (!extract_string (&value, &buf, &len, "\r\n", 2))
      return false;

    /* Add name / value pair to hash table */
    g_hash_table_insert (conn->headers, (char *) name, (char *) value);
  }

  return true;
}

static bool
handle_connection (
    GSocket *sock, GIOCondition condition, MeloRtspServerConnection *conn)
{
  MeloRtspServer *srv = conn->parent;
  GSource *source;
  char *buf;
  size_t len;

  /* Read from socket */
  if (condition & G_IO_IN) {
    /* Read data from socket */
    len = g_socket_receive (sock, conn->buffer + conn->buffer_len,
        conn->buffer_size - conn->buffer_len, NULL, NULL);
    if (len <= 0)
      goto close;
    conn->buffer_len += len;
  }

  /* Parse buffer */
  switch (conn->state) {
  case MELO_RTSP_STATE_WAIT_HEADER:
    /* Find end of header */
    buf = g_strstr_len (conn->buffer, conn->buffer_len, "\r\n\r\n");

    /* Not enough data to parse */
    if (!buf)
      break;

    /* Parse request */
    len = buf - conn->buffer + 4;
    if (!parse_request (conn, len - 2))
      goto failed;

    /* Get content length */
    buf = g_hash_table_lookup (conn->headers, "Content-Length");
    if (buf)
      conn->content_length = strtoul (buf, NULL, 10);
    conn->body_size = conn->content_length;

    /* Call request callback */
    if (srv->request_cb)
      srv->request_cb (
          conn, conn->method, conn->url, srv->request_data, &conn->user_data);

    /* Reset request details (only available during request_cb call) */
    g_hash_table_remove_all (conn->headers);
    conn->method_name = NULL;
    conn->url = NULL;

    /* Move body data to buffer start */
    memmove (conn->buffer, conn->buffer + len, conn->buffer_len - len);
    conn->buffer_len -= len;

    /* Go to next state: wait body */
    conn->state = MELO_RTSP_STATE_WAIT_BODY;

    /* Not enough data yet */
    if (conn->content_length > conn->buffer_len)
      break;

  case MELO_RTSP_STATE_WAIT_BODY:
    if (conn->content_length) {
      /* Get body data */
      if (conn->content_length <= conn->buffer_len) {
        /* Last chunk */
        if (srv->read_cb)
          srv->read_cb (conn, (unsigned char *) conn->buffer,
              conn->content_length, true, srv->read_data, &conn->user_data);

        /* Move remaining data to buffer start */
        memmove (conn->buffer, conn->buffer + conn->content_length,
            conn->buffer_len - conn->content_length);
        conn->buffer_len -= conn->content_length;
        conn->content_length = 0;
      } else if (conn->buffer_len == conn->buffer_size) {
        /* Next chunk */
        if (srv->read_cb)
          srv->read_cb (conn, (unsigned char *) conn->buffer, conn->buffer_size,
              false, srv->read_data, &conn->user_data);

        /* Reset buffer */
        conn->content_length -= conn->buffer_size;
        conn->buffer_len = 0;
        break;
      } else
        break;
    }

    /* No response */
    if (conn->out_buffer_len == 0)
      melo_rtsp_server_connection_init_response (conn, 404, "Not found");

    /* Go to next state: send reply header */
    conn->state = MELO_RTSP_STATE_SEND_HEADER;

    /* Add source to wait socket becomes writable */
    source = g_socket_create_source (sock, G_IO_OUT, NULL);
    g_source_set_callback (source, (GSourceFunc) handle_connection, conn, NULL);
    g_source_attach (source, srv->context);
    return G_SOURCE_REMOVE;

  case MELO_RTSP_STATE_SEND_HEADER:
    if (conn->out_buffer_len > 0) {
      len = g_socket_send (
          sock, conn->out_buffer, conn->out_buffer_len, NULL, NULL);
      if (len <= 0)
        goto close;
      conn->out_buffer_len -= len;
      if (conn->out_buffer_len > 0)
        break;
    }

    /* Go to next state: send reply body */
    conn->state = MELO_RTSP_STATE_SEND_BODY;
    break;
  case MELO_RTSP_STATE_SEND_BODY:
    if (conn->packet) {
      if (conn->packet_len > 0) {
        len = g_socket_send (
            sock, (char *) conn->packet, conn->packet_len, NULL, NULL);
        if (len <= 0)
          goto close;
        conn->packet_len -= len;
        if (conn->packet_len > 0)
          break;
      }

      /* Free packet */
      if (conn->packet_free)
        conn->packet_free (conn->packet);
      conn->packet_free = NULL;
      conn->packet_len = 0;
      conn->packet = NULL;
    }

    /* Go to next state: wait for next request */
    conn->state = MELO_RTSP_STATE_WAIT_HEADER;

    /* Add source to wait next incoming packet */
    source = g_socket_create_source (sock, G_IO_IN | G_IO_PRI, NULL);
    g_source_set_callback (source, (GSourceFunc) handle_connection, conn, NULL);
    g_source_attach (source, srv->context);
    g_source_unref (source);
    return G_SOURCE_REMOVE;
  }

  return G_SOURCE_CONTINUE;

failed:
  g_socket_send (sock, "RTSP/1.0 400 Bad request\r\n\r\n", 28, NULL, NULL);
close:
  connection_close (conn);
  return G_SOURCE_REMOVE;
}

static bool
melo_rtsp_get_address (GSocketAddress *addr, unsigned char *ip,
    char **ip_string, unsigned int *port, char **name)
{
  GInetSocketAddress *inet_sock_addr;
  GInetAddress *inet_addr;
  GResolver *resolver;
  size_t len;

  /* Not an INET Address */
  if (!addr || !G_IS_INET_SOCKET_ADDRESS (addr)) {
    MELO_LOGE ("invalid address");
    return false;
  }

  /* Get inet socket address */
  inet_sock_addr = G_INET_SOCKET_ADDRESS (addr);

  /* Get IP */
  inet_addr = g_inet_socket_address_get_address (inet_sock_addr);
  len = g_inet_address_get_native_size (inet_addr);
  memcpy (ip, g_inet_address_to_bytes (inet_addr), len > 4 ? 4 : len);

  /* Get IP string */
  if (ip_string)
    *ip_string = g_inet_address_to_string (inet_addr);

  /* Get port */
  *port = g_inet_socket_address_get_port (inet_sock_addr);

  /* Get hostname */
  if (name) {
    resolver = g_resolver_get_default ();
    *name = g_resolver_lookup_by_address (resolver, inet_addr, NULL, NULL);
    g_object_unref (resolver);
  }

  return true;
}

static bool
melo_rtsp_accept (
    GSocket *server_sock, GIOCondition condition, MeloRtspServer *srv)
{
  MeloRtspServerConnection *conn;
  GSocketAddress *addr;
  GSource *source;
  GSocket *sock;

  /* An error occurred: stop server */
  if (condition != G_IO_IN)
    return G_SOURCE_REMOVE;

  /* Accept connection */
  sock = g_socket_accept (srv->sock, NULL, NULL);
  if (!sock) {
    MELO_LOGW ("failed to accept new connection");
    goto exit;
  }

  /* Check users count */
  if (srv->users >= srv->max_user) {
    g_socket_send (
        sock, "RTSP/1.0 503 Server too busy\r\n\r\n", 32, NULL, NULL);
    goto failed;
  }

  /* Setup non-blocking */
  g_socket_set_blocking (sock, FALSE);

  /* Create a new connection */
  conn = g_slice_new0 (MeloRtspServerConnection);
  if (!conn)
    goto failed;

  /* Fill connection */
  conn->sock = sock;
  conn->parent = g_object_ref (srv);
  conn->state = MELO_RTSP_STATE_WAIT_HEADER;

  /* Get server details */
  addr = g_socket_get_local_address (sock, NULL);
  melo_rtsp_get_address (addr, conn->server_ip, NULL, &conn->server_port, NULL);
  g_object_unref (addr);

  /* Get connection details */
  addr = g_socket_get_remote_address (sock, NULL);
  melo_rtsp_get_address (
      addr, conn->ip, &conn->ip_string, &conn->port, &conn->hostname);
  g_object_unref (addr);

  /* Allocate buffer */
  conn->buffer_len = 0;
  conn->buffer_size = MELO_DEFAULT_BUFFER_SIZE;
  conn->buffer = g_slice_alloc (conn->buffer_size);

  /* Allocate output buffer */
  conn->out_buffer_len = 0;
  conn->out_buffer_size = MELO_DEFAULT_BUFFER_SIZE;
  conn->out_buffer = g_slice_alloc (conn->out_buffer_size);

  /* Create hash table for headers */
  conn->headers = g_hash_table_new (g_str_hash, g_str_equal);

  /* Lock connection list */
  g_mutex_lock (&srv->mutex);

  /* Add connection to list */
  srv->connections = g_list_prepend (srv->connections, conn);
  srv->users++;

  /* Unlock connection list */
  g_mutex_unlock (&srv->mutex);

  /* Create and attach source to wait next incoming packet */
  source = g_socket_create_source (sock, G_IO_IN | G_IO_PRI, NULL);
  g_source_set_callback (source, (GSourceFunc) handle_connection, conn, NULL);
  g_source_attach (source, srv->context);
  g_source_unref (source);

exit:
  return G_SOURCE_CONTINUE;

failed:
  g_socket_close (sock, NULL);
  g_object_unref (sock);
  return G_SOURCE_CONTINUE;
}

/**
 * melo_rtsp_server_attach:
 * @srv: a RTSP server handle
 * @context: a #GMainContext on which to attach the RTSP server
 *
 * Attach the RTSP server instance to a #GMainContext.
 *
 * Returns: a source ID which can be used to detach the server.
 */
guint
melo_rtsp_server_attach (MeloRtspServer *srv, GMainContext *context)
{
  GSource *source;
  guint id;

  /* Already attached */
  if (srv->context)
    return 0;
  srv->context = context;

  /* Get source from server socket */
  source = g_socket_create_source (
      srv->sock, G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_NVAL, NULL);
  if (!source)
    return 0;

  /* Set server callback */
  g_source_set_callback (source, (GSourceFunc) melo_rtsp_accept, srv,
      (GDestroyNotify) melo_rtsp_server_stop);

  /* Attach server socket */
  id = g_source_attach (source, context);
  g_source_unref (source);

  return id;
}

/**
 * melo_rtsp_get_method:
 * @conn: a RTSP connection handle
 *
 * Get method of the current RTSP request.
 *
 * Returns: a #MeloRtspMethod containing the current request method.
 */
MeloRtspMethod
melo_rtsp_server_connection_get_method (MeloRtspServerConnection *conn)
{
  g_return_val_if_fail (conn, MELO_RTSP_METHOD_UNKNOWN);
  return conn->method;
}

/**
 * melo_rtsp_server_connection_get_method_name:
 * @conn: a RTSP connection handle
 *
 * Get method name of the current RTSP request.
 *
 * Returns: a string containing the current request method name.
 */
const char *
melo_rtsp_server_connection_get_method_name (MeloRtspServerConnection *conn)
{
  g_return_val_if_fail (conn, NULL);
  return conn->method_name;
}

/**
 * melo_rtsp_server_connection_get_header:
 * @conn: a RTSP connection handle
 * @name: a string containing the name of the header value to get
 *
 * Get value of the header named @name for the current RTSP request.
 *
 * Returns: a string containing the value of the header requested or %NULL if
 * not found.
 */
const char *
melo_rtsp_server_connection_get_header (
    MeloRtspServerConnection *conn, const char *name)
{
  return g_hash_table_lookup (conn->headers, name);
}

/**
 * melo_rtsp_server_connection_get_content_length:
 * @conn: a RTSP connection handle
 *
 * Get content length of the current RTSP request.
 *
 * Returns: the content length of the RTSP request.
 */
size_t
melo_rtsp_server_connection_get_content_length (MeloRtspServerConnection *conn)
{
  return conn->body_size;
}

/**
 * melo_rtsp_server_connection_get_ip:
 * @conn: a RTSP connection handle
 *
 * Get the IPv4 address of the connection. It provides the address as a 4 bytes
 * array.
 *
 * Returns: a 4-bytes array containing the IPv4 address of the connection.
 */
const unsigned char *
melo_rtsp_server_connection_get_ip (MeloRtspServerConnection *conn)
{
  g_return_val_if_fail (conn, NULL);
  return conn->ip;
}

/**
 * melo_rtsp_server_connection_get_ip_string:
 * @conn: a RTSP connection handle
 *
 * Get the IPv4 address of the connection. It provides the address as a string
 * in standard format like "127.0.0.1".
 *
 * Returns: a string containing the IPv4 address of the connection.
 */
const char *
melo_rtsp_server_connection_get_ip_string (MeloRtspServerConnection *conn)
{
  g_return_val_if_fail (conn, NULL);
  return conn->ip_string;
}

/**
 * melo_rtsp_server_connection_get_port:
 * @conn: a RTSP connection handle
 *
 * Get the port used by the current connection.
 *
 * Returns: the port number used by the current connection.
 */
unsigned int
melo_rtsp_server_connection_get_port (MeloRtspServerConnection *conn)
{
  g_return_val_if_fail (conn, 0);
  return conn->port;
}

/**
 * melo_rtsp_server_connection_get_hostname:
 * @conn: a RTSP connection handle
 *
 * Get the host name of the current connection.
 *
 * Returns: a string containing the host name of the current connection.
 */
const char *
melo_rtsp_server_connection_get_hostname (MeloRtspServerConnection *conn)
{
  g_return_val_if_fail (conn, NULL);
  return conn->hostname;
}

/**
 * melo_rtsp_server_connection_get_server_ip:
 * @conn: a RTSP connection handle
 *
 * Get the IPv4 address of the server. It provides the address as a 4 bytes
 * array.
 *
 * Returns: a 4-bytes array containing the IPv4 address of the server.
 */
const unsigned char *
melo_rtsp_server_connection_get_server_ip (MeloRtspServerConnection *conn)
{
  g_return_val_if_fail (conn, NULL);
  return conn->server_ip;
}

/**
 * melo_rtsp_server_connection_get_server_port:
 * @conn: a RTSP connection handle
 *
 * Get the port used by the server.
 *
 * Returns: the port number used by the server.
 */
unsigned int
melo_rtsp_server_connection_get_server_port (MeloRtspServerConnection *conn)
{
  g_return_val_if_fail (conn, 0);
  return conn->server_port;
}

/**
 * melo_rtsp_server_connection_init_response:
 * @conn: a RTSP connection handle
 * @code: the RTSP response code
 * @reason: a string containing the RTSP reason
 *
 * Initialize the response for the current RTSP request. It basically adds the
 * "RTSP/1.0 CODE REASON" line to the response buffer, where CODE is @code and
 * REASON is @reason.
 *
 * Returns: %true if the response has been initialized successfully, %false
 * otherwise.
 */
bool
melo_rtsp_server_connection_init_response (
    MeloRtspServerConnection *conn, unsigned int code, const char *reason)
{
  g_return_val_if_fail (conn, false);

  /* Reset output buffer */
  conn->out_buffer_len = 0;

  /* Create first line of response */
  conn->out_buffer_len += g_snprintf (conn->out_buffer, conn->out_buffer_size,
      "RTSP/1.0 %d %s\r\n\r\n", code, reason);

  return true;
}

/**
 * melo_rtsp_server_connection_add_header:
 * @conn: a RTSP connection handle
 * @name: the name of the header field
 * @value: the value of the header field
 *
 * Add a new header field to the current response. The
 * melo_rtsp_server_connection_init_response() function should be called once
 * before any call to this function. It basically adds the "NAME: VALUE" line to
 * the response buffer, where NAME is @name and VALUE is @value.
 *
 * Returns: %true if the header field has been added successfully to the
 * response, %false otherwise.
 */
bool
melo_rtsp_server_connection_add_header (
    MeloRtspServerConnection *conn, const char *name, const char *value)
{
  g_return_val_if_fail (conn, false);
  g_return_val_if_fail (conn->out_buffer_len != 0, false);

  /* Add header to response */
  conn->out_buffer_len +=
      g_snprintf (conn->out_buffer + conn->out_buffer_len - 2,
          conn->out_buffer_size - conn->out_buffer_len + 2, "%s: %s\r\n\r\n",
          name, value) -
      2;

  return true;
}

/**
 * melo_rtsp_server_connection_set_response:
 * @conn: a RTSP connection handle
 * @response: a string containing the full formatted RTSP response
 *
 * Set a complete formatted RTSP response into response buffer. It should not
 * be used in combination with melo_rtsp_server_connection_init_response() and
 * melo_rtsp_server_connection_add_header().
 *
 * Returns: %true if the response has been set successfully, %false otherwise.
 */
bool
melo_rtsp_server_connection_set_response (
    MeloRtspServerConnection *conn, const char *response)
{
  size_t len;

  g_return_val_if_fail (conn, false);

  /* Get response length */
  len = strlen (response);
  if (len > conn->out_buffer_size)
    return false;

  /* Copy response */
  memcpy (conn->out_buffer, response, len);

  return true;
}

/**
 * melo_rtsp_server_connection_set_packet:
 * @conn: a RTSP connection handle
 * @buffer: a buffer containing the packet to send to the connection
 * @len: size in bytes of the packet to send
 * @free: a callback used to free the buffer at end of transmission, or %NULL
 *
 * Set the packet to append to the RTSP response. The data can be send
 * asynchronously and for allocated packet, a #GDestroyNotify callback is
 * required in order to free the buffer at end of transmission.
 *
 * Returns: %true if the packet has been appended successfully, %false
 * otherwise.
 */
bool
melo_rtsp_server_connection_set_packet (MeloRtspServerConnection *conn,
    unsigned char *buffer, size_t len, GDestroyNotify free)
{
  char str[10];

  g_return_val_if_fail (conn, false);

  conn->packet = buffer;
  conn->packet_len = len;
  conn->packet_free = free;

  /* Add content length */
  g_snprintf (str, sizeof (str), "%lu", (unsigned long) len);
  melo_rtsp_server_connection_add_header (conn, "Content-Length", str);

  return true;
}

/**
 * melo_rtsp_server_connection_close:
 * @conn: a RTSP connection handle
 *
 * This function can be used to close immediately a connection. The close
 * callback will be called just after this call.
 *
 * Returns: %true if the connection has been closed successfully, %false
 * otherwise.
 */
bool
melo_rtsp_server_connection_close (MeloRtspServerConnection *conn)
{
  return g_socket_close (conn->sock, NULL);
}

/**
 * melo_rtsp_server_connection_basic_auth_check:
 * @conn: a RTSP connection handle
 * @username:  the required user name, or %NULL
 * @password: the required password, or %NULL
 *
 * Check if the request embed all basic authentication information and check
 * that user name and password are correct.
 *
 * Returns: %true if authentication information are available in the request and
 * correspond to the provided user name / password, %false otherwise.
 */
bool
melo_rtsp_server_connection_basic_auth_check (
    MeloRtspServerConnection *conn, const char *username, const char *password)
{
  bool ret = false;
  const char *auth;
  char *uname;
  char *pass;
  size_t len;

  g_return_val_if_fail (conn, false);

  /* Get authorization header */
  auth = melo_rtsp_server_connection_get_header (conn, "Authorization");
  if (!auth)
    return false;

  /* Check if basic authentication */
  if (!g_str_has_prefix (auth, "Basic "))
    return false;

  /* Decode string */
  uname = (char *) g_base64_decode (auth + 6, &len);

  /* Find password */
  pass = strchr (uname, ':');
  if (pass) {
    *pass++ = '\0';
    /* Check username and password */
    ret =
        (username && g_strcmp0 (uname, username)) || g_strcmp0 (pass, password);
  }

  /* Free decoded string */
  g_free (uname);

  return ret;
}

/**
 * melo_rtsp_server_connection_basic_auth_response:
 * @conn: a RTSP connection handle
 * @realm: the realm to use for basic authentication
 *
 * Set response with a basic authentication error message.
 *
 * Returns: %true if the response has been set successfully, %false otherwise.
 */
bool
melo_rtsp_server_connection_basic_auth_response (
    MeloRtspServerConnection *conn, const char *realm)
{
  char buffer[256];

  g_return_val_if_fail (conn, false);

  /* Generate response */
  melo_rtsp_server_connection_init_response (conn, 401, "Unauthorized");
  g_snprintf (buffer, 255, "Basic realm=\"%s\"", realm);
  melo_rtsp_server_connection_add_header (conn, "WWW-Authenticate", buffer);

  return true;
}

static char *
melo_rtsp_server_connection_digest_get_sub_value (
    const char *str, const char *name)
{
  char *str_end;
  char *value, *end;
  size_t len;

  /* Get length */
  str_end = (char *) str + strlen (str);
  len = strlen (name);

  /* Find name */
  value = (char *) str;
  do {
    value = strstr (value, name);
    if (!value || value + 2 >= str_end)
      return NULL;
  } while (value[len] != '=' || value[len + 1] != '\"');
  value += len + 2;

  /* Find next '"' */
  end = strchr (value, '\"');
  if (!end)
    return NULL;

  /* Copy string */
  return g_strndup (value, end - value);
}

/**
 * melo_rtsp_server_connection_digest_auth_check:
 * @conn: a RTSP connection handle
 * @username:  the required user name, or %NULL
 * @password: the required password, or %NULL
 * @realm: the required realm, or %NULL
 *
 * Check if the request embed all digest authentication information and check
 * that user name, password and realm are correct.
 *
 * Returns: %true if authentication information are available in the request and
 * correspond to the provided user name / password / realm, %false otherwise.
 */
bool
melo_rtsp_server_connection_digest_auth_check (MeloRtspServerConnection *conn,
    const char *username, const char *password, const char *realm)
{
  GChecksum *checksum;
  char *ha1, *ha2, *response;
  char *ha1_up, *ha2_up, *response_up;
  char *uname = NULL;
  char *resp = NULL;
  const char *auth;
  bool ret = false;

  /* Can't check without a nonce */
  if (!conn->nonce)
    return false;

  /* Get auth header */
  auth = melo_rtsp_server_connection_get_header (conn, "Authorization");
  if (!auth || strncmp (auth, "Digest ", 7))
    return false;

  /* Use provided username if NULL */
  if (!username) {
    uname = melo_rtsp_server_connection_digest_get_sub_value (auth, "username");
    if (!uname)
      return false;
    username = uname;
  }

  /* Calculate HA1 */
  checksum = g_checksum_new (G_CHECKSUM_MD5);
  g_checksum_update (checksum, (guchar *) username, strlen (username));
  g_checksum_update (checksum, (guchar *) ":", 1);
  g_checksum_update (checksum, (guchar *) realm, strlen (realm));
  g_checksum_update (checksum, (guchar *) ":", 1);
  g_checksum_update (checksum, (guchar *) password, strlen (password));
  ha1 = g_strdup (g_checksum_get_string (checksum));
  ha1_up = g_ascii_strup (g_checksum_get_string (checksum), -1);
  g_checksum_free (checksum);
  g_free (uname);

  /* Calculate HA2 */
  checksum = g_checksum_new (G_CHECKSUM_MD5);
  g_checksum_update (
      checksum, (guchar *) conn->method_name, strlen (conn->method_name));
  g_checksum_update (checksum, (guchar *) ":", 1);
  g_checksum_update (checksum, (guchar *) conn->url, strlen (conn->url));
  ha2 = g_strdup (g_checksum_get_string (checksum));
  ha2_up = g_ascii_strup (g_checksum_get_string (checksum), -1);
  g_checksum_free (checksum);

  /* Calculate response */
  checksum = g_checksum_new (G_CHECKSUM_MD5);
  g_checksum_update (checksum, (guchar *) ha1, strlen (ha1));
  g_checksum_update (checksum, (guchar *) ":", 1);
  g_checksum_update (checksum, (guchar *) conn->nonce, strlen (conn->nonce));
  g_checksum_update (checksum, (guchar *) ":", 1);
  g_checksum_update (checksum, (guchar *) ha2, strlen (ha2));
  response = g_strdup (g_checksum_get_string (checksum));
  g_checksum_free (checksum);
  g_free (ha1);
  g_free (ha2);

  /* Calculate response uppercase */
  checksum = g_checksum_new (G_CHECKSUM_MD5);
  g_checksum_update (checksum, (guchar *) ha1_up, strlen (ha1_up));
  g_checksum_update (checksum, (guchar *) ":", 1);
  g_checksum_update (checksum, (guchar *) conn->nonce, strlen (conn->nonce));
  g_checksum_update (checksum, (guchar *) ":", 1);
  g_checksum_update (checksum, (guchar *) ha2_up, strlen (ha2_up));
  response_up = g_ascii_strup (g_checksum_get_string (checksum), -1);
  g_checksum_free (checksum);
  g_free (ha1_up);
  g_free (ha2_up);

  /* Check response */
  resp = melo_rtsp_server_connection_digest_get_sub_value (auth, "response");
  if (resp && (!g_strcmp0 (resp, response) || !g_strcmp0 (resp, response_up)))
    ret = true;
  g_free (response);
  g_free (response_up);
  g_free (resp);

  return ret;
}

static void
generate_nonce (char *buffer, size_t len)
{
  size_t i = 0;
#ifdef G_OS_WIN32
  HCRYPTPROV crypt;

  /* Acquire random context */
  if (CryptAcquireContext (&crypt, "melo", NULL, PROV_RSA_FULL, 0)) {
    /* Generate random bytes sequence */
    if (CryptGenRandom (crypt, len, buffer)) {
      CryptReleaseContext (crypt, 0);
      return;
    }
    CryptReleaseContext (crypt, 0);
  }
#else
  int fd;

  /* Open kernel random generator */
  fd = open ("/dev/random", O_RDONLY);
  if (fd != -1) {
    ssize_t s;

    /* Read len random bytes */
    s = read (fd, buffer, len);
    close (fd);

    /* Check length of random bytes sequence generated */
    if (s == (ssize_t) len)
      return;
    i = s > 0 ? s : 0;
  }
#endif

  /* Use default (less secure) random generator */
  for (; i < len; i++)
    buffer[i] = g_random_int_range (0, 256);
}

/**
 * melo_rtsp_server_connection_digest_auth_response:
 * @conn: a RTSP connection handle
 * @realm: the realm to use for digest authentication
 * @opaque: the opaque to use for digest authentication
 * @signal_stale: add 'stale' header if set to %true
 *
 * Set response with a digest authentication error message.
 *
 * Returns: %true if the response has been set successfully, %false otherwise.
 */
bool
melo_rtsp_server_connection_digest_auth_response (
    MeloRtspServerConnection *conn, const char *realm, const char *opaque,
    int signal_stale)
{
  char buffer[256];
  size_t len;

  g_return_val_if_fail (conn, false);

  /* Generate a nonce */
  if (!conn->nonce) {
    /* Generate an array of 32 random bytes */
    generate_nonce (buffer, 32);

    /* Convert it into md5 string */
    conn->nonce = g_compute_checksum_for_data (
        G_CHECKSUM_MD5, (const unsigned char *) buffer, 32);
  }

  /* Create response */
  melo_rtsp_server_connection_init_response (conn, 401, "Unauthorized");
  len = g_snprintf (
      buffer, 255, "Digest realm=\"%s\", nonce=\"%s\"", realm, conn->nonce);

  /* Add opaque if necessary */
  if (opaque)
    len += g_snprintf (buffer + len, 255 - len, ", opaque=\"%s\"", opaque);

  /* Add stale if necessary */
  if (signal_stale)
    g_snprintf (buffer + len, 255 - len, ", stale=\"true\"");

  melo_rtsp_server_connection_add_header (conn, "WWW-Authenticate", buffer);

  return true;
}

/*
 * melo_rtsp.c: Tiny RTSP server
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

#include <string.h>

#include <gio/gio.h>

#include "melo_rtsp.h"

#define MELO_DEFAULT_MAX_USER 5
#define MELO_DEFAULT_BUFFER_SIZE 8192

typedef enum {
  MELO_RTSP_STATE_WAIT_HEADER = 0,
  MELO_RTSP_STATE_WAIT_BODY,
  MELO_RTSP_STATE_SEND_HEADER,
  MELO_RTSP_STATE_SEND_BODY
} MeloRTSPSate;

struct _MeloRTSPClient {
  /* Parent */
  MeloRTSP *parent;
  /* Client socket */
  GSocket *sock;
  /* RTSP status */
  MeloRTSPSate state;
  /* RTSP variables */
  MeloRTSPMethod method;
  const gchar *method_name;
  const gchar *url;
  GHashTable *headers;
  guint seq;
  gsize content_length;
  /* Server address */
  guchar server_ip[4];
  guint server_port;
  /* Client address */
  gchar *hostname;
  guchar ip[4];
  guint port;
  /* Input buffer */
  gchar *buffer;
  gsize buffer_size;
  gsize buffer_len;
  /* Output buffer */
  gchar *out_buffer;
  gsize out_buffer_size;
  gsize out_buffer_len;
  /* Packet buffer (response) */
  guchar *packet;
  gsize packet_len;
  GDestroyNotify packet_free;
  /* User data */
  gpointer user_data;
};

struct _MeloRTSPPrivate {
  /* Server socket */
  GSocket *sock;
  GMainContext *context;
  /* Clients */
  GMutex mutex;
  gint users;
  gint max_user;
  GList *clients;
  /* Request handler */
  MeloRTSPRequest request_cb;
  gpointer request_data;
  /* Read handler */
  MeloRTSPRead read_cb;
  gpointer read_data;
  /* Close handler */
  MeloRTSPClose close_cb;
  gpointer close_data;
};

G_DEFINE_TYPE_WITH_PRIVATE (MeloRTSP, melo_rtsp, G_TYPE_OBJECT)

static void
melo_rtsp_finalize (GObject *gobject)
{
  MeloRTSP *rtsp = MELO_RTSP (gobject);
  MeloRTSPPrivate *priv = melo_rtsp_get_instance_private (rtsp);

  /* Stop RTSP server */
  melo_rtsp_stop (rtsp);

  /* Clear mutex */
  g_mutex_clear (&priv->mutex);

  /* Chain up to the parent class */
  G_OBJECT_CLASS (melo_rtsp_parent_class)->finalize (gobject);
}

static void
melo_rtsp_class_init (MeloRTSPClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  /* Add custom finalize() function */
  oclass->finalize = melo_rtsp_finalize;
}

static void
melo_rtsp_init (MeloRTSP *self)
{
  MeloRTSPPrivate *priv = melo_rtsp_get_instance_private (self);

  self->priv = priv;
  priv->sock = NULL;
  priv->users = 0;
  priv->max_user = MELO_DEFAULT_MAX_USER;
  priv->clients = NULL;

  /* Init mutex */
  g_mutex_init (&priv->mutex);
}

MeloRTSP *
melo_rtsp_new (void)
{
  return g_object_new (MELO_TYPE_RTSP, NULL);
}

gboolean
melo_rtsp_start (MeloRTSP *rtsp, guint port)
{
  MeloRTSPPrivate *priv = rtsp->priv;
  GInetAddress *inet_addr;
  GSocketAddress *addr;
  gboolean ret;
  GError *err;

  /* Server is already started */
  if (priv->sock)
    return FALSE;

  /* Open socket */
  priv->sock = g_socket_new (G_SOCKET_FAMILY_IPV4, G_SOCKET_TYPE_STREAM,
                             G_SOCKET_PROTOCOL_DEFAULT, &err);
  if (!priv->sock) {
    g_clear_error (&err);
    return FALSE;
  }

  /* Create a new address for bind */
  inet_addr = g_inet_address_new_any (G_SOCKET_FAMILY_IPV4);
  addr = g_inet_socket_address_new (inet_addr, port);
  g_object_unref (inet_addr);
  if (!addr)
    goto failed;

  /* Bind */
  ret = g_socket_bind (priv->sock, addr, TRUE, &err);
  g_object_unref (addr);
  if (!ret) {
    g_clear_error (&err);
    goto failed;
  }

  /* Keep connection alive */
  g_socket_set_keepalive (priv->sock, TRUE);

  /* Setup non-blocking */
  g_socket_set_blocking (priv->sock, FALSE);

  /* Listen */
  if (!g_socket_listen (priv->sock, &err)) {
    g_clear_error (&err);
    goto failed;
  }

  return TRUE;
failed:
  g_object_unref (priv->sock);
  priv->sock = NULL;
  return FALSE;
}

void
melo_rtsp_stop (MeloRTSP *rtsp)
{
  MeloRTSPPrivate *priv = rtsp->priv;
  GError *err;

  /* No server running */
  if (!priv->sock)
    return;

  /* Close server socker */
  if (!g_socket_close (priv->sock, &err)) {
    g_clear_error (&err);
    return;
  }

  /* Free socket */
  g_object_unref (priv->sock);
  priv->sock = NULL;

  /* Remove context */
  priv->context = NULL;
}

void
melo_rtsp_set_request_callback (MeloRTSP *rtsp, MeloRTSPRequest callback,
                                gpointer user_data)
{
  rtsp->priv->request_cb = callback;
  rtsp->priv->request_data = user_data;
}

void
melo_rtsp_set_read_callback (MeloRTSP *rtsp, MeloRTSPRead callback,
                             gpointer user_data)
{
  rtsp->priv->read_cb = callback;
  rtsp->priv->read_data = user_data;
}

void
melo_rtsp_set_close_callback (MeloRTSP *rtsp, MeloRTSPClose callback,
                              gpointer user_data)
{
  rtsp->priv->close_cb = callback;
  rtsp->priv->close_data = user_data;
}

static void
melo_rtsp_client_close (MeloRTSPClient *client)
{
  MeloRTSP *rtsp = client->parent;
  MeloRTSPPrivate *priv = rtsp->priv;

  /* Close client socket */
  if (client->sock) {
    g_socket_close (client->sock, NULL);
    g_object_unref (client->sock);
  }

  /* Callback before closing client socket */
  if (priv->close_cb)
    priv->close_cb (client, priv->close_data, &client->user_data);

  /* Lock client list */
  g_mutex_lock (&priv->mutex);

  /* Remove client from list */
  priv->clients = g_list_remove (priv->clients, client);
  priv->users--;

  /* Unlock client list */
  g_mutex_unlock (&priv->mutex);

  /* Free hash table for headers */
  g_hash_table_unref (client->headers);

  /* Free buffers */
  g_slice_free1 (client->out_buffer_size, client->out_buffer);
  g_slice_free1 (client->buffer_size, client->buffer);
  if (client->packet && client->packet_free)
    client->packet_free (client->packet);

  /* Free client hostname */
  g_free (client->hostname);

  /* Free client */
  g_slice_free (MeloRTSPClient, client);
  g_object_unref (rtsp);
}

static inline gboolean
melo_rtsp_extract_string (const gchar **dest, gchar **src, gsize *len,
                          const gchar *needle, gsize needle_len)
{
  gchar *end;

  /* Find end of string */
  end = g_strstr_len (*src, *len, needle);
  if (!end)
    return FALSE;

  /* Set string */
  *len -= end - *src + needle_len;
  if (dest)
    *dest = *src;
  *src = end + needle_len;
  *end = '\0';

  return TRUE;
}

static gboolean
melo_rtsp_parse_request (MeloRTSPClient *client, gsize len)
{
  gchar *buf = client->buffer;

  /* Get method name */
  if (!melo_rtsp_extract_string (&client->method_name, &buf, &len, " ", 1))
    return FALSE;

  /* Find method */
  if (!g_strcmp0 (client->method_name, "OPTIONS"))
    client->method = MELO_RTSP_METHOD_OPTIONS;
  else if (!g_strcmp0 (client->method_name, "DESCRIBE"))
    client->method = MELO_RTSP_METHOD_DESCRIBE;
  else if (!g_strcmp0 (client->method_name, "ANNOUNCE"))
    client->method = MELO_RTSP_METHOD_ANNOUNCE;
  else if (!g_strcmp0 (client->method_name, "SETUP"))
    client->method = MELO_RTSP_METHOD_SETUP;
  else if (!g_strcmp0 (client->method_name, "PLAY"))
    client->method = MELO_RTSP_METHOD_PLAY;
  else if (!g_strcmp0 (client->method_name, "PAUSE"))
    client->method = MELO_RTSP_METHOD_PAUSE;
  else if (!g_strcmp0 (client->method_name, "TEARDOWN"))
    client->method = MELO_RTSP_METHOD_TEARDOWN;
  else if (!g_strcmp0 (client->method_name, "GET_PARAMETER"))
    client->method = MELO_RTSP_METHOD_GET_PARAMETER;
  else if (!g_strcmp0 (client->method_name, "SET_PARAMETER"))
    client->method = MELO_RTSP_METHOD_SET_PARAMETER;
  else if (!g_strcmp0 (client->method_name, "RECORD"))
    client->method = MELO_RTSP_METHOD_RECORD;
  else
    client->method = MELO_RTSP_METHOD_UNKNOWN;

  /* Get URL */
  if (!melo_rtsp_extract_string (&client->url, &buf, &len, " ", 1))
    return FALSE;

  /* Go to first header line (skip RTSP version) */
  if (!melo_rtsp_extract_string (NULL, &buf, &len, "\r\n", 2))
    return FALSE;

  /* Parse all headers now */
  while (len > 2) {
    const gchar *name, *value;

    /* Get name */
    if (!melo_rtsp_extract_string (&name, &buf, &len, ": ", 2))
      return FALSE;

    /* Get value */
    if (!melo_rtsp_extract_string (&value, &buf, &len, "\r\n", 2))
      return FALSE;

    /* Add name / value pair to hash table */
    g_hash_table_insert (client->headers, (gpointer) name, (gpointer) value);
  }

  return TRUE;
}

static gboolean
melo_rtsp_handle_client (GSocket *sock, GIOCondition condition,
                         MeloRTSPClient *client)
{
  MeloRTSPPrivate *priv = client->parent->priv;
  GSource *source;
  gchar *buf;
  gsize len;

  /* Read from socket */
  if (condition & G_IO_IN) {
      /* Read data from socket */
      len = g_socket_receive (sock, client->buffer + client->buffer_len,
                              client->buffer_size - client->buffer_len,
                              NULL, NULL);
      if (len <= 0)
        goto close;
      client->buffer_len += len;
  }

  /* Parse buffer */
  switch (client->state) {
    case MELO_RTSP_STATE_WAIT_HEADER:
      /* Find end of header */
      buf = g_strstr_len (client->buffer, client->buffer_len, "\r\n\r\n");

      /* Not enough data to parse */
      if (!buf)
        break;

      /* Parse request */
      len = buf - client->buffer + 4;
      if (!melo_rtsp_parse_request (client, len - 2))
        goto failed;

      /* Get content length */
      buf = g_hash_table_lookup (client->headers, "Content-Length");
      if (buf)
        client->content_length = strtoul (buf, NULL, 10);

      /* Call request callback */
      if (priv->request_cb)
        priv->request_cb (client, client->method, client->url,
                          priv->request_data, &client->user_data);

      /* Reset request details (only available during request_cb call) */
      g_hash_table_remove_all (client->headers);
      client->method_name = NULL;
      client->url = NULL;

      /* Move body data to buffer start */
      memmove (client->buffer, client->buffer + len, client->buffer_len - len);
      client->buffer_len -= len;

      /* Go to next state: wait body */
      client->state = MELO_RTSP_STATE_WAIT_BODY;

      /* Not enough data yet */
      if (client->content_length > client->buffer_len)
        break;

    case MELO_RTSP_STATE_WAIT_BODY:
      if (client->content_length) {
        /* Get body data */
        if (client->content_length <= client->buffer_len) {
          /* Last chunck */
          if (priv->read_cb)
            priv->read_cb (client, client->buffer, client->content_length, TRUE,
                           priv->read_data, &client->user_data);

          /* Move remaining data to buffer start */
          memmove (client->buffer, client->buffer + client->content_length,
                   client->buffer_len - client->content_length);
          client->buffer_len -= client->content_length;
          client->content_length = 0;
        } else if (client->buffer_len == client->buffer_size) {
          /* Next chunk */
          if (priv->read_cb)
            priv->read_cb (client, client->buffer, client->buffer_size, FALSE,
                           priv->read_data, &client->user_data);

          /* Reset buffer */
          client->content_length -= client->buffer_size;
          client->buffer_len = 0;
          break;
        } else
          break;
      }

      /* No response */
      if (client->out_buffer_len == 0)
        melo_rtsp_init_response (client, 404, "Not found");

      /* Go to next state: send reply header */
      client->state = MELO_RTSP_STATE_SEND_HEADER;

      /* Add source to wait socket becomes writable */
      source = g_socket_create_source (sock, G_IO_OUT, NULL);
      g_source_set_callback (source, (GSourceFunc) melo_rtsp_handle_client,
                             client, NULL);
      g_source_attach (source, priv->context);
      return G_SOURCE_REMOVE;

    case MELO_RTSP_STATE_SEND_HEADER:
      if (client->out_buffer_len > 0) {
        len = g_socket_send (sock, client->out_buffer, client->out_buffer_len,
                             NULL, NULL);
        if (len <= 0)
          goto close;
        client->out_buffer_len -= len;
        if (client->out_buffer_len > 0)
          break;
      }

      /* Go to next state: send reply body */
      client->state = MELO_RTSP_STATE_SEND_BODY;
      break;
    case MELO_RTSP_STATE_SEND_BODY:
      if (client->packet) {
        if (client->packet_len > 0) {
          len = g_socket_send (sock, client->packet, client->packet_len, NULL,
                               NULL);
          if (len <= 0)
            goto close;
          client->packet_len -= len;
          if (client->packet_len > 0)
            break;
        }

        /* Free packet */
        if (client->packet_free)
          client->packet_free (client->packet_free);
        client->packet_free = NULL;
        client->packet_len = 0;
        client->packet = NULL;
      }

      /* Go to next state: wait for next request */
      client->state = MELO_RTSP_STATE_WAIT_HEADER;

      /* Add source to wait next incoming packet */
      source = g_socket_create_source (sock, G_IO_IN | G_IO_PRI, NULL);
      g_source_set_callback (source, (GSourceFunc) melo_rtsp_handle_client,
                             client, NULL);
      g_source_attach (source, priv->context);
      g_source_unref (source);
      return G_SOURCE_REMOVE;
  }

  return G_SOURCE_CONTINUE;

failed:
  g_socket_send (sock, "RTSP/1.0 400 Bad request\r\n\r\n", 28, NULL,
                 NULL);
close:
  melo_rtsp_client_close (client);
  return G_SOURCE_REMOVE;
}

static gboolean
melo_rtsp_get_address (GSocketAddress *addr, guchar *ip, guint *port,
                       gchar **name)
{
  GInetSocketAddress *inet_sock_addr;
  GInetAddress *inet_addr;
  GResolver *resolver;
  gsize len;

  /* Not an INET Address */
  if (!addr || !G_IS_INET_SOCKET_ADDRESS (addr))
    return FALSE;

  /* Get inet socket address */
  inet_sock_addr = G_INET_SOCKET_ADDRESS (addr);

  /* Get IP */
  inet_addr = g_inet_socket_address_get_address (inet_sock_addr);
  len = g_inet_address_get_native_size (inet_addr);
  memcpy (ip, g_inet_address_to_bytes (inet_addr), len > 4 ? 4 : len);

  /* Get port */
  *port = g_inet_socket_address_get_port (inet_sock_addr);

  /* Get hostname */
  if (name) {
    resolver = g_resolver_get_default ();
    *name = g_resolver_lookup_by_address (resolver, inet_addr, NULL, NULL);
    g_object_unref (resolver);
  }

  return TRUE;
}

static gboolean
melo_rtsp_accept (GSocket *server_sock, GIOCondition condition, MeloRTSP *rtsp)
{
  MeloRTSPPrivate *priv = rtsp->priv;
  MeloRTSPClient *client;
  GSocketAddress *addr;
  GSource *source;
  GSocket *sock;

  /* An error occured: stop server */
  if (condition != G_IO_IN)
    return G_SOURCE_REMOVE;

  /* Accept client */
  sock = g_socket_accept (priv->sock, NULL, NULL);
  if (!sock)
    goto exit;

  /* Check users count */
  if (priv->users >= priv->max_user) {
    g_socket_send (sock, "RTSP/1.0 503 Server too busy\r\n\r\n", 32,
                   NULL, NULL);
    goto failed;
  }

  /* Setup non-blocking */
  g_socket_set_blocking (sock, FALSE);

  /* Create a new client */
  client = g_slice_new0 (MeloRTSPClient);
  if (!client)
    goto failed;

  /* Fill client */
  client->sock = sock;
  client->parent = g_object_ref (rtsp);
  client->state = MELO_RTSP_STATE_WAIT_HEADER,

  /* Get server details */
  addr = g_socket_get_local_address (sock, NULL);
  melo_rtsp_get_address (addr, client->server_ip, &client->server_port, NULL);
  g_object_unref (addr);

  /* Get client details */
  addr = g_socket_get_remote_address (sock, NULL);
  melo_rtsp_get_address (addr, client->ip, &client->port, &client->hostname);
  g_object_unref (addr);

  /* Allocate buffer */
  client->buffer_len = 0;
  client->buffer_size = MELO_DEFAULT_BUFFER_SIZE;
  client->buffer = g_slice_alloc (client->buffer_size);

  /* Allocate output buffer */
  client->out_buffer_len = 0;
  client->out_buffer_size = MELO_DEFAULT_BUFFER_SIZE;
  client->out_buffer = g_slice_alloc (client->out_buffer_size);

  /* Create hash table for headers */
  client->headers = g_hash_table_new (g_str_hash, g_str_equal);

  /* Lock client list */
  g_mutex_lock (&priv->mutex);

  /* Add client to list */
  priv->clients = g_list_prepend (priv->clients, client);
  priv->users++;

  /* Unlock client list */
  g_mutex_unlock (&priv->mutex);

  /* Create and attach source to wait next incoming packet */
  source = g_socket_create_source (sock, G_IO_IN | G_IO_PRI, NULL);
  g_source_set_callback (source, (GSourceFunc) melo_rtsp_handle_client, client,
                         NULL);
  g_source_attach (source, priv->context);
  g_source_unref (source);

exit:
  return G_SOURCE_CONTINUE;

failed:
  g_socket_close (sock, NULL);
  g_object_unref (sock);
  return G_SOURCE_CONTINUE;
}

guint
melo_rtsp_attach (MeloRTSP *rtsp, GMainContext *context)
{
  MeloRTSPPrivate *priv = rtsp->priv;
  GSource *source;
  guint id;

  /* Already attached */
  if (priv->context)
    return 0;
  priv->context = context;

  /* Get source from server socket */
  source = g_socket_create_source (priv->sock,
                                   G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_NVAL,
                                   NULL);
  if (!source)
    return 0;

  /* Set server callback */
  g_source_set_callback (source, (GSourceFunc) melo_rtsp_accept, rtsp,
                         (GDestroyNotify) melo_rtsp_stop);

  /* Attach server socket */
  id = g_source_attach (source, context);
  g_source_unref (source);

  return id;
}

MeloRTSPMethod
melo_rtsp_get_method (MeloRTSPClient *client)
{
  g_return_val_if_fail (client, MELO_RTSP_METHOD_UNKNOWN);
  return client->method;
}

const gchar *
melo_rtsp_get_method_name (MeloRTSPClient *client)
{
  g_return_val_if_fail (client, NULL);
  return client->method_name;
}

const gchar *
melo_rtsp_get_header (MeloRTSPClient *client, const gchar *name)
{
  return g_hash_table_lookup (client->headers, name);
}

const guchar *
melo_rtsp_get_ip (MeloRTSPClient *client)
{
  g_return_val_if_fail (client, NULL);
  return client->ip;
}

guint
melo_rtsp_get_port (MeloRTSPClient *client)
{
  g_return_val_if_fail (client, 0);
  return client->port;
}

const gchar *
melo_rtsp_get_hostname (MeloRTSPClient *client)
{
  g_return_val_if_fail (client, NULL);
  return client->hostname;
}

const guchar *
melo_rtsp_get_server_ip (MeloRTSPClient *client)
{
  g_return_val_if_fail (client, NULL);
  return client->server_ip;
}

guint
melo_rtsp_get_server_port (MeloRTSPClient *client)
{
  g_return_val_if_fail (client, 0);
  return client->server_port;
}

gboolean
melo_rtsp_init_response (MeloRTSPClient *client, guint code,
                         const gchar *reason)
{
  g_return_val_if_fail (client, FALSE);

  /* Reset output buffer */
  client->out_buffer_len = 0;

  /* Create first line of response */
  client->out_buffer_len += g_snprintf (client->out_buffer,
                                        client->out_buffer_size,
                                        "RTSP/1.0 %d %s\r\n\r\n",
                                        code, reason);

  return TRUE;
}

gboolean
melo_rtsp_add_header (MeloRTSPClient *client, const gchar *name,
                      const gchar *value)
{
  g_return_val_if_fail (client, FALSE);
  g_return_val_if_fail (client->out_buffer_len != 0, FALSE);

  /* Add header to response */
  client->out_buffer_len += g_snprintf (
                           client->out_buffer + client->out_buffer_len - 2,
                           client->out_buffer_size - client->out_buffer_len + 2,
                           "%s: %s\r\n\r\n", name, value) - 2;

  return TRUE;
}

gboolean
melo_rtsp_set_response (MeloRTSPClient *client, const gchar *response)
{
  gsize len;

  g_return_val_if_fail (client, FALSE);

  /* Get response length */
  len = strlen (response);
  if (len > client->out_buffer_size)
    return FALSE;

  /* Copy response */
  memcpy (client->out_buffer, response, len);

  return TRUE;
}

gboolean
melo_rtsp_set_packet (MeloRTSPClient *client, guchar *buffer, gsize len,
                      GDestroyNotify free)
{
  g_return_val_if_fail (client, FALSE);

  client->packet = buffer;
  client->packet_len = len;
  client->packet_free = free;

  return TRUE;
}

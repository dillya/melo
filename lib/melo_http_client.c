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

#define MELO_LOG_TAG "http_client"
#include "melo/melo_log.h"

#include "melo/melo_http_client.h"

typedef struct _MeloHttpClientAsync {
  MeloHttpClient *client;
  union {
    MeloHttpClientCb cb;
    MeloHttpClientJsonCb json_cb;
  };
  void *user_data;
} MeloHttpClientAsync;

struct _MeloHttpClient {
  /* Parent instance */
  GObject parent_instance;

  SoupSession *session;
};

G_DEFINE_TYPE (MeloHttpClient, melo_http_client, G_TYPE_OBJECT)

static void
melo_http_client_finalize (GObject *gobject)
{
  MeloHttpClient *self = MELO_HTTP_CLIENT (gobject);

  /* Free Soup session */
  g_object_unref (self->session);

  /* Chain up to the parent class */
  G_OBJECT_CLASS (melo_http_client_parent_class)->finalize (gobject);
}

static void
melo_http_client_class_init (MeloHttpClientClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  /* Attach finalize function */
  object_class->finalize = melo_http_client_finalize;
}

static void
melo_http_client_init (MeloHttpClient *self)
{
  /* Create a new Soup session */
  self->session = soup_session_new ();
}

/**
 * melo_http_client_new:
 * @user_agent: (nullable): the user-agent to use for requests
 *
 * Creates a new HTTP client.
 *
 * Returns: (transfer full): a new #MeloHttpClient.
 */
MeloHttpClient *
melo_http_client_new (const char *user_agent)
{
  MeloHttpClient *client;

  /* Create client */
  client = g_object_new (MELO_TYPE_HTTP_CLIENT, NULL);

  /* Set user agent */
  if (client && client->session)
    g_object_set (client->session, SOUP_SESSION_USER_AGENT,
        user_agent ? user_agent : "Melo", NULL);

  return client;
}

/**
 * melo_http_client_set_max_connections:
 * @client: the #MeloHttpClient
 * @max_connections: the maximum number of connections opened at once
 *
 * This function can be used to limit the number of connections that the client
 * can open at once.
 */
void
melo_http_client_set_max_connections (
    MeloHttpClient *client, unsigned int max_connections)
{
  if (client && max_connections > 0)
    g_object_set (client->session, "max-conns", max_connections,
        "max-conns-per-host", max_connections, NULL);
}

static void
send_cb (SoupSession *session, SoupMessage *msg, gpointer user_data)
{
  MeloHttpClientAsync *async = user_data;

  /* Forward response body */
  if (async->cb)
    async->cb (async->client, msg->status_code, msg->response_body->data,
        msg->response_body->length, async->user_data);

  /* Release asynchronous data */
  free (async);
}

/**
 * melo_http_client_get:
 * @client: the #MeloHttpClient
 * @url: the URL request
 * @cb: the #MeloHttpClientCb to call when response is received
 * @user_data: the data to pass to @cb
 *
 * This function can be used to send an HTTP GET request to @url. When the
 * response body is fully received, the callback @cb will be called.
 *
 * Returns: %true if the GET request has been sent, %false otherwise.
 */
bool
melo_http_client_get (MeloHttpClient *client, const char *url,
    MeloHttpClientCb cb, void *user_data)
{
  MeloHttpClientAsync *async;
  SoupMessage *msg;

  if (!client || !url)
    return false;

  /* Create GET request */
  msg = soup_message_new ("GET", url);
  if (!msg)
    return false;

  /* Allocate asynchronous data */
  async = malloc (sizeof (*async));
  if (!async) {
    g_object_unref (msg);
    return false;
  }

  /* Fill asynchronous data */
  async->client = client;
  async->cb = cb;
  async->user_data = user_data;

  /* Send request */
  soup_session_queue_message (client->session, msg, send_cb, async);

  return true;
}

static void
parser_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  JsonParser *parser = JSON_PARSER (source_object);
  MeloHttpClientAsync *async = user_data;

  /* Parser has finished */
  if (json_parser_load_from_stream_finish (parser, res, NULL) && async->json_cb)
    async->json_cb (
        async->client, json_parser_get_root (parser), async->user_data);

  /* Release parser and asynchronous data */
  g_object_unref (parser);
  free (async);
}

static void
send_async_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  SoupSession *session = SOUP_SESSION (source_object);
  MeloHttpClientAsync *async = user_data;
  GInputStream *stream;
  GError *error = NULL;
  JsonParser *parser;

  /* Get response body stream */
  stream = soup_session_send_finish (session, res, &error);
  if (!stream) {
    MELO_LOGE ("failed to get input stream: %s", error->message);
    g_error_free (error);
    free (async);
    return;
  }

  /* Create JSON parser */
  parser = json_parser_new ();
  if (!parser) {
    g_object_unref (stream);
    free (async);
    return;
  }

  /* Start parser */
  json_parser_load_from_stream_async (parser, stream, NULL, parser_cb, async);
  g_object_unref (stream);
}

/**
 * melo_http_client_get_json:
 * @client: the #MeloHttpClient
 * @url: the URL request
 * @cb: the #MeloHttpClientJsonCb to call when response is received
 * @user_data: the data to pass to @cb
 *
 * This function can be used to send an HTTP GET request to @url and parse the
 * JSON response if the response content type is "application/json". When the
 * response body has been fully parsed and final JSON object is ready to be
 * used, the callback @cb will be called.
 *
 * Returns: %true if the GET request has been sent, %false otherwise.
 */
bool
melo_http_client_get_json (MeloHttpClient *client, const char *url,
    MeloHttpClientJsonCb cb, void *user_data)
{
  MeloHttpClientAsync *async;
  SoupMessage *msg;

  if (!client || !url)
    return false;

  /* Create GET request */
  msg = soup_message_new ("GET", url);
  if (!msg)
    return false;

  /* Allocate asynchronous data */
  async = malloc (sizeof (*async));
  if (!async) {
    g_object_unref (msg);
    return false;
  }

  /* Fill asynchronous data */
  async->client = client;
  async->json_cb = cb;
  async->user_data = user_data;

  /* Send request */
  soup_session_send_async (client->session, msg, NULL, send_async_cb, async);

  return true;
}

/**
 * melo_http_client_status_cannot_resolve:
 * @code: the code passed to #MeloHttpClientCb
 *
 * Returns: %true if client was unable to resolve destination host name, %false
 *     otherwise.
 */
bool
melo_http_client_status_cannot_resolve (unsigned int code)
{
  return code == SOUP_STATUS_CANT_RESOLVE ||
         code == SOUP_STATUS_CANT_RESOLVE_PROXY;
}

/**
 * melo_http_client_status_cannot_connect:
 * @code: the code passed to #MeloHttpClientCb
 *
 * Returns: %true if client was unable to connect to remote host, %false
 *     otherwise.
 */
bool
melo_http_client_status_cannot_connect (unsigned int code)
{
  return code == SOUP_STATUS_CANT_CONNECT ||
         code == SOUP_STATUS_CANT_CONNECT_PROXY;
}

/**
 * melo_http_client_status_ssl_failed:
 * @code: the code passed to #MeloHttpClientCb
 *
 * Returns: %true if SSL/TLS negotiation failed, %false otherwise.
 */
bool
melo_http_client_status_ssl_failed (unsigned int code)
{
  return code == SOUP_STATUS_SSL_FAILED;
}

/**
 * melo_http_client_status_io_error:
 * @code: the code passed to #MeloHttpClientCb
 *
 * Returns: %true if a network error occurred, %false otherwise.
 */
bool
melo_http_client_status_io_error (unsigned int code)
{
  return code == SOUP_STATUS_IO_ERROR;
}

/**
 * melo_http_client_status_too_many_redirects:
 * @code: the code passed to #MeloHttpClientCb
 *
 * Returns: %true if there were too many redirections, %false otherwise.
 */
bool
melo_http_client_status_too_many_redirects (unsigned int code)
{
  return code == SOUP_STATUS_TOO_MANY_REDIRECTS;
}

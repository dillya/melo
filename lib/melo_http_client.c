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
  JsonParser *parser;

  /* Get response body stream */
  stream = soup_session_send_finish (session, res, NULL);
  if (!stream) {
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

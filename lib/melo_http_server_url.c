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

#define MELO_LOG_TAG "http_server_url"
#include "melo/melo_log.h"

#include "melo_http_server_url.h"

typedef struct {
  SoupServer *server;
  SoupMessage *msg;

  SoupSession *session;
  SoupMessage *msg2;

} MeloHttpServerUrlData;

static void
copy_header_cb (const char *name, const char *value, void *user_data)
{
  soup_message_headers_append (user_data, name, value);
}

static void
headers_cb (SoupMessage *msg, void *user_data)
{
  MeloHttpServerUrlData *data = user_data;

  soup_message_set_status_full (
      data->msg, msg->status_code, msg->reason_phrase);
  soup_message_headers_foreach (
      msg->response_headers, copy_header_cb, data->msg->response_headers);
  soup_message_headers_remove (data->msg->response_headers, "Content-Length");
  soup_server_unpause_message (data->server, data->msg);
}

static void
chunk_cb (SoupMessage *msg, SoupBuffer *chunk, void *user_data)
{
  MeloHttpServerUrlData *data = user_data;

  soup_message_body_append_buffer (data->msg->response_body, chunk);
  soup_server_unpause_message (data->server, data->msg);
}

static void
finished_cb (SoupMessage *msg, void *user_data)
{
  MeloHttpServerUrlData *data = user_data;

  soup_session_cancel_message (data->session, data->msg2, SOUP_STATUS_IO_ERROR);
}

static void
done_cb (SoupSession *session, SoupMessage *msg, void *user_data)
{
  MeloHttpServerUrlData *data = user_data;

  g_signal_handlers_disconnect_by_func (data->msg, finished_cb, data);

  soup_message_body_complete (data->msg->response_body);
  soup_server_unpause_message (data->server, data->msg);
  g_object_unref (data->msg);
  free (data);
}

void
melo_http_server_url_serve (
    SoupServer *server, SoupMessage *msg, SoupSession *session, const char *url)
{
  MeloHttpServerUrlData *data;
  SoupMessage *msg2;

  /* Allocate asynchronous data */
  data = malloc (sizeof (*data));
  if (!data)
    return;

  /* Create a new message */
  msg2 = soup_message_new (msg->method, url);
  if (!msg2) {
    free (data);
    return;
  }

  /* Copy headers */
  soup_message_headers_foreach (
      msg->request_headers, copy_header_cb, msg2->request_headers);

  /* Remove host and connection headers */
  soup_message_headers_remove (msg2->request_headers, "Host");
  soup_message_headers_remove (msg2->request_headers, "Connection");

  /* Set chunked encoding */
  soup_message_headers_set_encoding (
      msg->response_headers, SOUP_ENCODING_CHUNKED);

  /* Set data */
  data->server = server;
  data->msg = msg;
  data->session = session;
  data->msg2 = msg2;

  /* Capture headers and chunks from new message */
  g_signal_connect (msg2, "got_headers", G_CALLBACK (headers_cb), data);
  g_signal_connect (msg2, "got_chunk", G_CALLBACK (chunk_cb), data);

  /* Capture abort */
  g_signal_connect (msg, "finished", G_CALLBACK (finished_cb), data);

  /* Queue new message */
  soup_session_queue_message (session, msg2, done_cb, data);

  /* Keep and pause message */
  g_object_ref (msg);
  soup_server_pause_message (server, msg);
}

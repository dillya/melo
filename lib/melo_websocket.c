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

#define MELO_LOG_TAG "websocket"
#include "melo/melo_log.h"

#include "melo/melo_websocket.h"
#include "melo_websocket_priv.h"

MeloWebsocket *
melo_websocket_new (
    MeloWebsocketConnCb conn_cb, MeloWebsocketMsgCb msg_cb, void *user_data)
{
  MeloWebsocket *ws;

  /* Allocate websocket context */
  ws = malloc (sizeof (*ws));
  if (!ws) {
    MELO_LOGE ("failed to allocate websocket context");
    return NULL;
  }

  /* Fill context */
  ws->cbs.conn = conn_cb;
  ws->cbs.msg = msg_cb;
  ws->cbs.user_data = user_data;
  ws->user_data = NULL;
  ws->closed = false;

  return ws;
}

MeloWebsocket *
melo_websocket_copy (const MeloWebsocket *orig)
{
  MeloWebsocket *ws;

  /* Allocate and copy context */
  ws = malloc (sizeof (*ws));
  if (ws)
    memcpy (ws, orig, sizeof (*ws));

  return ws;
}

void
melo_websocket_destroy (MeloWebsocket *ws)
{
  /* Free context */
  free (ws);
}

/**
 * melo_websocket_get_protocol:
 * @ws: a #MeloWebsocket
 *
 * Gets protocol handled by the connection.
 *
 * Returns: (transfer none): the protocol handled by this connection or %NULL.
 */
const char *
melo_websocket_get_protocol (MeloWebsocket *ws)
{
  return ws ? soup_websocket_connection_get_protocol (ws->connection) : NULL;
}

/**
 * melo_websocket_set_user_data:
 * @ws: a #MeloWebsocket
 * @user_data: the data to attach
 *
 * Attach a data to the connection. Before attaching a new data, be sure to free
 * the previous one.
 */
void
melo_websocket_set_user_data (MeloWebsocket *ws, void *user_data)
{
  if (ws)
    ws->user_data = user_data;
}

/**
 * melo_websocket_get_user_data:
 * @ws: a #MeloWebsocket
 *
 * Gets data attached to the connection.
 *
 * Returns: (transfer none): the data to attached to the connection or %NULL.
 */
void *
melo_websocket_get_user_data (MeloWebsocket *ws)
{
  return ws ? ws->user_data : NULL;
}

/**
 * melo_websocket_send:
 * @ws: a #MeloWebsocket
 * @data: the data of the message to send
 * @size: the size of the message data
 * @text: %true if the message is a string, %false if the message is binary
 *
 * Sends a message to client.
 */
void
melo_websocket_send (
    MeloWebsocket *ws, const unsigned char *data, size_t len, bool text)
{
  if (!ws || ws->closed ||
      soup_websocket_connection_get_state (ws->connection) !=
          SOUP_WEBSOCKET_STATE_OPEN)
    return;

  /* Send message */
  if (text)
    soup_websocket_connection_send_text (ws->connection, (const char *) data);
  else
    soup_websocket_connection_send_binary (ws->connection, data, len);
}

/**
 * melo_websocket_close:
 * @ws: a #MeloWebsocket
 * @code: the code reason of close
 * @data: the data message associated to the closure, can be %NULL
 *
 * Close a websocket connection.
 */
void
melo_websocket_close (MeloWebsocket *ws, unsigned short code, const char *data)
{
  if (!ws || ws->closed ||
      soup_websocket_connection_get_state (ws->connection) !=
          SOUP_WEBSOCKET_STATE_OPEN)
    return;

  /* Close connection */
  soup_websocket_connection_close (ws->connection, code, data);
  ws->closed = true;
}

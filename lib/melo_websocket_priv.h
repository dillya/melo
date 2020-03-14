/**
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

#ifndef _MELO_WEBSOCKET_PRIV_H_
#define _MELO_WEBSOCKET_PRIV_H_

#include <libsoup/soup.h>

#include <melo/melo_websocket.h>

struct _MeloWebsocket {
  SoupWebsocketConnection *connection;
  struct {
    MeloWebsocketConnCb conn;
    MeloWebsocketMsgCb msg;
    void *user_data;
  } cbs;
  void *user_data;
  bool closed;
};

MeloWebsocket *melo_websocket_new (
    MeloWebsocketConnCb conn_cb, MeloWebsocketMsgCb msg_cb, void *user_data);

MeloWebsocket *melo_websocket_copy (const MeloWebsocket *orig);

void melo_websocket_destroy (MeloWebsocket *ws);

static inline void
melo_websocket_set_connection (
    MeloWebsocket *ws, SoupWebsocketConnection *connection)
{
  ws->connection = g_object_ref (connection);
}

static inline void
melo_websocket_signal_connection (MeloWebsocket *ws, bool connected)
{
  SoupURI *uri = soup_websocket_connection_get_uri (ws->connection);
  const char *path = soup_uri_get_path (uri);

  if (ws->cbs.conn)
    ws->cbs.conn (ws, path, connected, ws->cbs.user_data);
  if (!connected)
    g_object_unref (ws->connection);
}

static inline void
melo_websocket_signal_message (MeloWebsocket *ws, GBytes *msg)
{
  SoupURI *uri = soup_websocket_connection_get_uri (ws->connection);
  const char *path = soup_uri_get_path (uri);
  const unsigned char *data;
  gsize size;

  data = g_bytes_get_data (msg, &size);
  if (ws->cbs.msg)
    ws->cbs.msg (ws, path, data, size, ws->cbs.user_data);
}

#endif /* !_MELO_WEBSOCKET_PRIV_H_ */

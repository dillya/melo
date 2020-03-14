/*
 * Copyright (C) 2020 Alexandre Dilly <dillya@sparod.com>
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA.
 */

#include <glib.h>

#define MELO_LOG_TAG "melo_websocket"
#include <melo/melo_log.h>

#include <melo/melo_browser.h>
#include <melo/melo_player.h>
#include <melo/melo_playlist.h>

#include "websocket.h"

typedef enum {
  WEBSOCKET_TYPE_EVENT,
  WEBSOCKET_TYPE_REQUEST,
} WebsocketType;

typedef enum {
  WEBSOCKET_OBJECT_BROWSER,
  WEBSOCKET_OBJECT_PLAYER,
  WEBSOCKET_OBJECT_PLAYLIST,
} WebsocketObject;

static bool
websocket_async_cb (MeloMessage *msg, void *user_data)
{
  MeloWebsocket *ws = user_data;
  const unsigned char *data;
  size_t size;

  /* Get message data */
  data = melo_message_get_cdata (msg, &size);

  /* Send message or close connection */
  if (data && size)
    melo_websocket_send (ws, data, size, false);
  else
    melo_websocket_close (ws, 1000, NULL);

  return true;
}

static bool
websocket_parse_path (
    const char *path, WebsocketType type, WebsocketObject *obj, const char **id)
{
  /* Check path start */
  switch (type) {
  case WEBSOCKET_TYPE_EVENT:
    if (strncmp (path, "/api/event/", 11))
      return false;
    path += 11;
    break;
  case WEBSOCKET_TYPE_REQUEST:
    if (strncmp (path, "/api/request/", 13))
      return false;
    path += 13;
    break;
  default:
    return false;
  }

  /* Get object */
  if (!strncmp (path, "browser", 7)) {
    *obj = WEBSOCKET_OBJECT_BROWSER;
    path += 7;
  } else if (!strncmp (path, "player", 6)) {
    *obj = WEBSOCKET_OBJECT_PLAYER;
    path += 6;
  } else if (!strncmp (path, "playlist", 8)) {
    *obj = WEBSOCKET_OBJECT_PLAYLIST;
    path += 8;
  } else
    return false;

  /* Get ID */
  if (*path == '/')
    *id = path + 1;
  else if (*path != '\0')
    return false;
  else
    *id = NULL;

  return true;
}

/**
 * websocket_event_cb:
 * @ws: the #MeloWebsocet
 * @path: the websocket path
 * @connected: %true when connection has been accepted, %false when connection
 *     has been closed
 * @user_data: the data pointer associated to the callback
 *
 * Websocket connection callback for events.
 */
void
websocket_event_cb (
    MeloWebsocket *ws, const char *path, bool connected, void *user_data)
{
  WebsocketObject obj;
  const char *id;
  bool ret = false;

  /* Parse path */
  if (!websocket_parse_path (path, WEBSOCKET_TYPE_EVENT, &obj, &id)) {
    if (connected) {
      MELO_LOGW ("invalid path: %s", path);
      melo_websocket_close (ws, 1000, NULL);
    }
    return;
  }

  /* Add / remove event listener */
  switch (obj) {
  case WEBSOCKET_OBJECT_BROWSER:
    ret = connected
              ? melo_browser_add_event_listener (id, websocket_async_cb, ws)
              : melo_browser_remove_event_listener (id, websocket_async_cb, ws);
    break;
  case WEBSOCKET_OBJECT_PLAYER:
    ret = connected
              ? melo_player_add_event_listener (websocket_async_cb, ws)
              : melo_player_remove_event_listener (websocket_async_cb, ws);
    break;
  case WEBSOCKET_OBJECT_PLAYLIST:
    ret =
        connected
            ? melo_playlist_add_event_listener (id, websocket_async_cb, ws)
            : melo_playlist_remove_event_listener (id, websocket_async_cb, ws);
    break;
  default:
    MELO_LOGW ("unsupported event");
  }

  /* Failed to handle event connection */
  if (!ret)
    melo_websocket_close (ws, 1007, NULL);
}

/**
 * websocket_conn_request_cb:
 * @ws: the #MeloWebsocet
 * @path: the websocket path
 * @connected: %true when connection has been accepted, %false when connection
 *     has been closed
 * @user_data: the data pointer associated to the callback
 *
 * Websocket connection callback for requests.
 */
void
websocket_conn_request_cb (
    MeloWebsocket *ws, const char *path, bool connected, void *user_data)
{
  WebsocketObject obj;
  const char *id;

  /* Catch only disconnection */
  if (connected)
    return;

  /* Parse path */
  if (!websocket_parse_path (path, WEBSOCKET_TYPE_REQUEST, &obj, &id))
    return;

  /* Stop / cancel request */
  switch (obj) {
  case WEBSOCKET_OBJECT_BROWSER:
    melo_browser_cancel_request (id, websocket_async_cb, ws);
    break;
  case WEBSOCKET_OBJECT_PLAYLIST:
    melo_playlist_cancel_request (id, websocket_async_cb, ws);
    break;
  default:
    break;
  }
}

/**
 * websocket_request_cb:
 * @ws: the #MeloWebsocet
 * @path: the websocket path
 * @data: (transfer none) (array length=size): the message data received
 * @size: the size of the message data
 * @user_data: the user data pointer associated to the callback
 *
 * Websocket message callback for requests.
 */
void
websocket_request_cb (MeloWebsocket *ws, const char *path,
    const unsigned char *data, size_t size, void *user_data)
{
  WebsocketObject obj;
  MeloMessage *msg;
  const char *id;
  bool ret = false;

  /* Parse path */
  if (!websocket_parse_path (path, WEBSOCKET_TYPE_REQUEST, &obj, &id)) {
    MELO_LOGW ("invalid path: %s", path);
    melo_websocket_close (ws, 1000, NULL);
    return;
  }

  /* Create message */
  msg = melo_message_new_from_buffer (data, size);
  if (!msg) {
    MELO_LOGE ("failed to create request message: %s", path);
    melo_websocket_close (ws, 1011, NULL);
    return;
  }

  /* Forward message */
  switch (obj) {
  case WEBSOCKET_OBJECT_BROWSER:
    ret = melo_browser_handle_request (id, msg, websocket_async_cb, ws);
    break;
  case WEBSOCKET_OBJECT_PLAYER:
    ret = melo_player_handle_request (msg, websocket_async_cb, ws);
    break;
  case WEBSOCKET_OBJECT_PLAYLIST:
    ret = melo_playlist_handle_request (id, msg, websocket_async_cb, ws);
    break;
  default:
    MELO_LOGW ("unsupported request");
  }

  /* Failed to handle request */
  if (!ret)
    melo_websocket_close (ws, 1007, NULL);

  /* Free message */
  melo_message_unref (msg);
}

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

#ifndef _MELO_WEBSOCKET_H_
#define _MELO_WEBSOCKET_H_

#include <stdbool.h>
#include <stddef.h>

/**
 * MeloWebsocket:
 *
 * This object is created by #MeloHttpServer or #MeloHttpClient when a new
 * connection is created and opened.
 */
typedef struct _MeloWebsocket MeloWebsocket;

/**
 * MeloWebsocketConnCb!
 * @ws: the #MeloWebsocket
 * @path: the websocket path
 * @connected: %true when new connection occurred, %false when the connection
 *     has been closed
 * @user_data: the pointer provided during websocket handle registration
 *
 * This function is called when a new websocket connection is accepted by the
 * HTTP server.
 */
typedef void (*MeloWebsocketConnCb) (
    MeloWebsocket *ws, const char *path, bool connected, void *user_data);

/**
 * MeloWebsocketMsgCb:
 * @ws: the #MeloWebsocket
 * @path the websocket path
 * @data the data of the message received
 * @size the size of the message data
 * @user_data the pointer provided during websocket handle registration for
 *     server side and the pointer provided during the connection creation for
 *     client side
 *
 * This function is called when a new message has been received on current
 * websocket connection.
 */
typedef void (*MeloWebsocketMsgCb) (MeloWebsocket *ws, const char *path,
    const unsigned char *data, size_t size, void *user_data);

const char *melo_websocket_get_protocol (MeloWebsocket *ws);

void melo_websocket_set_user_data (MeloWebsocket *ws, void *user_data);
void *melo_websocket_get_user_data (MeloWebsocket *ws);

void melo_websocket_send (
    MeloWebsocket *ws, const unsigned char *data, size_t len, bool text);

void melo_websocket_close (
    MeloWebsocket *ws, unsigned short code, const char *data);

#endif /* !_MELO_WEBSOCKET_H_ */

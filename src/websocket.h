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

#ifndef _WEBSOCKET_H_
#define _WEBSOCKET_H_

#include <melo/melo_message.h>
#include <melo/melo_websocket.h>

void websocket_event_cb (
    MeloWebsocket *ws, const char *path, bool connected, void *user_data);

void websocket_conn_request_cb (
    MeloWebsocket *ws, const char *path, bool connected, void *user_data);

void websocket_request_cb (MeloWebsocket *ws, const char *path,
    const unsigned char *data, size_t size, void *user_data);

#endif /* !_WEBSOCKET_H_ */

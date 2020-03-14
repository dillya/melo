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

#ifndef _MELO_HTTP_SERVER_H_
#define _MELO_HTTP_SERVER_H_

#include <stdbool.h>

#include <glib-object.h>

#include <melo/melo_websocket.h>

G_BEGIN_DECLS

/**
 * MeloHttpServerConnection:
 *
 * This structure is used to handle a connection in the HTTP server: it will be
 * used to get request details and generate the response.
 */
typedef struct _MeloHttpServerConnection MeloHttpServerConnection;

/**
 * MeloHttpServer:
 *
 * This object can create an asynchronous HTTP(s) server to serve files and
 * arbitrary data. It supports also websocket through the #MeloWebsocket.
 */

#define MELO_TYPE_HTTP_SERVER melo_http_server_get_type ()
G_DECLARE_FINAL_TYPE (
    MeloHttpServer, melo_http_server, MELO, HTTP_SERVER, GObject)

/**
 * MeloHttpServerCb:
 * @server: the #MeloHttpServer
 * @connection: the current #MeloHttpServerConnection
 * @path: the path of the request
 * @user_data: user data passed to the callback
 *
 * This function is called when a new request is received by the HTTP server.
 * To make response, the #MeloHttpServerConnection related function should be
 * called, otherwise, a 500 - internal error will be returned by the server.
 */
typedef void (*MeloHttpServerCb) (MeloHttpServer *server,
    MeloHttpServerConnection *connection, const char *path, void *user_data);

MeloHttpServer *melo_http_server_new (void);

bool melo_http_server_add_handler (MeloHttpServer *srv, const char *path,
    MeloHttpServerCb cb, void *user_data);
bool melo_http_server_add_file_handler (
    MeloHttpServer *srv, const char *path, const char *www_path);
bool melo_http_server_add_websocket_handler (MeloHttpServer *srv,
    const char *path, const char *origin, char **protocols,
    MeloWebsocketConnCb conn_cb, MeloWebsocketMsgCb msg_cb, void *user_data);
void melo_http_server_remove_handler (MeloHttpServer *srv, const gchar *path);

bool melo_http_server_start (
    MeloHttpServer *srv, unsigned int http_port, unsigned int https_port);
void melo_http_server_stop (MeloHttpServer *srv);

void melo_http_server_connection_send_file (
    MeloHttpServerConnection *conn, const char *path);
void melo_http_server_connection_send_url (
    MeloHttpServerConnection *conn, const char *url);

G_END_DECLS

#endif /* !_MELO_HTTP_SERVER_H_ */

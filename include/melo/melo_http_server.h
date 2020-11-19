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

typedef enum _MeloHttpServerMethod MeloHttpServerMethod;

/**
 * MeloHttpServerConnection:
 *
 * This structure is used to handle a connection in the HTTP server: it will be
 * used to get request details and generate the response.
 */
typedef struct _MeloHttpServerConnection MeloHttpServerConnection;

/**
 * MeloHttpServerMethod:
 * @MELO_HTTP_SERVER_METHOD_UNKNOWN: unknown HTTP method
 * @MELO_HTTP_SERVER_METHOD_GET: GET HTTP method
 * @MELO_HTTP_SERVER_METHOD_HEAD: HEAD HTTP method
 * @MELO_HTTP_SERVER_METHOD_POST: POST HTTP method
 * @MELO_HTTP_SERVER_METHOD_PUT: PUT HTTP method
 * @MELO_HTTP_SERVER_METHOD_DELETE: DELETE HTTP method
 * @MELO_HTTP_SERVER_METHOD_CONNECT: CONNECT HTTP method
 * @MELO_HTTP_SERVER_METHOD_OPTIONS: OPTIONS HTTP method
 * @MELO_HTTP_SERVER_METHOD_TRACE: TRACE HTTP method
 *
 * This enumerator lists all HTTP methods supported by the HTTP server.
 */
enum _MeloHttpServerMethod {
  MELO_HTTP_SERVER_METHOD_UNKNOWN = 0,
  MELO_HTTP_SERVER_METHOD_GET,
  MELO_HTTP_SERVER_METHOD_HEAD,
  MELO_HTTP_SERVER_METHOD_POST,
  MELO_HTTP_SERVER_METHOD_PUT,
  MELO_HTTP_SERVER_METHOD_DELETE,
  MELO_HTTP_SERVER_METHOD_CONNECT,
  MELO_HTTP_SERVER_METHOD_OPTIONS,
  MELO_HTTP_SERVER_METHOD_TRACE,
};

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

/**
 * MeloHttpServerCloseCb:
 * @server: the #MeloHttpServer
 * @connection: the current #MeloHttpServerConnection
 * @user_data: user data passed to the callback
 *
 * This function is called when a request has finished (response body has been
 * sent) and the HTTP connection is closing. It should be used to release
 * resources used during request processing.
 */
typedef void (*MeloHttpServerCloseCb) (MeloHttpServer *server,
    MeloHttpServerConnection *connection, void *user_data);

/**
 * MeloHttpServerChunkCb:
 * @connection: the current #MeloHttpServerConnection
 * @chunk; a data chunk
 * @user_data: user data passed to the callback
 *
 * This function is called when a new request body data chunk is available. The
 * reference to @chunk must be released after usage with g_bytes_unref().
 * This function can be registered during server callback handling request
 * headers reception (see melo_http_server_add_handler()) with the dedicated
 * function melo_http_server_connection_capture_body().
 */
typedef void (*MeloHttpServerChunkCb) (
    MeloHttpServerConnection *connection, GBytes *chunk, void *user_data);

MeloHttpServer *melo_http_server_new (void);

bool melo_http_server_add_handler (MeloHttpServer *srv, const char *path,
    MeloHttpServerCb header_cb, MeloHttpServerCb body_cb,
    MeloHttpServerCloseCb close_cb, void *user_data);
bool melo_http_server_add_file_handler (
    MeloHttpServer *srv, const char *path, const char *www_path);
bool melo_http_server_add_websocket_handler (MeloHttpServer *srv,
    const char *path, const char *origin, char **protocols,
    MeloWebsocketConnCb conn_cb, MeloWebsocketMsgCb msg_cb, void *user_data);
void melo_http_server_remove_handler (MeloHttpServer *srv, const gchar *path);

bool melo_http_server_start (
    MeloHttpServer *srv, unsigned int http_port, unsigned int https_port);
void melo_http_server_stop (MeloHttpServer *srv);

void melo_http_server_connection_set_user_data (
    MeloHttpServerConnection *conn, void *user_data);
void *melo_http_server_connection_get_user_data (
    MeloHttpServerConnection *conn);

MeloHttpServerMethod melo_http_server_connection_get_method (
    MeloHttpServerConnection *conn);
ssize_t melo_http_server_connection_get_content_length (
    MeloHttpServerConnection *conn);

bool melo_http_server_connection_capture_body (
    MeloHttpServerConnection *conn, MeloHttpServerChunkCb cb, void *user_data);

void melo_http_server_connection_set_status (
    MeloHttpServerConnection *conn, unsigned int code);
void melo_http_server_connection_set_content_type (
    MeloHttpServerConnection *conn, const char *mime_type);
void melo_http_server_connection_set_content_length (
    MeloHttpServerConnection *conn, size_t len);

void melo_http_server_connection_send_chunk (MeloHttpServerConnection *conn,
    const unsigned char *data, size_t len, GDestroyNotify free_func,
    void *user_data);
void melo_http_server_connection_close (MeloHttpServerConnection *conn);

void melo_http_server_connection_send (MeloHttpServerConnection *conn,
    unsigned int code, const unsigned char *data, size_t len,
    GDestroyNotify free_func, void *user_data);
void melo_http_server_connection_send_file (
    MeloHttpServerConnection *conn, const char *path);
void melo_http_server_connection_send_url (
    MeloHttpServerConnection *conn, const char *url);

void melo_http_server_set_auth (MeloHttpServer *srv, bool enable,
    const char *username, const char *password);

G_END_DECLS

#endif /* !_MELO_HTTP_SERVER_H_ */

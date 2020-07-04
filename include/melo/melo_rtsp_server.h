/*
 * Copyright (C) 2016-2020 Alexandre Dilly <dillya@sparod.com>
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

#ifndef _MELO_RTSP_SERVER_H_
#define _MELO_RTSP_SERVER_H_

#include <stdbool.h>

#include <glib-object.h>

G_BEGIN_DECLS

/**
 * MeloRtspServerConnection:
 *
 * The opaque #MeloRtspServer data structure.
 */

typedef struct _MeloRtspServerConnection MeloRtspServerConnection;

#define MELO_TYPE_RTSP_SERVER (melo_rtsp_server_get_type ())
G_DECLARE_FINAL_TYPE (
    MeloRtspServer, melo_rtsp_server, MELO, RTSP_SERVER, GObject)

/**
 * MeloRtspMethod:
 * @MELO_RTSP_METHOD_UNKNOWN: unknown RTSP method (not in specification)
 * @MELO_RTSP_METHOD_OPTIONS: OPTIONS RTSP method
 * @MELO_RTSP_METHOD_DESCRIBE: DESCRIBE RTSP method
 * @MELO_RTSP_METHOD_ANNOUNCE: ANNOUNCE RTSP method
 * @MELO_RTSP_METHOD_SETUP: SETUP RTSP method
 * @MELO_RTSP_METHOD_PLAY: PLAY RTSP method
 * @MELO_RTSP_METHOD_PAUSE: PAUSE RTSP method
 * @MELO_RTSP_METHOD_TEARDOWN: TEARDOWN RTSP method
 * @MELO_RTSP_METHOD_GET_PARAMETER: GET_PARAMETER RTSP method
 * @MELO_RTSP_METHOD_SET_PARAMETER: SET_PARAMETER RTSP method
 * @MELO_RTSP_METHOD_RECORD: RECORD RTSP method
 *
 * #MeloRtspMethod represents all RTSP methods specified by the RFC 2326 and is
 * used to create or identify an RTSP request.
 *
 * Some custom protocols based on RTSP protocol can implement more methods, in
 * which case, the MELO_RTSP_METHOD_UNKNOWN is used and method name can then be
 * retrieved with melo_rtsp_server_get_method_name().
 */
typedef enum _MeloRtspMethod {
  MELO_RTSP_METHOD_UNKNOWN = 0,
  MELO_RTSP_METHOD_OPTIONS,
  MELO_RTSP_METHOD_DESCRIBE,
  MELO_RTSP_METHOD_ANNOUNCE,
  MELO_RTSP_METHOD_SETUP,
  MELO_RTSP_METHOD_PLAY,
  MELO_RTSP_METHOD_PAUSE,
  MELO_RTSP_METHOD_TEARDOWN,
  MELO_RTSP_METHOD_GET_PARAMETER,
  MELO_RTSP_METHOD_SET_PARAMETER,
  MELO_RTSP_METHOD_RECORD,
} MeloRtspMethod;

/**
 * MeloRtspServerRequestCb:
 * @connection: the current RTSP connection handle
 * @method: the method of the request
 * @url: the URL of the request
 * @user_data: a pointer to the user data set with callback
 * @conn_data: a pointer to the connection data
 *
 * This callback is called when a new request is received by the RTSP server
 * instance. For each new request, a #MeloRtspServerConnection handle is
 * provided to follow usage of the connection through a request parsing, a data
 * receiving from connection (handled by #MeloRtspServerReadCb) and end of
 * connection (handled by #MeloRtspServerCloseCb).
 * The @conn_data can be used to attach a specific buffer to the current
 * connection and it will be kept until end of connection. If the value is set,
 * it must be freed in the #MeloRtspServerCloseCb callback implementation.
 */
typedef void (*MeloRtspServerRequestCb) (MeloRtspServerConnection *connection,
    MeloRtspMethod method, const char *url, void *user_data, void **conn_data);

/**
 * MeloRtspServerReadCb:
 * @connection: the current RTSP connection handle
 * @buffer: a pointer to the buffer to filled with body RTSP request
 * @size: size of the buffer in bytes
 * @last: set to %TRUE if it is last buffer (end of body)
 * @user_data: a pointer to the user data set with callback
 * @conn_data: a pointer to the connection data
 *
 * This callback is called when data is received from a connection: @buffer is
 * filled with @size bytes of data received from the connection and
 * corresponding to the body request data. This callback can be called several
 * times, until the end of the body is reached, signaled by @last.
 * The @conn_data can be used to attach a specific buffer to the current
 * connection and it will be kept until end of connection. If the value is set,
 * it must be freed in the #MeloRtspServerCloseCb callback implementation.
 */
typedef void (*MeloRtspServerReadCb) (MeloRtspServerConnection *connection,
    unsigned char *buffer, size_t size, bool last, void *user_data,
    void **conn_data);

/**
 * MeloRtspServerCloseCb:
 * @connection: the current RTSP connection handle
 * @user_data: a pointer to the user data set with callback
 * @conn_data: a pointer to the connection data
 *
 * This callback is called at end of connection, after a request completion or
 * an abort / error.
 * If the @conn_data has been set in previous callback (#MeloRtspServerRequestCb
 * and/or #MeloRtspServerReadCb) execution, it should be freed here.
 */
typedef void (*MeloRtspServerCloseCb) (
    MeloRtspServerConnection *connection, void *user_data, void **conn_data);

GType melo_rtsp_server_get_type (void);

MeloRtspServer *melo_rtsp_server_new (void);

guint melo_rtsp_server_attach (MeloRtspServer *srv, GMainContext *context);

bool melo_rtsp_server_start (MeloRtspServer *srv, unsigned int port);
void melo_rtsp_server_stop (MeloRtspServer *srv);

void melo_rtsp_server_set_request_callback (
    MeloRtspServer *srv, MeloRtspServerRequestCb cb, void *user_data);
void melo_rtsp_server_set_read_callback (
    MeloRtspServer *srv, MeloRtspServerReadCb cb, void *user_data);
void melo_rtsp_server_set_close_callback (
    MeloRtspServer *srv, MeloRtspServerCloseCb cb, void *user_data);

MeloRtspMethod melo_rtsp_server_connection_get_method (
    MeloRtspServerConnection *conn);
const char *melo_rtsp_server_connection_get_method_name (
    MeloRtspServerConnection *conn);

const char *melo_rtsp_server_connection_get_header (
    MeloRtspServerConnection *conn, const char *name);
size_t melo_rtsp_server_connection_get_content_length (
    MeloRtspServerConnection *conn);

const unsigned char *melo_rtsp_server_connection_get_ip (
    MeloRtspServerConnection *conn);
const char *melo_rtsp_server_connection_get_ip_string (
    MeloRtspServerConnection *conn);
unsigned int melo_rtsp_server_connection_get_port (
    MeloRtspServerConnection *conn);
const char *melo_rtsp_server_connection_get_hostname (
    MeloRtspServerConnection *conn);

const unsigned char *melo_rtsp_server_connection_get_server_ip (
    MeloRtspServerConnection *conn);
unsigned int melo_rtsp_server_connection_get_server_port (
    MeloRtspServerConnection *conn);

bool melo_rtsp_server_connection_init_response (
    MeloRtspServerConnection *conn, unsigned int code, const char *reason);
bool melo_rtsp_server_connection_add_header (
    MeloRtspServerConnection *conn, const char *name, const char *value);
bool melo_rtsp_server_connection_set_response (
    MeloRtspServerConnection *conn, const char *response);
bool melo_rtsp_server_connection_set_packet (MeloRtspServerConnection *conn,
    unsigned char *buffer, size_t len, GDestroyNotify free);

/* Authentication part */
bool melo_rtsp_server_connection_basic_auth_check (
    MeloRtspServerConnection *conn, const char *username, const char *password);
bool melo_rtsp_server_connection_basic_auth_response (
    MeloRtspServerConnection *conn, const char *realm);
bool melo_rtsp_server_connection_digest_auth_check (
    MeloRtspServerConnection *conn, const char *username, const char *password,
    const char *realm);
bool melo_rtsp_server_connection_digest_auth_response (
    MeloRtspServerConnection *conn, const char *realm, const char *opaque,
    int signal_stale);

G_END_DECLS

#endif /* !_MELO_RTSP_SERVER_H_ */

/*
 * melo_rtsp.h: Tiny RTSP server
 *
 * Copyright (C) 2016 Alexandre Dilly <dillya@sparod.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifndef __MELO_RTSP_H__
#define __MELO_RTSP_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define MELO_TYPE_RTSP             (melo_rtsp_get_type ())
#define MELO_RTSP(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), MELO_TYPE_RTSP, MeloRTSP))
#define MELO_IS_RTSP(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MELO_TYPE_RTSP))
#define MELO_RTSP_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), MELO_TYPE_RTSP, MeloRTSPClass))
#define MELO_IS_RTSP_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), MELO_TYPE_RTSP))
#define MELO_RTSP_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), MELO_TYPE_RTSP, MeloRTSPClass))

typedef struct _MeloRTSP MeloRTSP;
typedef struct _MeloRTSPClass MeloRTSPClass;
typedef struct _MeloRTSPPrivate MeloRTSPPrivate;

typedef struct _MeloRTSPClient MeloRTSPClient;

/**
 * MeloRTSPMethod:
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
 * #MeloRTSPMethod represents all RTSP methods specified by the RFC 2326 and is
 * used to create or identify an RTSP request.
 *
 * Some custom protocols based on RTSP protocol can implement more methods, in
 * which case, the MELO_RTSP_METHOD_UNKNOWN is used and method name can then be
 * retrieved with melo_rtsp_get_method_name().
 */
typedef enum _MeloRTSPMethod {
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
} MeloRTSPMethod;

/**
 * MeloRTSP:
 *
 * The opaque #MeloRTSP data structure.
 */
struct _MeloRTSP {
  GObject parent_instance;

  /*< private >*/
  MeloRTSPPrivate *priv;
};

/**
 * MeloRTSPClass:
 * @parent_class: Object parent class
 *
 * The #MeloRTSPClass data structure.
 */
struct _MeloRTSPClass {
  GObjectClass parent_class;
};

/**
 * MeloRTSPRequest:
 * @client: the current RTSP client handle
 * @method: the method of the request
 * @url: the URL of the request
 * @user_data: a pointer to the user data set with callback
 * @client_data: a pointer to the client data
 *
 * This callback is called when a new request is received by the RTSP server
 * instance. For each new request, a #MeloRTSPClient handle is provided to
 * follow usage of the client through a request parsing, a data receiving from
 * client (handled by #MeloRTSPRead)  and end of connection (handled by
 * #MeloRTSPClose).
 * The @client_data can be used to attach a specific buffer to the current
 * connection and it will be kept until end of connection. If the value is set,
 * it must be freed in the #MeloRTSPClose callback implementation.
 */
typedef void (*MeloRTSPRequest) (MeloRTSPClient *client, MeloRTSPMethod method,
                                 const gchar *url, gpointer user_data,
                                 gpointer *client_data);

/**
 * MeloRTSPRead:
 * @client: the current RTSP client handle
 * @buffer: a pointer to the buffer to filled with body RTSP request
 * @size: size of the buffer in bytes
 * @last: set to %TRUE if it is last buffer (end of body)
 * @user_data: a pointer to the user data set with callback
 * @client_data: a pointer to the client data
 *
 * This callback is called when data is received from a client: @buffer is
 * filled with @size bytes of data received from the client and corresponding to
 * the body request data. This callback can be called several times, until the
 * end of the body is reached, signaled by @last.
 * The @client_data can be used to attach a specific buffer to the current
 * connection and it will be kept until end of connection. If the value is set,
 * it must be freed in the #MeloRTSPClose callback implementation.
 */
typedef void (*MeloRTSPRead) (MeloRTSPClient *client, guchar *buffer,
                              gsize size, gboolean last, gpointer user_data,
                              gpointer *client_data);

/**
 * MeloRTSPClose:
 * @client: the current RTSP client handle
 * @user_data: a pointer to the user data set with callback
 * @client_data: a pointer to the client data
 *
 * This callback is called at end of connection, after a request completion or
 * an abort / error.
 * If the @client_data has been set in previous callback (#MeloRTSPRequest
 * and/or #MeloRTSPRead) execution, it should be freed here.
 */
typedef void (*MeloRTSPClose) (MeloRTSPClient *client, gpointer user_data,
                               gpointer *client_data);

GType melo_rtsp_get_type (void);

MeloRTSP *melo_rtsp_new (void);

guint melo_rtsp_attach (MeloRTSP *rtsp, GMainContext *context);

gboolean melo_rtsp_start (MeloRTSP *rtsp, guint port);
void melo_rtsp_stop (MeloRTSP *rtsp);

void melo_rtsp_set_request_callback (MeloRTSP *rtsp, MeloRTSPRequest callback,
                                     gpointer user_data);
void melo_rtsp_set_read_callback (MeloRTSP *rtsp, MeloRTSPRead callback,
                                  gpointer user_data);
void melo_rtsp_set_close_callback (MeloRTSP *rtsp, MeloRTSPClose callback,
                                   gpointer user_data);

MeloRTSPMethod melo_rtsp_get_method (MeloRTSPClient *client);
const gchar *melo_rtsp_get_method_name (MeloRTSPClient *client);

const gchar *melo_rtsp_get_header (MeloRTSPClient *client, const gchar *name);
gsize melo_rtsp_get_content_length (MeloRTSPClient *client);

const guchar *melo_rtsp_get_ip (MeloRTSPClient *client);
const gchar *melo_rtsp_get_ip_string (MeloRTSPClient *client);
guint melo_rtsp_get_port (MeloRTSPClient *client);
const gchar *melo_rtsp_get_hostname (MeloRTSPClient *client);

const guchar *melo_rtsp_get_server_ip (MeloRTSPClient *client);
guint melo_rtsp_get_server_port (MeloRTSPClient *client);

gboolean melo_rtsp_init_response (MeloRTSPClient *client, guint code,
                                  const gchar *reason);
gboolean melo_rtsp_add_header (MeloRTSPClient *client, const gchar *name,
                               const gchar *value);
gboolean melo_rtsp_set_response (MeloRTSPClient *client, const gchar *response);
gboolean melo_rtsp_set_packet (MeloRTSPClient *client, guchar *buffer,
                               gsize len, GDestroyNotify free);

/* Authentication part */
gboolean melo_rtsp_basic_auth_check (MeloRTSPClient *client,
                                     const gchar *username,
                                     const gchar *password);
gboolean melo_rtsp_basic_auth_response (MeloRTSPClient *client,
                                        const gchar *realm);
gboolean melo_rtsp_digest_auth_check (MeloRTSPClient *client,
                                      const gchar *username,
                                      const gchar *password,
                                      const gchar *realm);
gboolean melo_rtsp_digest_auth_response (MeloRTSPClient *client,
                                         const gchar *realm,
                                         const gchar *opaque,
                                         gint signal_stale);

G_END_DECLS

#endif /* __MELO_RTSP_H__ */

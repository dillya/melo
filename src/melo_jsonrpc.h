/*
 * melo_json_rpc.h: JSON-RPC server helpers
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

#ifndef __MELO_JSONRPC_H__
#define __MELO_JSONRPC_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define MELO_TYPE_JSONRPC             (melo_jsonrpc_get_type ())
#define MELO_JSONRPC(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), MELO_TYPE_JSONRPC, MeloJSONRPC))
#define MELO_IS_JSONRPC(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MELO_TYPE_JSONRPC))
#define MELO_JSONRPC_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), MELO_TYPE_JSONRPC, MeloJSONRPCClass))
#define MELO_IS_JSONRPC_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), MELO_TYPE_JSONRPC))
#define MELO_JSONRPC_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), MELO_TYPE_JSONRPC, MeloJSONRPCClass))

typedef struct _MeloJSONRPC MeloJSONRPC;
typedef struct _MeloJSONRPCPrivate MeloJSONRPCPrivate;
typedef struct _MeloJSONRPCClass MeloJSONRPCClass;

/* JSON RPC error codes */
typedef enum {
  MELO_JSONRPC_ERROR_PARSE_ERROR = -32700,
  MELO_JSONRPC_ERROR_INVALID_REQUEST = -32600,
  MELO_JSONRPC_ERROR_METHOD_NOT_FOUND = -32601,
  MELO_JSONRPC_ERROR_INVALID_PARAMS = -32602,
  MELO_JSONRPC_ERROR_INTERNAL_ERROR = -32603,
  MELO_JSONRPC_ERROR_SERVER_ERROR = -32000,
} MeloJSONRPCError;

typedef void (*MeloJSONRPCCallback) (const char *method, GVariant *params,
                                     gboolean is_notification,
                                     gpointer user_data);

struct _MeloJSONRPC {
  GObject parent_instance;

  /*< private >*/
  MeloJSONRPCPrivate *priv;
};

struct _MeloJSONRPCClass {
  GObjectClass parent_class;
};

GType melo_jsonrpc_get_type (void);

MeloJSONRPC *melo_jsonrpc_new (void);
gboolean melo_jsonrpc_parse_request (MeloJSONRPC *self,
                                     const char *request, gsize length,
                                     MeloJSONRPCCallback callback,
                                     gpointer user_data);
void melo_jsonrpc_set_response (MeloJSONRPC *self, GVariant *variant);
void melo_jsonrpc_set_error (MeloJSONRPC *self, MeloJSONRPCError error_code,
                             const char *error_format,
                              ...) G_GNUC_PRINTF (3, 4);
char *melo_jsonrpc_get_response (MeloJSONRPC *self);

/* Utils */
char *melo_jsonrpc_build_error (const char *id, MeloJSONRPCError error_code,
                                const char *error_format,
                                ...) G_GNUC_PRINTF (3, 4);

G_END_DECLS

#endif /* __MELO_JSONRPC_H__ */

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
#include <libsoup/soup.h>

/* JSON RPC error codes */
typedef enum {
  MELO_JSONRPC_ERROR_PARSE_ERROR = -32700,
  MELO_JSONRPC_ERROR_INVALID_REQUEST = -32600,
  MELO_JSONRPC_ERROR_METHOD_NOT_FOUND = -32601,
  MELO_JSONRPC_ERROR_INVALID_PARAMS = -32602,
  MELO_JSONRPC_ERROR_INTERNAL_ERROR = -32603,
  MELO_JSONRPC_ERROR_SERVER_ERROR = -32000,
} MeloJSONRPCError;

char *melo_jsonrpc_build_error (const char *id, MeloJSONRPCError error_code,
                                const char *error_format,
                                ...) G_GNUC_PRINTF (3, 4);

void melo_jsonrpc_message_set_error (SoupMessage *msg, const char *id,
                                     MeloJSONRPCError error_code,
                                     const char *error_format,
                                     ...) G_GNUC_PRINTF (4, 5);

#endif /* __MELO_JSONRPC_H__ */

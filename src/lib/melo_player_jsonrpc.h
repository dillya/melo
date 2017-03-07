/*
 * melo_player_jsonrpc.h: Player base JSON-RPC interface
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

#ifndef __MELO_PLAYER_JSONRPC_H__
#define __MELO_PLAYER_JSONRPC_H__

#include "melo_player.h"
#include "melo_jsonrpc.h"

typedef enum {
  MELO_PLAYER_JSONRPC_INFO_FIELDS_NONE = 0,
  MELO_PLAYER_JSONRPC_INFO_FIELDS_NAME = 1,
  MELO_PLAYER_JSONRPC_INFO_FIELDS_PLAYLIST = 2,
  MELO_PLAYER_JSONRPC_INFO_FIELDS_CONTROLS = 4,

  MELO_PLAYER_JSONRPC_INFO_FIELDS_FULL = ~0,
} MeloPlayerJSONRPCInfoFields;

typedef enum {
  MELO_PLAYER_JSONRPC_STATUS_FIELDS_NONE = 0,
  MELO_PLAYER_JSONRPC_STATUS_FIELDS_STATE = 1,
  MELO_PLAYER_JSONRPC_STATUS_FIELDS_NAME = 2,
  MELO_PLAYER_JSONRPC_STATUS_FIELDS_POS = 4,
  MELO_PLAYER_JSONRPC_STATUS_FIELDS_DURATION = 8,
  MELO_PLAYER_JSONRPC_STATUS_FIELDS_PLAYLIST = 16,
  MELO_PLAYER_JSONRPC_STATUS_FIELDS_VOLUME = 32,
  MELO_PLAYER_JSONRPC_STATUS_FIELDS_MUTE = 64,
  MELO_PLAYER_JSONRPC_STATUS_FIELDS_TAGS = 128,

  MELO_PLAYER_JSONRPC_STATUS_FIELDS_FULL = ~0,
} MeloPlayerJSONRPCStatusFields;

MeloPlayerJSONRPCInfoFields melo_player_jsonrpc_get_info_fields (
                                                             JsonObject *obj,
                                                             const gchar *name);
JsonObject *melo_player_jsonrpc_info_to_object (
                                            const gchar *id,
                                            const MeloPlayerInfo *info,
                                            MeloPlayerJSONRPCInfoFields fields);
JsonObject * melo_player_jsonrpc_status_to_object (
                                           const MeloPlayerStatus *status,
                                           MeloPlayerJSONRPCStatusFields fields,
                                           MeloTagsFields tags_fields,
                                           gint64 tags_timestamp);
/* JSON-RPC methods */
void melo_player_jsonrpc_register_methods (void);
void melo_player_jsonrpc_unregister_methods (void);

#endif /* __MELO_PLAYER_JSONRPC_H__ */

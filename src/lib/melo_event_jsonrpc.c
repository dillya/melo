/*
 * melo_event_jsonrpc.c: Event JSON-RPC interface
 *
 * Copyright (C) 2017 Alexandre Dilly <dillya@sparod.com>
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

#include "melo_player_jsonrpc.h"

#include "melo_event_jsonrpc.h"

typedef void (*MeloEventJsonrpcParser) (JsonObject *obj, gpointer data);
typedef const gchar *(*MeloEventJsonrpcString) (guint event);

/* Player event parsers */
static void
melo_event_jsonrpc_player_new (JsonObject *obj, gpointer data)
{
  const MeloPlayerInfo *info;
  JsonObject *o;

  info = melo_event_player_new_parse (data);
  o = melo_player_jsonrpc_info_to_object (NULL, info,
                                          MELO_PLAYER_JSONRPC_INFO_FIELDS_FULL);
  if (o)
    json_object_set_object_member (obj, "info", o);
}

static void
melo_event_jsonrpc_player_status (JsonObject *obj, gpointer data)
{
  const MeloPlayerStatus *status = melo_event_player_status_parse (data);
  JsonObject *o = melo_player_jsonrpc_status_to_object (status,
                                         MELO_PLAYER_JSONRPC_STATUS_FIELDS_FULL,
                                         MELO_TAGS_FIELDS_FULL, 0);
  json_object_set_object_member (obj, "status", o);
}

static void
melo_event_jsonrpc_player_state (JsonObject *obj, gpointer data)
{
  MeloPlayerState state = melo_event_player_state_parse (data);
  json_object_set_string_member (obj, "state",
                                 melo_player_state_to_string (state));
}

static void
melo_event_jsonrpc_player_buffering (JsonObject *obj, gpointer data)
{
  MeloPlayerState state;
  guint percent;
  melo_event_player_buffering_parse (data, &state, &percent);
  json_object_set_string_member (obj, "state",
                                 melo_player_state_to_string (state));
  json_object_set_int_member (obj, "percent", percent);
}

static void
melo_event_jsonrpc_player_seek (JsonObject *obj, gpointer data)
{
  gint pos = melo_event_player_seek_parse (data);
  json_object_set_int_member (obj, "pos", pos);
}

static void
melo_event_jsonrpc_player_duration (JsonObject *obj, gpointer data)
{
  gint duration = melo_event_player_duration_parse (data);
  json_object_set_int_member (obj, "duration", duration);
}

static void
melo_event_jsonrpc_player_playlist (JsonObject *obj, gpointer data)
{
  gboolean has_prev, has_next;
  melo_event_player_playlist_parse (data, &has_prev, &has_next);
  json_object_set_boolean_member (obj, "has_prev", has_prev);
  json_object_set_boolean_member (obj, "has_next", has_next);
}

static void
melo_event_jsonrpc_player_volume (JsonObject *obj, gpointer data)
{
  gdouble volume = melo_event_player_volume_parse (data);
  json_object_set_double_member (obj, "volume", volume);
}

static void
melo_event_jsonrpc_player_mute (JsonObject *obj, gpointer data)
{
  gboolean mute = melo_event_player_mute_parse (data);
  json_object_set_boolean_member (obj, "mute", mute);
}

static void
melo_event_jsonrpc_player_name (JsonObject *obj, gpointer data)
{
  const gchar *name = melo_event_player_name_parse (data);
  json_object_set_string_member (obj, "name", name);
}

static void
melo_event_jsonrpc_player_error (JsonObject *obj, gpointer data)
{
  const gchar *error = melo_event_player_error_parse (data);
  json_object_set_string_member (obj, "error", error);
}

static void
melo_event_jsonrpc_player_tags (JsonObject *obj, gpointer data)
{
  MeloTags *tags = melo_event_player_tags_parse (data);
  JsonObject *o = melo_tags_to_json_object (tags, MELO_TAGS_FIELDS_FULL);
  json_object_set_object_member (obj, "tags", o);
}

static MeloEventJsonrpcParser melo_event_jsonrpc_player_parsers[] = {
  [MELO_EVENT_PLAYER_NEW] = melo_event_jsonrpc_player_new,
  [MELO_EVENT_PLAYER_DELETE] = NULL,
  [MELO_EVENT_PLAYER_STATUS] = melo_event_jsonrpc_player_status,
  [MELO_EVENT_PLAYER_STATE] = melo_event_jsonrpc_player_state,
  [MELO_EVENT_PLAYER_BUFFERING] = melo_event_jsonrpc_player_buffering,
  [MELO_EVENT_PLAYER_SEEK] = melo_event_jsonrpc_player_seek,
  [MELO_EVENT_PLAYER_DURATION] = melo_event_jsonrpc_player_duration,
  [MELO_EVENT_PLAYER_PLAYLIST] = melo_event_jsonrpc_player_playlist,
  [MELO_EVENT_PLAYER_VOLUME] = melo_event_jsonrpc_player_volume,
  [MELO_EVENT_PLAYER_MUTE] = melo_event_jsonrpc_player_mute,
  [MELO_EVENT_PLAYER_NAME] = melo_event_jsonrpc_player_name,
  [MELO_EVENT_PLAYER_ERROR] = melo_event_jsonrpc_player_error,
  [MELO_EVENT_PLAYER_TAGS] = melo_event_jsonrpc_player_tags,
};

/* Melo event type persers */
static MeloEventJsonrpcParser *melo_event_jsonrpc_parsers[] = {
  [MELO_EVENT_TYPE_GENERAL] = NULL,
  [MELO_EVENT_TYPE_MODULE] = NULL,
  [MELO_EVENT_TYPE_BROWSER] = NULL,
  [MELO_EVENT_TYPE_PLAYER] = melo_event_jsonrpc_player_parsers,
  [MELO_EVENT_TYPE_PLAYLIST] = NULL,
};

static MeloEventJsonrpcString melo_event_jsonrpc_strings[] = {
  [MELO_EVENT_TYPE_GENERAL] = NULL,
  [MELO_EVENT_TYPE_MODULE] = NULL,
  [MELO_EVENT_TYPE_BROWSER] = NULL,
  [MELO_EVENT_TYPE_PLAYER] = melo_event_player_to_string,
  [MELO_EVENT_TYPE_PLAYLIST] = NULL,
};

JsonObject *
melo_event_jsonrpc_evnet_to_object (MeloEventType type, guint event,
                                    const gchar *id, gpointer data)
{
  const gchar *event_string = NULL;
  JsonObject *obj;

  /* Create new object */
  obj = json_object_new ();
  if (!obj)
    return NULL;

  /* Add type and event strings */
  json_object_set_string_member (obj, "type", melo_event_type_to_string (type));
  if (melo_event_jsonrpc_strings[type])
    event_string = melo_event_jsonrpc_strings[type] (event);
  json_object_set_string_member (obj, "event", event_string);

  /* Add id string */
  json_object_set_string_member (obj, "id", id);

  /* Parse event and add members to current object */
  if (melo_event_jsonrpc_parsers[type] &&
      melo_event_jsonrpc_parsers[type][event])
    melo_event_jsonrpc_parsers[type][event] (obj, data);

  return obj;
}

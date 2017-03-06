/*
 * melo_event.c: Event dispatcher
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

#include "melo_event.h"

/* Event client list */
G_LOCK_DEFINE_STATIC (melo_event_mutex);
static GList *melo_event_clients = NULL;

struct _MeloEventClient {
  MeloEventCallback callback;
  gpointer user_data;
};

typedef struct {
  MeloPlayerState state;
  guint percent;
} MeloEventPlayerBuffering;

typedef struct {
  gboolean has_prev;
  gboolean has_next;
} MeloEventPlayerPlaylist;

MeloEventClient *
melo_event_register (MeloEventCallback callback, gpointer user_data)
{
  MeloEventClient *client;

  /* Create new client context */
  client = g_slice_new0 (MeloEventClient);
  if (!client)
    return NULL;

  /* Fill client context */
  client->callback = callback;
  client->user_data = user_data;

  /* Add client to list */
  G_LOCK (melo_event_mutex);
  melo_event_clients = g_list_prepend (melo_event_clients, client);
  G_UNLOCK (melo_event_mutex);

  return client;
}

void
melo_event_unregister (MeloEventClient *client)
{
  /* Remove from list */
  G_LOCK (melo_event_mutex);
  melo_event_clients = g_list_remove (melo_event_clients, client);
  G_UNLOCK (melo_event_mutex);

  /* Free client */
  g_slice_free (MeloEventClient, client);
}

static const gchar *melo_event_type_string[] = {
  [MELO_EVENT_TYPE_GENERAL] = "general",
  [MELO_EVENT_TYPE_MODULE] = "module",
  [MELO_EVENT_TYPE_BROWSER] = "browser",
  [MELO_EVENT_TYPE_PLAYER] = "player",
  [MELO_EVENT_TYPE_PLAYLIST] = "playlist",
};

const gchar *
melo_event_type_to_string (MeloEventType type)
{
  if (type < MELO_EVENT_TYPE_COUNT)
    return melo_event_type_string[type];
  return NULL;
}

void
melo_event_new (MeloEventType type, guint event, const gchar *id, gpointer data,
                GDestroyNotify free_data_func)
{
  GList *l;

  /* Lock client list */
  G_LOCK (melo_event_mutex);

  /* Send event to all registered clients */
  for (l = melo_event_clients; l != NULL; l = l->next) {
    MeloEventClient *client = (MeloEventClient *) l->data;

    /* Call event callback */
    client->callback(client, type, event, id, data, client->user_data);
  }

  /* Unlock client list */
  G_UNLOCK (melo_event_mutex);

  /* Free event data */
  if (free_data_func)
    free_data_func (data);
}

#define melo_event_player(event, id, data, free) \
  melo_event_new (MELO_EVENT_TYPE_PLAYER, MELO_EVENT_PLAYER_##event, id, data, \
                  free)
inline void
melo_event_player_new (const gchar *id, const MeloPlayerInfo *info)
{
  melo_event_player (NEW, id, (gpointer) info, NULL);
}

inline void
melo_event_player_delete (const gchar *id)
{
  melo_event_player (DELETE, id, NULL, NULL);
}

inline void
melo_event_player_status (const gchar *id, MeloPlayerStatus *status)
{
  melo_event_player (STATUS, id, status,
                     (GDestroyNotify) melo_player_status_unref);
}

inline void
melo_event_player_state (const gchar *id, MeloPlayerState state)
{
  melo_event_player (STATE, id, &state, NULL);
}

inline void
melo_event_player_buffering (const gchar *id, MeloPlayerState state,
                             guint percent)
{
  MeloEventPlayerBuffering evt = { .state = state, .percent = percent };
  melo_event_player (BUFFERING, id, &evt, NULL);
}

inline void
melo_event_player_seek (const gchar *id, gint pos)
{
  melo_event_player (SEEK, id, GINT_TO_POINTER (pos), NULL);
}

inline void
melo_event_player_duration (const gchar *id, gint duration)
{
  melo_event_player (DURATION, id, GINT_TO_POINTER (duration), NULL);
}

inline void
melo_event_player_playlist (const gchar *id, gboolean has_prev,
                            gboolean has_next)
{
  MeloEventPlayerPlaylist evt = { .has_prev = has_prev, .has_next = has_next };
  melo_event_player (PLAYLIST, id, &evt, NULL);
}

inline void
melo_event_player_volume (const gchar *id, gdouble volume)
{
  melo_event_player (VOLUME, id, &volume, NULL);
}

inline void
melo_event_player_mute (const gchar *id, gboolean mute)
{
  melo_event_player (MUTE, id, &mute, NULL);
}

inline void
melo_event_player_name (const gchar *id, const gchar *name)
{
  melo_event_player (NAME, id, (gpointer) name, NULL);
}

inline void
melo_event_player_error (const gchar *id, const gchar *error)
{
  melo_event_player (ERROR, id, (gpointer) error, NULL);
}

inline void
melo_event_player_tags (const gchar *id, MeloTags *tags)
{
  melo_event_player (TAGS, id, tags, NULL);
}

inline const MeloPlayerInfo *
melo_event_player_new_parse (gpointer data)
{
  return (const MeloPlayerInfo *) data;
}

inline MeloPlayerStatus *
melo_event_player_status_parse (gpointer data)
{
  return (MeloPlayerStatus *) data;
}

inline MeloPlayerState
melo_event_player_state_parse (gpointer data)
{
  return *((MeloPlayerState *) data);
}

inline void
melo_event_player_buffering_parse (gpointer data, MeloPlayerState *state,
                                   guint *percent)
{
  MeloEventPlayerBuffering *evt = (MeloEventPlayerBuffering *) data;
  if (state)
    *state = evt->state;
  if (percent)
    *percent = evt->percent;
}

inline gint
melo_event_player_seek_parse (gpointer data)
{
  return GPOINTER_TO_INT (data);
}

inline gint
melo_event_player_duration_parse (gpointer data)
{
  return GPOINTER_TO_INT (data);
}

inline void
melo_event_player_playlist_parse (gpointer data, gboolean *has_prev,
                                  gboolean *has_next)
{
  MeloEventPlayerPlaylist *evt = (MeloEventPlayerPlaylist *) data;
  if (has_prev)
    *has_prev = evt->has_prev;
  if (has_next)
    *has_next = evt->has_next;
}

inline gdouble
melo_event_player_volume_parse (gpointer data)
{
  return *((gdouble *) data);
}

inline gboolean
melo_event_player_mute_parse (gpointer data)
{
  return *((gboolean *) data);
}

inline const gchar *
melo_event_player_name_parse (gpointer data)
{
  return (const gchar *) data;
}

inline const gchar *
melo_event_player_error_parse (gpointer data)
{
  return (const gchar *) data;
}

inline MeloTags *
melo_event_player_tags_parse (gpointer data)
{
  return (MeloTags *) data;
}

static const gchar *melo_event_player_string[] = {
  [MELO_EVENT_PLAYER_NEW] = "new",
  [MELO_EVENT_PLAYER_DELETE] = "delete",
  [MELO_EVENT_PLAYER_STATUS] = "status",
  [MELO_EVENT_PLAYER_STATE] = "state",
  [MELO_EVENT_PLAYER_BUFFERING] = "buffering",
  [MELO_EVENT_PLAYER_SEEK] = "seek",
  [MELO_EVENT_PLAYER_DURATION] = "duration",
  [MELO_EVENT_PLAYER_PLAYLIST] = "playlist",
  [MELO_EVENT_PLAYER_VOLUME] = "volume",
  [MELO_EVENT_PLAYER_MUTE] = "mute",
  [MELO_EVENT_PLAYER_NAME] = "name",
  [MELO_EVENT_PLAYER_ERROR] = "error",
  [MELO_EVENT_PLAYER_TAGS] = "tags",
};

const gchar *
melo_event_player_to_string (MeloEventPlayer event)
{
  if (event < MELO_EVENT_PLAYER_COUNT)
    return melo_event_player_string[event];
  return NULL;
}

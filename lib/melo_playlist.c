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

#define MELO_LOG_TAG "playlist"
#include "melo/melo_log.h"

#include "melo/melo_tags.h"
#include "melo_events.h"
#include "melo_player_priv.h"
#include "melo_requests.h"

#include "playlist.pb-c.h"

#include "melo/melo_playlist.h"

#define DEFAULT_PLAYLIST_ID "default"

/* Global playlist list */
G_LOCK_DEFINE_STATIC (melo_playlist_mutex);
static GHashTable *melo_playlist_list;

/* Playlist event listeners */
static MeloEvents melo_playlist_events;

/* Current playlist */
static MeloPlaylist *melo_playlist_current;

typedef struct _MeloPlaylistList {
  MeloPlaylistEntry *list;
  unsigned int count;

  MeloPlaylistEntry *current;
  unsigned int current_index;
} MeloPlaylistList;

struct _MeloPlaylistEntry {
  atomic_int ref_count;
  MeloPlaylist *playlist;

  char *player;
  char *path;
  char *name;
  MeloTags *tags;

  union {
    MeloPlaylistList sub_medias;
    MeloPlaylistEntry *parent;
  };

  MeloPlaylistEntry *prev;
  MeloPlaylistEntry *next;
};

struct _MeloPlaylist {
  /* Parent instance */
  GObject parent_instance;

  char *id;
  MeloPlaylistList entries;

  MeloEvents events;
  MeloRequests requests;
};

G_DEFINE_TYPE (MeloPlaylist, melo_playlist, G_TYPE_OBJECT)

/* Melo playlist properties */
enum { PROP_0, PROP_ID };

static void melo_playlist_constructed (GObject *object);
static void melo_playlist_finalize (GObject *object);

static void melo_playlist_set_property (
    GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void melo_playlist_get_property (
    GObject *object, guint property_id, GValue *value, GParamSpec *pspec);

static void
melo_playlist_class_init (MeloPlaylistClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  /* Override constructed to capture ID and register the playlist */
  object_class->constructed = melo_playlist_constructed;

  /* Override finalize to un-register the playlist and free private data */
  object_class->finalize = melo_playlist_finalize;

  /* Override properties accessors */
  object_class->set_property = melo_playlist_set_property;
  object_class->get_property = melo_playlist_get_property;

  /**
   * MeloPlaylist:id:
   *
   * The unique ID of the playlist. This must be set during the construct and it
   * can be only read after instantiation. The value provided at construction
   * will be used to register the playlist in the global list which will be
   * exported to user interface.
   * The NULL ID is reserved for temporary / unnamed playlist.
   */
  g_object_class_install_property (object_class, PROP_ID,
      g_param_spec_string ("id", "ID", "Playlist unique ID.",
          DEFAULT_PLAYLIST_ID,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
}

static void
melo_playlist_init (MeloPlaylist *self)
{
}

/**
 * melo_playlist_new:
 *
 * Creates a new #MeloPlaylist.
 *
 * Returns: (transfer full): the new #MeloPlaylist.
 */
MeloPlaylist *
melo_playlist_new (const char *id)
{
  /* Set default name */
  if (!id)
    id = DEFAULT_PLAYLIST_ID;

  return g_object_new (MELO_TYPE_PLAYLIST, "id", id, NULL);
}

static void
melo_playlist_constructed (GObject *object)
{
  MeloPlaylist *playlist = MELO_PLAYLIST (object);

  /* Register playlist */
  if (playlist->id) {
    gboolean added = FALSE;

    /* Lock playlist list */
    G_LOCK (melo_playlist_mutex);

    /* Create playlist list */
    if (!melo_playlist_list)
      melo_playlist_list = g_hash_table_new (g_str_hash, g_str_equal);

    /* Insert playlist into list */
    if (melo_playlist_list &&
        g_hash_table_contains (melo_playlist_list, playlist->id) == FALSE) {
      g_hash_table_insert (
          melo_playlist_list, (gpointer) playlist->id, playlist);
      added = TRUE;
    }

    /* Default playlist as current */
    if (!melo_playlist_current && !strcmp (playlist->id, DEFAULT_PLAYLIST_ID))
      melo_playlist_current = playlist;

    /* Unlock playlist list */
    G_UNLOCK (melo_playlist_mutex);

    /* playlist added */
    if (added == TRUE)
      MELO_LOGI ("playlist '%s' added", playlist->id);
    else
      MELO_LOGW ("failed to add playlist '%s' to global list", playlist->id);
  }

  /* Chain constructed */
  G_OBJECT_CLASS (melo_playlist_parent_class)->constructed (object);
}

static void
melo_playlist_finalize (GObject *object)
{
  MeloPlaylist *playlist = MELO_PLAYLIST (object);
  MeloPlaylistList *entries = &playlist->entries;
  gboolean removed = FALSE;

  /* Free playlist list */
  while (entries->list) {
    MeloPlaylistEntry *entry = entries->list;

    /* Remove from list */
    entries->list = entry->next;

    /* Release entry */
    melo_playlist_entry_unref (entry);
  }

  /* Un-register playlist */
  if (playlist->id) {
    /* Lock playlist list */
    G_LOCK (melo_playlist_mutex);

    /* Remove playlist from list */
    if (melo_playlist_list) {
      g_hash_table_remove (melo_playlist_list, playlist->id);
      removed = TRUE;
    }

    /* Destroy playlist list */
    if (melo_playlist_list && !g_hash_table_size (melo_playlist_list)) {
      g_hash_table_destroy (melo_playlist_list);
      melo_playlist_list = NULL;
    }

    /* Unlock playlist list */
    G_UNLOCK (melo_playlist_mutex);

    /* playlist removed */
    if (removed == TRUE)
      MELO_LOGI ("playlist '%s' removed", playlist->id);

    /* Free ID */
    g_free (playlist->id);
  }

  /* Chain finalize */
  G_OBJECT_CLASS (melo_playlist_parent_class)->finalize (object);
}

static void
melo_playlist_set_property (
    GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
  MeloPlaylist *playlist = MELO_PLAYLIST (object);

  switch (property_id) {
  case PROP_ID:
    g_free (playlist->id);
    playlist->id = g_value_dup_string (value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
melo_playlist_get_property (
    GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
  MeloPlaylist *playlist = MELO_PLAYLIST (object);

  switch (property_id) {
  case PROP_ID:
    g_value_set_string (value, playlist->id);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static MeloPlaylistEntry *
melo_playlist_list_nth (MeloPlaylistList *list, unsigned int index)
{
  MeloPlaylistEntry *entry = list->list;

  if (!index)
    return entry;

  while (index-- && entry)
    entry = entry->next;

  return entry;
}

static inline MeloPlaylistEntry *
melo_playlist_list_get_last (MeloPlaylistList *list)
{
  return list->list ? list->list->prev : NULL;
}

static inline bool
melo_playlist_list_is_first (MeloPlaylistList *list, MeloPlaylistEntry *entry)
{
  return list->list == entry;
}

static inline bool
melo_playlist_list_is_last (MeloPlaylistList *list, MeloPlaylistEntry *entry)
{
  return list->list && list->list->prev == entry;
}

static void
melo_playlist_list_prepend (MeloPlaylistList *list, MeloPlaylistEntry *entry)
{
  entry->prev = list->list ? list->list->prev : entry;
  entry->next = list->list;
  if (list->list)
    list->list->prev = entry;
  list->list = entry;
  list->count++;
}

static bool
melo_playlist_list_get_index (
    MeloPlaylistList *list, MeloPlaylistEntry *entry, unsigned int *idx)
{
  MeloPlaylistEntry *e = list->list;
  unsigned int i;

  for (i = 0; i < list->count && e; i++, e = e->next) {
    if (entry == e) {
      *idx = i;
      return true;
    }
  }

  return false;
}

static void
melo_playlist_list_clear (MeloPlaylistList *list)
{
  while (list->list) {
    MeloPlaylistEntry *entry = list->list;
    list->list = entry->next;
    melo_playlist_entry_unref (entry);
  }
  list->count = 0;
  list->current = NULL;
  list->current_index = 0;
}

static void
melo_playlist_message_tags (MeloTags *tags, Tags__Tags *media_tags)
{
  media_tags->title = (char *) melo_tags_get_title (tags);
  media_tags->artist = (char *) melo_tags_get_artist (tags);
  media_tags->album = (char *) melo_tags_get_album (tags);
  media_tags->genre = (char *) melo_tags_get_genre (tags);
  media_tags->track = melo_tags_get_track (tags);
  media_tags->cover = (char *) melo_tags_get_cover (tags);
}

static void
melo_playlist_message_sub_medias (
    MeloPlaylistList *sub_medias, Playlist__Media *media)
{
  Playlist__SubMedia **list_ptr = NULL;
  Playlist__SubMedia *list = NULL;
  Tags__Tags *tags = NULL;
  MeloPlaylistEntry *e;
  unsigned int i;

  /* No need to add sub-media list */
  if (!sub_medias->count || !sub_medias->list)
    return;

  /* Allocate buffer to store media list and tags */
  list_ptr = malloc (sizeof (*list_ptr) * sub_medias->count);
  list = malloc (sizeof (*list) * sub_medias->count);
  tags = malloc (sizeof (*tags) * sub_medias->count);

  /* Set list */
  media->n_sub_medias = sub_medias->count;
  media->sub_medias = list_ptr;

  /* Fill list */
  for (e = sub_medias->list, i = 0; e != NULL; e = e->next, i++) {

    /* Set sub-media */
    playlist__sub_media__init (&list[i]);
    list[i].name = (char *) e->name;
    list[i].parent = media->index;
    tags__tags__init (&tags[i]);
    if (e->tags)
      melo_playlist_message_tags (e->tags, &tags[i]);
    list[i].tags = &tags[i];
    list_ptr[i] = &list[i];
  }
}

static MeloMessage *
melo_playlist_message_media (
    unsigned int idx, MeloPlaylistEntry *entry, bool add)
{
  Playlist__Event pmsg = PLAYLIST__EVENT__INIT;
  Playlist__Media media = PLAYLIST__MEDIA__INIT;
  Tags__Tags media_tags = TAGS__TAGS__INIT;
  const char *name = entry->name;
  MeloTags *tags = entry->tags;
  MeloMessage *msg;

  /* Set event type */
  if (add) {
    pmsg.event_case = PLAYLIST__EVENT__EVENT_ADD;
    pmsg.add = &media;
  } else {
    pmsg.event_case = PLAYLIST__EVENT__EVENT_UPDATE;
    pmsg.update = &media;
  }

  /* Set media */
  media.index = idx;
  media.name = (char *) name;

  /* Set tags */
  if (tags) {
    melo_playlist_message_tags (tags, &media_tags);
    media.tags = &media_tags;
  }

  /* Add sub-medias */
  if (add)
    melo_playlist_message_sub_medias (&entry->sub_medias, &media);

  /* Generate message */
  msg = melo_message_new (playlist__event__get_packed_size (&pmsg));
  if (msg)
    melo_message_set_size (
        msg, playlist__event__pack (&pmsg, melo_message_get_data (msg)));

  /* Destroy allocated memory to handle sub-medias */
  if (add && media.sub_medias) {
    free (media.sub_medias[0]->tags);
    free (media.sub_medias[0]);
    free (media.sub_medias);
  }

  return msg;
}

static MeloMessage *
melo_playlist_message_sub_media (
    unsigned int idx, unsigned int parent, MeloPlaylistEntry *entry, bool add)
{
  Playlist__Event pmsg = PLAYLIST__EVENT__INIT;
  Playlist__SubMedia sub_media = PLAYLIST__SUB_MEDIA__INIT;
  Tags__Tags media_tags = TAGS__TAGS__INIT;
  MeloMessage *msg;

  /* Set event type */
  if (add) {
    pmsg.event_case = PLAYLIST__EVENT__EVENT_ADD_SUB;
    pmsg.add_sub = &sub_media;
  } else {
    pmsg.event_case = PLAYLIST__EVENT__EVENT_UPDATE_SUB;
    pmsg.update_sub = &sub_media;
  }

  /* Set media */
  sub_media.index = idx;
  sub_media.parent = parent;
  sub_media.name = (char *) entry->name;

  /* Set tags */
  if (entry->tags) {
    melo_playlist_message_tags (entry->tags, &media_tags);
    sub_media.tags = &media_tags;
  }

  /* Generate message */
  msg = melo_message_new (playlist__event__get_packed_size (&pmsg));
  if (msg)
    melo_message_set_size (
        msg, playlist__event__pack (&pmsg, melo_message_get_data (msg)));

  return msg;
}

static MeloMessage *
melo_playlist_message_play (unsigned int index, int sub_index)
{
  Playlist__Event pmsg = PLAYLIST__EVENT__INIT;
  Playlist__MediaIndex idx = PLAYLIST__MEDIA_INDEX__INIT;
  MeloMessage *msg;

  /* Set event type */
  pmsg.event_case = PLAYLIST__EVENT__EVENT_PLAY;

  /* Set current index */
  idx.index = index;
  idx.sub_index = sub_index;
  pmsg.play = &idx;

  /* Generate message */
  msg = melo_message_new (playlist__event__get_packed_size (&pmsg));
  if (msg)
    melo_message_set_size (
        msg, playlist__event__pack (&pmsg, melo_message_get_data (msg)));

  return msg;
}

/*
 * Static functions
 */

static MeloPlaylist *
melo_playlist_get_by_id (const char *id)
{
  MeloPlaylist *playlist = NULL;

  /* Lock playlist list */
  G_LOCK (melo_playlist_mutex);

  if (!id)
    playlist =
        melo_playlist_current ? g_object_ref (melo_playlist_current) : NULL;
  else if (melo_playlist_list)
    playlist = g_hash_table_lookup (melo_playlist_list, id);
  if (playlist)
    g_object_ref (playlist);

  /* Unlock playlist list */
  G_UNLOCK (melo_playlist_mutex);

  return playlist;
}

/**
 * melo_playlist_add_event_listener:
 * @id: the id of a #MeloPlaylist or %NULL for global / current playlist
 * @cb: the function to call when a new event occurred
 * @user_data: data to pass to @cb
 *
 * This function will add the @cb and @user_data to internal list of event
 * listener and the @cb will be called when a new event occurs.
 *
 * If @id is %NULL, the event listener will be added to the global / current
 * playlist class, otherwise, the event listener will be added to a specific
 * playlist instance, pointed by its ID. If the playlist doesn't exist, %false
 * will be returned.
 *
 * When a new global listener is added, the current playlist is sent to it.
 *
 * The @cb / @user_data couple is used to identify an event listener, so a
 * second call to this function with the same parameters will fails. You must
 * use the same couple in melo_playlist_remove_event_listener() to remove this
 * event listener.
 *
 * Returns: %true if the listener has been added successfully, %false otherwise.
 */
bool
melo_playlist_add_event_listener (
    const char *id, MeloAsyncCb cb, void *user_data)
{
  bool ret;

  /* Add event listener to global / current playlist or specific playlist */
  if (id) {
    MeloPlaylist *playlist;

    /* Find playlist */
    playlist = melo_playlist_get_by_id (id);
    if (!playlist)
      return false;

    /* Add event listener to playlist */
    ret = melo_events_add_listener (&playlist->events, cb, user_data);
    g_object_unref (playlist);
  } else {
    /* Add event listener to global / current playlist */
    ret = melo_events_add_listener (&melo_playlist_events, cb, user_data);
  }

  return ret;
}

/**
 * melo_playlist_remove_event_listener:
 * @id: the id of a #MeloPlaylist or %NULL for global / current playlist
 * @cb: the function to remove
 * @user_data: data passed with @cb during add
 *
 * This function will remove the @cb and @user_data from internal list of event
 * listener and the @cb won't be called anymore after the function returns.
 *
 * Returns: %true if the listener has been removed successfully, %false
 *     otherwise.
 */
bool
melo_playlist_remove_event_listener (
    const char *id, MeloAsyncCb cb, void *user_data)
{
  bool ret;

  if (id) {
    MeloPlaylist *playlist;

    /* Find playlist */
    playlist = melo_playlist_get_by_id (id);
    if (!playlist)
      return false;

    /* Remove event listener to playlist */
    ret = melo_events_remove_listener (&playlist->events, cb, user_data);
    g_object_unref (playlist);
  } else
    ret = melo_events_remove_listener (&melo_playlist_events, cb, user_data);

  return ret;
}

static void
melo_playlist_update_player_control (MeloPlaylistList *entries)
{
  MeloPlaylistEntry *current = entries->current;
  bool prev = false, next = false;

  if (current) {
    MeloPlaylistEntry *sub_current = current->sub_medias.current;

    if (current->next || (sub_current && sub_current->next))
      prev = true;
    if (!melo_playlist_list_is_first (entries, current) ||
        (sub_current &&
            !melo_playlist_list_is_first (&current->sub_medias, sub_current)))
      next = true;
  }

  /* Update player controls */
  melo_player_update_playlist_controls (prev, next);
}

static bool
melo_playlist_get_media_list (MeloPlaylist *playlist,
    Playlist__Request__GetMediaList *req, MeloAsyncData *async)
{
  Playlist__Response resp = PLAYLIST__RESPONSE__INIT;
  Playlist__Response__MediaList media_list =
      PLAYLIST__RESPONSE__MEDIA_LIST__INIT;
  Playlist__MediaIndex media_index = PLAYLIST__MEDIA_INDEX__INIT;
  Playlist__Media **medias_ptr = NULL;
  Playlist__Media *medias = NULL;
  Tags__Tags *tags = NULL;
  MeloPlaylistList *entries = &playlist->entries;
  MeloMessage *msg;
  unsigned int i;

  /* Set response type */
  resp.resp_case = PLAYLIST__RESPONSE__RESP_MEDIA_LIST;
  resp.media_list = &media_list;

  /* Set current index */
  media_index.index = entries->current_index;
  if (entries->current && entries->current->sub_medias.current)
    media_index.sub_index = entries->current->sub_medias.current_index;
  else
    media_index.sub_index = -1;
  media_list.current = &media_index;

  /* Generate media list */
  if (req->offset < entries->count) {
    MeloPlaylistEntry *e;

    /* Set message */
    media_list.offset = req->offset;
    media_list.count = entries->count - req->offset;

    /* Allocate buffer to store media list and tags */
    medias_ptr = malloc (sizeof (*medias_ptr) * media_list.count);
    medias = malloc (sizeof (*medias) * media_list.count);
    tags = malloc (sizeof (*tags) * media_list.count);

    /* Set list */
    media_list.n_medias = media_list.count;
    media_list.medias = medias_ptr;

    /* Fill list */
    for (e = melo_playlist_list_nth (entries, req->offset), i = 0; e != NULL;
         e = e->next, i++) {

      /* Set media */
      playlist__media__init (&medias[i]);
      medias[i].name = (char *) e->name;
      if (e->tags) {
        tags__tags__init (&tags[i]);
        melo_playlist_message_tags (e->tags, &tags[i]);
        medias[i].tags = &tags[i];
      }
      media_list.medias[i] = &medias[i];

      /* Add sub-medias */
      melo_playlist_message_sub_medias (&e->sub_medias, &medias[i]);
    }
  } else {
    /* Set default message */
    media_list.offset = entries->count;
    media_list.count = 0;
  }

  /* Generate message */
  msg = melo_message_new (playlist__response__get_packed_size (&resp));
  if (msg)
    melo_message_set_size (
        msg, playlist__response__pack (&resp, melo_message_get_data (msg)));

  /* Destroy allocated memory to handle sub-medias */
  for (i = 0; i < media_list.count; i++) {
    if (media_list.medias[i] && media_list.medias[i]->sub_medias) {
      free (media_list.medias[i]->sub_medias[0]->tags);
      free (media_list.medias[i]->sub_medias[0]);
      free (media_list.medias[i]->sub_medias);
    }
  }

  /* Free buffer */
  free (medias_ptr);
  free (medias);
  free (tags);

  /* Send message */
  if (async->cb)
    async->cb (msg, async->user_data);

  /* Release message */
  melo_message_unref (msg);

  return true;
}

static bool
melo_playlist_get_current (MeloPlaylist *playlist, MeloAsyncData *async)
{
  Playlist__Response resp = PLAYLIST__RESPONSE__INIT;
  Playlist__MediaIndex idx = PLAYLIST__MEDIA_INDEX__INIT;
  MeloPlaylistList *entries = &playlist->entries;
  MeloMessage *msg;

  /* Set response type */
  resp.resp_case = PLAYLIST__RESPONSE__RESP_CURRENT;

  /* Set current media index */
  idx.index = entries->current_index;
  if (entries->current && entries->current->sub_medias.current)
    idx.sub_index = entries->current->sub_medias.current_index;
  else
    idx.sub_index = -1;
  resp.current = &idx;

  /* Generate message */
  msg = melo_message_new (playlist__response__get_packed_size (&resp));
  if (msg)
    melo_message_set_size (
        msg, playlist__response__pack (&resp, melo_message_get_data (msg)));

  /* Send message */
  if (async->cb)
    async->cb (msg, async->user_data);

  /* Release message */
  melo_message_unref (msg);

  return true;
}

static bool
melo_playlist_play (MeloPlaylist *playlist, unsigned int index, int sub_index)
{
  MeloPlaylistList *entries = &playlist->entries;
  MeloPlaylistEntry *entry, *media = NULL;

  /* Find entry */
  entry = melo_playlist_list_nth (entries, index);
  if (!entry)
    return false;

  /* Invalid sub-index */
  if (sub_index >= 0 && !entry->sub_medias.list)
    return false;

  /* Find media */
  if (sub_index < 0) {
    media = melo_playlist_list_get_last (&entry->sub_medias);
    if (media) {
      if (!media->path) {
        melo_playlist_list_clear (&entry->sub_medias);
        media = NULL;
      } else
        sub_index = entry->sub_medias.count - 1;
    }
  } else if (entry->sub_medias.list) {
    media = melo_playlist_list_nth (&entry->sub_medias, sub_index);
    if (!media || !media->path)
      return false;
  }

  /* Switch playlist */
  G_LOCK (melo_playlist_mutex);
  if (playlist != melo_playlist_current)
    melo_playlist_current = playlist;
  G_UNLOCK (melo_playlist_mutex);

  /* Play this media */
  entries->current = entry;
  entries->current_index = index;
  if (media) {
    entry->sub_medias.current = media;
    entry->sub_medias.current_index = sub_index;
  } else
    media = entry;
  melo_player_play_media (entry->player, media->path, media->name,
      melo_tags_ref (media->tags), melo_playlist_entry_ref (media));

  /* Broadcast media addition */
  melo_events_broadcast (
      &melo_playlist_events, melo_playlist_message_play (index, sub_index));

  /* Update player controls */
  melo_playlist_update_player_control (entries);

  return true;
}

static bool
melo_playlist_move (
    MeloPlaylist *playlist, Playlist__Move *req, MeloAsyncData *async)
{
  return false;
}

static bool
melo_playlist_delete (
    MeloPlaylist *playlist, Playlist__Range *req, MeloAsyncData *async)
{
  return false;
}

/**
 * melo_playlist_handle_request:
 * @id: the id of the playlist or %NULL for current playlist
 * @msg: the #MeloMessage to handle
 * @cb: the function to call when a response is sent
 * @user_data: data to pass to @cb
 *
 * This function is called when a new playlist request is received by the
 * application. This function will handle the request for a playlist pointed by
 * @id, or the current playlist if @id is NULL.

 * If the request is malformed or an internal error occurs, the function will
 * return %false, otherwise %true will be returned.
 * After returning, many asynchronous tasks related to the request can still be
 * pending, so @cb and @user_data should not be destroyed. If the request need
 * to stopped / cancelled, melo_playlist_cancel_request() is intended for.
 *
 * Returns: %true if the message has been handled by the playlist, %false
 *     otherwise.
 */
bool
melo_playlist_handle_request (
    const char *id, MeloMessage *msg, MeloAsyncCb cb, void *user_data)
{
  MeloAsyncData async = {
      .cb = cb,
      .user_data = user_data,
  };
  Playlist__Request *request;
  MeloPlaylist *playlist;
  bool ret = false;

  if (!msg)
    return false;

  /* Find playlist */
  playlist = melo_playlist_get_by_id (id);
  if (!playlist)
    return false;

  /* Unpack request */
  request = playlist__request__unpack (
      NULL, melo_message_get_size (msg), melo_message_get_cdata (msg, NULL));
  if (!request) {
    MELO_LOGE ("failed to unpack request");
    g_object_unref (playlist);
    return false;
  }

  /* Handle request */
  switch (request->req_case) {
  case PLAYLIST__REQUEST__REQ_GET_MEDIA_LIST:
    ret = melo_playlist_get_media_list (
        playlist, request->get_media_list, &async);
    break;
  case PLAYLIST__REQUEST__REQ_GET_CURRENT:
    ret = melo_playlist_get_current (playlist, &async);
    break;
  case PLAYLIST__REQUEST__REQ_PLAY:
    if (request->play)
      ret = melo_playlist_play (
          playlist, request->play->index, request->play->sub_index);
    break;
  case PLAYLIST__REQUEST__REQ_MOVE:
    ret = melo_playlist_move (playlist, request->move, &async);
    break;
  case PLAYLIST__REQUEST__REQ_DELETE:
    ret = melo_playlist_delete (playlist, request->delete_, &async);
    break;
  default:
    MELO_LOGE ("request %u not supported", request->req_case);
  }

  /* Free request */
  playlist__request__free_unpacked (request, NULL);

  /* Release playlist */
  g_object_unref (playlist);

  /* End of request */
  if (ret && cb)
    cb (NULL, user_data);

  return ret;
}

/**
 * melo_playlist_cancel_request:
 * @id: the id of the playlist or %NULL for current playlist
 * @cb: the function used during call to melo_playlist_handle_request()
 * @user_data: data passed with @cb
 *
 * This function can be called to cancel a running or a pending request. If the
 * request exists, the asynchronous tasks will be cancelled and @cb will be
 * called with a NULL-message to signal end of request. If the request is
 * already finished or a cancellation is already pending, this function will do
 * nothing.
 */
void
melo_playlist_cancel_request (const char *id, MeloAsyncCb cb, void *user_data)
{
  MeloAsyncData async = {
      .cb = cb,
      .user_data = user_data,
  };
  MeloPlaylist *playlist;

  /* Find playlist */
  playlist = melo_playlist_get_by_id (id);
  if (!playlist)
    return;

  /* Cancel / stop request */
  melo_requests_cancel_request (&playlist->requests, &async);
  g_object_unref (playlist);
}

static bool
melo_playlist_handle_entry (MeloPlaylistEntry *entry, bool play)
{
  MeloPlaylistList *entries;
  MeloPlaylist *playlist;
  int sub_index = -1;

  /* Invalid entry */
  if (!entry || !entry->player) {
    MELO_LOGE ("invalid playlist entry to %s: no player ID set",
        play ? "play" : "add");
    return false;
  }

  /* Get current playlist */
  G_LOCK (melo_playlist_mutex);
  playlist =
      melo_playlist_current ? g_object_ref (melo_playlist_current) : NULL;
  G_UNLOCK (melo_playlist_mutex);

  /* No playlist set */
  if (!playlist) {
    MELO_LOGE ("no default playlist set");
    return false;
  }

  /* Add media item to list */
  entries = &playlist->entries;
  melo_playlist_list_prepend (entries, entry);
  entry->playlist = playlist;

  /* Broadcast media addition */
  melo_events_broadcast (
      &melo_playlist_events, melo_playlist_message_media (0, entry, true));

  /* Play media */
  if (play) {
    MeloPlaylistEntry *media;

    /* Set current entry */
    entries->current = entry;
    entries->current_index = 0;

    /* Select sub-media */
    if (!entry->path && entry->sub_medias.list) {
      media = melo_playlist_list_get_last (&entry->sub_medias);
      entry->sub_medias.current = media;
      entry->sub_medias.current_index = entry->sub_medias.count - 1;
      sub_index = entry->sub_medias.current_index;
    } else
      media = entry;

    melo_player_play_media (entry->player, media->path, media->name,
        melo_tags_ref (media->tags), melo_playlist_entry_ref (media));
  } else
    entries->current_index++;

  /* Broadcast media playing */
  if (play)
    melo_events_broadcast (&melo_playlist_events,
        melo_playlist_message_play (entries->current_index, sub_index));

  /* Update player controls */
  melo_playlist_update_player_control (entries);

  /* Release playlist reference */
  g_object_unref (playlist);

  return true;
}

/**
 * melo_playlist_add_media:
 * @player_id: the ID of the media player to use
 * @path: the media path / URI / URL to add
 * @name: the display name
 * @tags: (nullable): the media tags
 *
 * This function will add the media to the current playlist. It takes the
 * ownership of @tags.
 *
 * Returns: %true if the media has been sent added to the current playlist,
 *     %false otherwise.
 */
bool
melo_playlist_add_media (
    const char *player_id, const char *path, const char *name, MeloTags *tags)
{
  MeloPlaylistEntry *entry;

  /* Create entry and add */
  entry = melo_playlist_entry_new (player_id, path, name, tags);
  if (!entry || !melo_playlist_handle_entry (entry, false)) {
    melo_playlist_entry_unref (entry);
    return false;
  }

  return true;
}

/**
 * melo_playlist_play_media:
 * @player_id: the ID of the media player to use
 * @path: the media path / URI / URL to add and play
 * @name: the display name
 * @tags: (nullable): the media tags
 *
 * This function will add the media to the current playlist and will play it on
 * the player pointed by @player_id. It takes the ownership of @tags.
 *
 * Returns: %true if the media has been sent added to the current playlist,
 *     %false otherwise.
 */
bool
melo_playlist_play_media (
    const char *player_id, const char *path, const char *name, MeloTags *tags)
{
  MeloPlaylistEntry *entry;

  /* Create entry and play */
  entry = melo_playlist_entry_new (player_id, path, name, tags);
  if (!entry || !melo_playlist_handle_entry (entry, true)) {
    melo_playlist_entry_unref (entry);
    return false;
  }

  return true;
}

/**
 * melo_playlist_add_entry:
 * @entry: the entry to add
 *
 * This function will add the entry to the current playlist. It takes the
 * ownership of @entry.
 *
 * Returns: %true if the entry has been sent added to the current playlist,
 *     %false otherwise.
 */
bool
melo_playlist_add_entry (MeloPlaylistEntry *entry)
{
  return melo_playlist_handle_entry (entry, false);
}

/**
 * melo_playlist_play_entry:
 * @entry: the entry to add and play
 *
 * This function will add the entry to the current playlist and will play it on
 * the player pointed by the entry. It takes the ownership of @entry.
 *
 * Returns: %true if the entry has been sent added to the current playlist,
 *     %false otherwise.
 */
bool
melo_playlist_play_entry (MeloPlaylistEntry *entry)
{
  return melo_playlist_handle_entry (entry, true);
}

/**
 * melo_playlist_entry_new:
 * @player_id: the ID of the media player to use
 * @path: (nullable): the media path / URI / URL
 * @name: the display name
 * @tags: (nullable): the media tags
 *
 * Creates a new #MeloPlaylistEntry which will contain a media to hold in the
 * playlist. The newly created entry should be added to the playlist with
 * melo_playlist_add_entry() or melo_playlist_play_entry().
 *
 * Returns: (transfer full): a new #MeloPlaylistEntry.
 */
MeloPlaylistEntry *
melo_playlist_entry_new (
    const char *player_id, const char *path, const char *name, MeloTags *tags)
{
  MeloPlaylistEntry *entry;

  /* Find a name from path */
  if (!name && path) {
    name = strrchr (path, '/');
    if (name)
      name++;
  }

  /* Allocate new entry */
  entry = calloc (1, sizeof (*entry));
  if (!entry)
    return false;

  /* Initialize reference counter */
  atomic_init (&entry->ref_count, 1);

  /* Fill entry */
  entry->player = g_strdup (player_id);
  entry->path = g_strdup (path);
  entry->name = g_strdup (name);
  entry->tags = tags;

  return entry;
}

/**
 * melo_playlist_entry_ref:
 * @entry: a #MeloPlaylistEntry
 *
 * Increase the reference count on @entry.
 *
 * Returns: the #MeloPlaylistEntry.
 */
MeloPlaylistEntry *
melo_playlist_entry_ref (MeloPlaylistEntry *entry)
{
  if (!entry)
    return NULL;

  atomic_fetch_add (&entry->ref_count, 1);

  return entry;
}

/**
 * melo_playlist_entry_unref:
 * @entry: (nullable): a #MeloPlaylistEntry
 *
 * Releases a reference on @entry. This may result in the entry being freed.
 */
void
melo_playlist_entry_unref (MeloPlaylistEntry *entry)
{
  int ref_count;

  if (!entry)
    return;

  ref_count = atomic_fetch_sub (&entry->ref_count, 1);
  if (ref_count == 1) {
    /* Clear sub-medias list */
    if (entry->player)
      melo_playlist_list_clear (&entry->sub_medias);

    melo_tags_unref (entry->tags);
    g_free (entry->name);
    g_free (entry->path);
    g_free (entry->player);
    free (entry);
  } else if (ref_count < 1)
    MELO_LOGC ("negative reference counter %d", ref_count - 1);
}

/**
 * melo_playlist_entry_update:
 * @entry: (nullable): a #MeloPlaylistEntry
 * @name: (nullable): the display name
 * @tags: (nullable): the media tags
 * @reset: replace the name and tags instead of merge
 *
 * The current entry will be updated with the @name and @tags provided in
 * parameters. If @reset is set to %true, the name and tags will be replaced,
 * otherwise, the name will not be replaces and tags will be merged with current
 * one thanks to melo_tags_merge().
 *
 * Returns: %true if the entry has been updated successfully, %false otherwise.
 */
bool
melo_playlist_entry_update (
    MeloPlaylistEntry *entry, const char *name, MeloTags *tags, bool reset)
{
  MeloPlaylist *playlist;
  MeloMessage *msg = NULL;
  unsigned int index;

  if (!entry)
    return false;

  /* Replace tags */
  if (reset) {
    melo_tags_unref (entry->tags);
    entry->tags = tags;
  } else
    entry->tags =
        melo_tags_merge (tags, entry->tags, MELO_TAGS_MERGE_FLAG_NONE);

  /* Replace name */
  if (reset && entry->name != name) {
    g_free (entry->name);
    entry->name = g_strdup (name);
  }

  /* Get playlist */
  playlist = entry->player ? entry->playlist : entry->parent->playlist;
  if (!playlist)
    return true;

  /* Generate entry update event */
  if (entry->player) {
    if (melo_playlist_list_get_index (&playlist->entries, entry, &index))
      msg = melo_playlist_message_media (index, entry, false);
  } else {
    unsigned int sub_index;
    if (melo_playlist_list_get_index (
            &playlist->entries, entry->parent, &index) &&
        melo_playlist_list_get_index (
            &entry->parent->sub_medias, entry, &sub_index))
      msg = melo_playlist_message_sub_media (sub_index, index, entry, false);
  }

  /* Broadcast message */
  if (msg) {
    /* Broadcast entry update */
    melo_events_broadcast (&playlist->events, melo_message_ref (msg));

    /* Broadcast entry update to current playlist */
    if (melo_playlist_current == playlist)
      melo_events_broadcast (&melo_playlist_events, msg);
    else
      melo_message_unref (msg);
  }

  return true;
}

/**
 * melo_playlist_entry_add_media:
 * @entry: (nullable): a #MeloPlaylistEntry
 * @path: (nullable): the media path / URI / URL
 * @name: the display name
 * @tags: (nullable): the media tags
 * @ref: (nullable): a pointer to store a reference of the media
 *
 * This function can be used to add a sub-media to an existing playlist entry.
 * The media will be added on top of the current list. If @path is set to %NULL,
 * the newly added media will not be playable.
 * This function takes the ownership of @tags.
 *
 * Returns: %true if the media has been added to the entry successfully, %false
 *     otherwise.
 */
bool
melo_playlist_entry_add_media (MeloPlaylistEntry *entry, const char *path,
    const char *name, MeloTags *tags, MeloPlaylistEntry **ref)
{
  MeloPlaylistEntry *media;
  MeloPlaylist *playlist;
  unsigned int index;

  /* Cannot add entry to invalid entry */
  if (!entry || !entry->player) {
    MELO_LOGE ("cannot add media to entry");
    return false;
  }

  /* Allocate new entry to hold media */
  media = melo_playlist_entry_new (NULL, path, name, tags);
  if (!media) {
    MELO_LOGE ("failed to add media to entry");
    return false;
  }

  /* Add parent */
  media->parent = entry;

  /* Export media reference */
  if (ref)
    *ref = melo_playlist_entry_ref (media);

  /* Add media item to entry */
  melo_playlist_list_prepend (&entry->sub_medias, media);
  entry->sub_medias.current_index = 0;
  entry->sub_medias.current = media;

  /* Get playlist */
  playlist = entry->playlist;
  if (!playlist)
    return true;

  /* Find index of parent */
  if (melo_playlist_list_get_index (&playlist->entries, entry, &index)) {
    MeloMessage *msg;

    /* Generate media add event */
    msg = melo_playlist_message_sub_media (0, index, media, true);
    if (msg) {
      /* Broadcast entry update */
      melo_events_broadcast (&playlist->events, melo_message_ref (msg));

      /* Broadcast entry update to current playlist */
      if (melo_playlist_current == playlist)
        melo_events_broadcast (&melo_playlist_events, msg);
      else
        melo_message_unref (msg);
    }
  }

  return true;
}

static bool
melo_playlist_handle_control (bool next)
{
  MeloPlaylistList *entries;
  MeloPlaylistEntry *entry, *media;
  MeloPlaylist *playlist;
  int sub_index = -1;

  /* Get current playlist */
  G_LOCK (melo_playlist_mutex);
  playlist =
      melo_playlist_current ? g_object_ref (melo_playlist_current) : NULL;
  G_UNLOCK (melo_playlist_mutex);

  /* No playlist set */
  if (!playlist) {
    MELO_LOGE ("no default playlist set");
    return false;
  }

  /* No current playing media */
  entries = &playlist->entries;
  entry = playlist->entries.current;
  if (!entry) {
    g_object_unref (playlist);
    return false;
  }

  /* Find next / previous media */
  do {
    MeloPlaylistList *sub_medias = &entry->sub_medias;
    MeloPlaylistEntry *sub_current = sub_medias->current;

    /* Select entry and sub-media to play */
    if (next) {
      if (sub_current &&
          !melo_playlist_list_is_first (sub_medias, sub_current) &&
          sub_current->prev) {
        sub_medias->current = sub_current->prev;
        sub_medias->current_index--;
        sub_index = sub_medias->current_index;
        media = sub_medias->current;
      } else if (!melo_playlist_list_is_first (entries, entry) && entry->prev) {
        entries->current = entry->prev;
        entries->current_index--;
        media = entries->current;
      } else
        return false;
    } else {
      if (sub_current &&
          !melo_playlist_list_is_last (sub_medias, sub_current) &&
          sub_current->next) {
        sub_medias->current = sub_current->next;
        sub_medias->current_index++;
        sub_index = sub_medias->current_index;
        media = sub_medias->current;
      } else if (!melo_playlist_list_is_last (entries, entry) && entry->next) {
        entries->current = entry->next;
        entries->current_index++;
        media = entries->current;
      } else
        return false;
    }
    entry = entries->current;

    /* Reset sub-medias current position */
    if (entry == media && entry->sub_medias.list) {
      if (entry->sub_medias.list->path) {
        media = melo_playlist_list_get_last (&entry->sub_medias);
        sub_index = entry->sub_medias.count - 1;
        entry->sub_medias.current = media;
      } else {
        melo_playlist_list_clear (&entry->sub_medias);
        sub_index = -1;
      }
      entry->sub_medias.current_index = sub_index;
    }
  } while (media->path == NULL);

  /* Broadcast media playing */
  melo_events_broadcast (&melo_playlist_events,
      melo_playlist_message_play (entries->current_index, sub_index));

  /* Play previous / next media in playlist */
  melo_player_play_media (entry->player, media->path, media->name,
      melo_tags_ref (media->tags), melo_playlist_entry_ref (media));

  /* Update player controls */
  melo_playlist_update_player_control (entries);

  /* Release playlist reference */
  g_object_unref (playlist);

  return true;
}

/**
 * melo_playlist_play_previous:
 *
 * The current player is stopped and the previous media in playlist is sent to
 * its player for playing. If no previous media is found in the playlist, the
 * functions will return %false.
 *
 * Returns: %true if the previous media in playlist is now playing, %false
 *    otherwise.
 */
bool
melo_playlist_play_previous (void)
{
  return melo_playlist_handle_control (false);
}

/**
 * melo_playlist_play_next:
 *
 * The current player is stopped and the next media in playlist is sent to its
 * player for playing. If no next media is found in the playlist, the functions
 * will return %false.
 *
 * Returns: %true if the next media in playlist is now playing, %false
 *     otherwise.
 */
bool
melo_playlist_play_next (void)
{
  return melo_playlist_handle_control (true);
}

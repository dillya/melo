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
#define PLAYLIST_MAX_DEPTH 10

typedef struct _MeloPlaylistBackup MeloPlaylistBackup;

/* Global playlist list */
G_LOCK_DEFINE_STATIC (melo_playlist_mutex);
static GHashTable *melo_playlist_list;

/* Playlist event listeners */
static MeloEvents melo_playlist_events;

/* Current playlist */
static MeloPlaylist *melo_playlist_current;

typedef enum _MeloPlaylistFlag {
  MELO_PLAYLIST_FLAG_NONE = 0,
  MELO_PLAYLIST_FLAG_PLAYABLE = (1 << 0),
  MELO_PLAYLIST_FLAG_SORTABLE = (1 << 1),
  MELO_PLAYLIST_FLAG_SHUFFLE_INSERTED = (1 << 2),
  MELO_PLAYLIST_FLAG_SHUFFLE_ADDED = (1 << 3),
  MELO_PLAYLIST_FLAG_SHUFFLE_DELETED = (1 << 4),
} MeloPlaylistFlag;

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
  unsigned int flags;

  MeloPlaylistEntry *parent;
  MeloPlaylistList children;

  MeloPlaylistEntry *prev;
  MeloPlaylistEntry *next;
};

struct _MeloPlaylistBackup {
  unsigned int count;
  struct {
    MeloPlaylistEntry *entry;
    MeloPlaylistBackup *children;
  } list[];
};

struct _MeloPlaylist {
  /* Parent instance */
  GObject parent_instance;

  char *id;
  MeloPlaylistList entries;
  MeloPlaylistBackup *shuffle;

  MeloEvents events;
  MeloRequests requests;
};

G_DEFINE_TYPE (MeloPlaylist, melo_playlist, G_TYPE_OBJECT)

/* Melo playlist properties */
enum { PROP_0, PROP_ID };

static void melo_playlist_backup_restore (MeloPlaylistBackup *list,
    MeloPlaylistList *entries, MeloPlaylistEntry *parent, bool remove);

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
  while (entries->count--) {
    MeloPlaylistEntry *entry = entries->list;

    /* Remove from list */
    entries->list = entry->next;

    /* Release entry */
    melo_playlist_entry_unref (entry);
  }

  /* Free shuffle */
  if (playlist->shuffle)
    melo_playlist_backup_restore (
        playlist->shuffle, &playlist->entries, NULL, true);

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
  else if (index == list->count - 1)
    return entry->prev;
  else if (index >= list->count)
    return NULL;

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
  if (list->list) {
    entry->prev = list->list->prev;
    entry->next = list->list;
    list->list->prev->next = entry;
    list->list->prev = entry;
    if (list->current)
      list->current_index++;
  } else
    entry->prev = entry->next = entry;
  list->list = entry;
  list->count++;
}

static void
melo_playlist_list_append (MeloPlaylistList *list, MeloPlaylistEntry *entry)
{
  if (list->list) {
    entry->prev = list->list->prev;
    entry->next = list->list;
    list->list->prev->next = entry;
    list->list->prev = entry;
  } else {
    entry->prev = entry->next = entry;
    list->list = entry;
  }
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
  while (list->count--) {
    MeloPlaylistEntry *entry = list->list;
    list->list = entry->next;
    melo_playlist_entry_unref (entry);
  }
  list->list = NULL;
  list->count = 0;
  list->current = NULL;
  list->current_index = 0;
}

static bool
melo_playlist_extract_entry (MeloPlaylist *playlist, MeloPlaylistEntry *entry,
    unsigned int idx, unsigned int *count, MeloPlaylistEntry **current)
{
  MeloPlaylistList *list;
  MeloPlaylistEntry *end;
  unsigned int len = 1;

  /* Get list */
  list = entry->parent ? &entry->parent->children : &playlist->entries;

  /* Find last entry */
  for (end = entry;
       len < *count && end && !melo_playlist_list_is_last (list, end);
       end = end->next)
    len++;
  if (!end)
    return false;

  /* Remove sub-list from list */
  entry->prev->next = end->next;
  end->next->prev = entry->prev;
  if (entry == list->list)
    list->list = entry != entry->next ? entry->next : NULL;
  list->count -= len;

  /* Update current index */
  if (list->current_index >= idx) {
    if (list->current_index <= idx) {
      *current = list->current;
      list->current = NULL;
    } else
      list->current_index -= len;
  }

  /* Finalize sub-list */
  entry->prev = end;
  end->next = entry;
  *count = len;

  return true;
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
melo_playlist_message_current (
    Playlist__MediaIndex *idx, MeloPlaylist *playlist)
{
  MeloPlaylistEntry *e = playlist->entries.current;
  MeloPlaylistList *list = &playlist->entries;
  unsigned int i;

  /* No current */
  if (!e)
    return;

  /* Count current indices */
  idx->n_indices = 1;
  while (e->children.current) {
    idx->n_indices++;
    e = e->children.current;
  }

  /* Allocate indices */
  idx->indices = malloc (sizeof (*idx->indices) * idx->n_indices);
  if (!idx->indices) {
    idx->n_indices = 0;
    return;
  }

  /* Fill array */
  for (i = 0; i < idx->n_indices; i++) {
    idx->indices[i] = list ? list->current_index : -1;
    list = &list->current->children;
  }
}

static void
melo_playlist_message_indices (
    Playlist__MediaIndex *idx, MeloPlaylistEntry *entry)
{
  MeloPlaylistEntry *e = entry;
  unsigned int count = 0;

  /* Count current indices */
  while (e->parent) {
    count++;
    e = e->parent;
  }

  /* Allocate indices */
  idx->indices = malloc (sizeof (*idx->indices) * count);
  if (!idx->indices) {
    return;
  }
  idx->n_indices = count;

  /* Fill array */
  entry = entry->parent;
  while (count-- && entry) {
    MeloPlaylistList *entries =
        entry->parent ? &entry->parent->children : &entry->playlist->entries;
    melo_playlist_list_get_index (entries, entry, &idx->indices[count]);
    entry = entry->parent;
  }
}

static void
melo_playlist_message_children_add (
    Playlist__Media *media, MeloPlaylistList *children)
{
  Playlist__Media **list_ptr = NULL;
  Playlist__Media *list = NULL;
  Tags__Tags *tags = NULL;
  MeloPlaylistEntry *e;
  unsigned int i;

  /* No children to add */
  if (!children->count || !children->list)
    return;

  /* Allocate buffer to store media list and tags */
  list_ptr = malloc (sizeof (*list_ptr) * children->count);
  list = malloc (sizeof (*list) * children->count);
  tags = malloc (sizeof (*tags) * children->count);

  /* Set list */
  media->n_children = children->count;
  media->children = list_ptr;

  /* Fill list */
  for (e = children->list, i = 0; i < children->count; e = e->next, i++) {
    /* Set media */
    playlist__media__init (&list[i]);
    list[i].name = (char *) e->name;
    list_ptr[i] = &list[i];

    /* Set doable flags */
    list[i].playable = e->flags & MELO_PLAYLIST_FLAG_PLAYABLE;
    list[i].sortable = e->flags & MELO_PLAYLIST_FLAG_SORTABLE;

    /* Set tags */
    tags__tags__init (&tags[i]);
    if (e->tags)
      melo_playlist_message_tags (e->tags, &tags[i]);
    list[i].tags = &tags[i];

    /* Add recursively children */
    melo_playlist_message_children_add (&list[i], &e->children);
  }
}

static void
melo_playlist_message_children_remove (Playlist__Media *media)
{
  unsigned int i;

  /* No children in current media */
  if (!media->children)
    return;

  /* Free recursively children */
  for (i = 0; i < media->n_children; i++)
    melo_playlist_message_children_remove (media->children[i]);

  /* Free children */
  free (media->children[0]->tags);
  free (media->children[0]);
  free (media->children);
}

static MeloMessage *
melo_playlist_message_media (MeloPlaylistEntry *entry, bool add)
{
  Playlist__Event pmsg = PLAYLIST__EVENT__INIT;
  Playlist__Media media = PLAYLIST__MEDIA__INIT;
  Playlist__MediaIndex parent = PLAYLIST__MEDIA_INDEX__INIT;
  Tags__Tags media_tags = TAGS__TAGS__INIT;
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
  melo_playlist_list_get_index (
      entry->parent ? &entry->parent->children : &entry->playlist->entries,
      entry, &media.index);
  melo_playlist_message_indices (&parent, entry);
  media.parent = &parent;
  media.name = (char *) entry->name;

  /* Set doable flags */
  media.playable = entry->flags & MELO_PLAYLIST_FLAG_PLAYABLE;
  media.sortable = entry->flags & MELO_PLAYLIST_FLAG_SORTABLE;

  /* Set tags */
  if (entry->tags) {
    melo_playlist_message_tags (entry->tags, &media_tags);
    media.tags = &media_tags;
  }

  /* Add children */
  if (add)
    melo_playlist_message_children_add (&media, &entry->children);

  /* Generate message */
  msg = melo_message_new (playlist__event__get_packed_size (&pmsg));
  if (msg)
    melo_message_set_size (
        msg, playlist__event__pack (&pmsg, melo_message_get_data (msg)));

  /* Destroy children */
  if (add)
    melo_playlist_message_children_remove (&media);

  /* Free parent */
  free (parent.indices);

  return msg;
}

static MeloMessage *
melo_playlist_message_play (MeloPlaylist *playlist)
{
  Playlist__Event pmsg = PLAYLIST__EVENT__INIT;
  Playlist__MediaIndex idx = PLAYLIST__MEDIA_INDEX__INIT;
  MeloMessage *msg;

  /* Set event type */
  pmsg.event_case = PLAYLIST__EVENT__EVENT_PLAY;

  /* Set current indices */
  melo_playlist_message_current (&idx, playlist);
  pmsg.play = &idx;

  /* Generate message */
  msg = melo_message_new (playlist__event__get_packed_size (&pmsg));
  if (msg)
    melo_message_set_size (
        msg, playlist__event__pack (&pmsg, melo_message_get_data (msg)));
  free (idx.indices);

  return msg;
}

static MeloMessage *
melo_playlist_message_shuffle (MeloPlaylist *playlist)
{
  Playlist__Event pmsg = PLAYLIST__EVENT__INIT;
  MeloMessage *msg;

  /* Set event type */
  pmsg.event_case = PLAYLIST__EVENT__EVENT_SHUFFLE;
  pmsg.shuffle = playlist->shuffle ? true : false;

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
melo_playlist_update_player_control (MeloPlaylist *playlist)
{
  MeloPlaylistList *entries = &playlist->entries;
  MeloPlaylistEntry *current = entries->current;
  bool prev = false, next = false;

  while (current) {
    MeloPlaylistEntry *child = current->children.current;

    if (!melo_playlist_list_is_last (entries, current) ||
        (child && !melo_playlist_list_is_last (&current->children, child)))
      prev = true;
    if (!melo_playlist_list_is_first (entries, current) ||
        (child && !melo_playlist_list_is_first (&current->children, child)))
      next = true;

    if (prev && next)
      break;

    entries = &current->children;
    current = child;
  }

  /* Update player controls */
  melo_player_update_playlist_controls (prev, next, playlist->shuffle);
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
  melo_playlist_message_current (&media_index, playlist);
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
    for (e = melo_playlist_list_nth (entries, req->offset), i = 0;
         i < media_list.count; e = e->next, i++) {

      /* Set media */
      playlist__media__init (&medias[i]);
      medias[i].index = req->offset + i;
      medias[i].name = (char *) e->name;
      media_list.medias[i] = &medias[i];

      /* Set doable flags */
      medias[i].playable = e->flags & MELO_PLAYLIST_FLAG_PLAYABLE;
      medias[i].sortable = e->flags & MELO_PLAYLIST_FLAG_SORTABLE;

      /* Set tags */
      if (e->tags) {
        tags__tags__init (&tags[i]);
        melo_playlist_message_tags (e->tags, &tags[i]);
        medias[i].tags = &tags[i];
      }

      /* Add children */
      melo_playlist_message_children_add (&medias[i], &e->children);
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

  /* Destroy children */
  for (i = 0; i < media_list.count; i++)
    melo_playlist_message_children_remove (media_list.medias[i]);

  /* Free buffer */
  free (medias_ptr);
  free (medias);
  free (tags);

  /* Free indices */
  free (media_index.indices);

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
  MeloMessage *msg;

  /* Set response type */
  resp.resp_case = PLAYLIST__RESPONSE__RESP_CURRENT;

  /* Set current media index */
  melo_playlist_message_current (&idx, playlist);
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

static MeloPlaylistEntry *
melo_playlist_get_entry (MeloPlaylist *playlist, unsigned int *indices,
    unsigned int count, MeloPlaylistEntry **parent)
{
  MeloPlaylistList *es = &playlist->entries;
  MeloPlaylistEntry *entry = NULL, *p;
  unsigned int i;

  if (!parent)
    parent = &p;

  /* Find entry */
  for (i = 0; i < count; i++) {
    *parent = entry;
    if (indices[i] < 0 || indices[i] > es->count)
      return NULL;
    entry = melo_playlist_list_nth (es, indices[i]);
    if (!entry)
      return NULL;
    es = &entry->children;
  }

  return entry;
}

static void
melo_playlist_reset_current (MeloPlaylist *playlist)
{
  MeloPlaylistList *entries = &playlist->entries;
  MeloPlaylistEntry *entry = entries->current;

  /* Reset current */
  while (entry) {
    entries->current = NULL;
    entries = &entry->children;
    entry = entries->current;
  }
}

static bool
melo_playlist_play (MeloPlaylist *playlist, unsigned int *indices,
    unsigned int count, MeloPlaylistEntry *entry, bool limit)
{
  MeloPlaylistList *entries;
  MeloPlaylistEntry *e;
  unsigned int depth = count;

  /* No indices */
  if (!count)
    return false;

  /* Find entry to play */
  if (!entry) {
    entry = melo_playlist_get_entry (playlist, indices, count, NULL);
    if (!entry)
      return false;
  }

  /* Move to last child */
  while ((e = melo_playlist_list_get_last (&entry->children))) {
    depth++;
    entry = e;
  }

  /* Update current index */
  while (!entry->player) {
    /* No parent: cannot play */
    if (!entry->parent) {
      /* First / last item: finished */
      if (melo_playlist_list_is_first (&playlist->entries, entry) ||
          melo_playlist_list_is_last (&playlist->entries, entry)) {
        melo_playlist_reset_current (playlist);
        melo_player_reset ();
        goto end;
      } else
        return false;
    }

    /* Limit reached */
    if (limit && count >= depth)
      return false;

    /* Move to parent */
    entry = entry->parent;
    depth--;

    /* Clear children list */
    melo_playlist_list_clear (&entry->children);
  }

  /* Switch playlist */
  G_LOCK (melo_playlist_mutex);
  if (playlist != melo_playlist_current)
    melo_playlist_current = playlist;
  G_UNLOCK (melo_playlist_mutex);

  /* Update parents indices */
  e = entry;
  while (e) {
    MeloPlaylistEntry *parent = e->parent;
    entries = parent ? &parent->children : &playlist->entries;

    /* Update current entry */
    melo_playlist_list_get_index (entries, e, &entries->current_index);
    entries->current = e;

    /* Move to next parent */
    e = parent;
  }

  /* Play media */
  melo_player_play_media (entry->player, entry->path, entry->name,
      melo_tags_ref (entry->tags), melo_playlist_entry_ref (entry));

end:
  /* Broadcast media addition */
  melo_events_broadcast (
      &melo_playlist_events, melo_playlist_message_play (playlist));

  /* Update player controls */
  melo_playlist_update_player_control (playlist);

  return true;
}

static bool
melo_playlist_play_recursive (MeloPlaylist *playlist, MeloPlaylistList *entries,
    MeloPlaylistEntry *entry, unsigned int *indices, unsigned int count,
    bool next)
{
  bool ret;

  /* Move to next / previous entry */
  if (next) {
    /* Try playing until success */
    do {
      /* Move to parent */
      while (melo_playlist_list_is_first (entries, entry)) {
        if (!entry->parent || !count)
          return false;
        entry = entry->parent;
        entries = entry->parent ? &entry->parent->children : &playlist->entries;
        count--;
      }
      entry = entry->prev;
      indices[count - 1]--;

      /* Try to play media */
      ret = melo_playlist_play (playlist, indices, count, entry, true);
    } while (!ret);
  } else {
    /* Try playing until success */
    do {
      /* Move to parent */
      while (melo_playlist_list_is_last (entries, entry)) {
        if (!entry->parent || !count)
          return false;
        entry = entry->parent;
        entries = entry->parent ? &entry->parent->children : &playlist->entries;
        count--;
      }
      entry = entry->next;
      indices[count - 1]++;

      /* Try to play media */
      ret = melo_playlist_play (playlist, indices, count, entry, true);
    } while (!ret);
  }

  return true;
}

static MeloPlaylistEntry *
melo_playlist_extract (MeloPlaylist *playlist, Playlist__Range *range,
    unsigned int *count, MeloPlaylistEntry **current)
{
  MeloPlaylistEntry *entry = NULL;

  /* Extract range of entries */
  if (range->linear) {
    /* No entry to extract */
    if (!range->first || !range->first->n_indices)
      return NULL;

    /* Find first entry to extract */
    entry = melo_playlist_get_entry (
        playlist, range->first->indices, range->first->n_indices, NULL);
    if (!entry)
      return entry;

    /* Extract entries */
    *count = range->length;
    if (!melo_playlist_extract_entry (playlist, entry,
            range->first->indices[range->first->n_indices - 1], count, current))
      return NULL;
  } else {
    MeloPlaylistEntry **map, *cur = NULL;
    unsigned int i, len = 0, cur_depth = 0;

    /* Allocate temporary entry map */
    map = malloc (sizeof (*map) * range->n_list);
    if (!map)
      return NULL;

    /* Populate map with entries */
    for (i = 0; i < range->n_list; i++) {
      /* No entry */
      if (!range->list[i] || !range->list[i]->n_indices) {
        map[i] = NULL;
        continue;
      }

      /* Add entry to map */
      map[i] = melo_playlist_get_entry (
          playlist, range->list[i]->indices, range->list[i]->n_indices, NULL);
    }

    /* Extract entries */
    for (i = 0; i < range->n_list; i++) {
      /* No entry to extract */
      if (!map[i])
        continue;

      /* Extract entry */
      *count = 1;
      if (!melo_playlist_extract_entry (playlist, map[i],
              range->list[i]->indices[range->list[i]->n_indices - 1], count,
              &cur))
        continue;
      len += *count;

      /* Current */
      if (cur && cur_depth < range->list[i]->n_indices) {
        *current = cur;
        cur_depth = range->list[i]->n_indices;
      }

      /* Append to list */
      if (entry) {
        map[i]->prev = entry->prev;
        entry->prev->next = map[i];
        entry->prev = map[i];
        map[i]->next = entry;
      } else
        entry = map[i];
    }
    *count = len;

    /* Free map */
    free (map);
  }

  return entry;
}

static bool
melo_playlist_move (
    MeloPlaylist *playlist, Playlist__Move *req, MeloAsyncData *async)
{
  Playlist__Event event = PLAYLIST__EVENT__INIT;
  MeloPlaylistEntry *parent = NULL, *current = NULL;
  MeloPlaylistEntry *list, *entry;
  MeloPlaylistList *entries;
  unsigned int count, i;
  MeloMessage *msg;

  /* No destination */
  if (!req->dest || !req->dest->n_indices)
    return false;

  /* Extract entries to temporary list */
  list = melo_playlist_extract (playlist, req->range, &count, &current);
  if (!list)
    return false;

  /* Find destination entry */
  entry = melo_playlist_get_entry (
      playlist, req->dest->indices, req->dest->n_indices, &parent);
  entries = parent ? &parent->children : &playlist->entries;

  /* Insert temporary list into destination */
  if (!entry) {
    MeloPlaylistEntry *list_end = list->prev;

    /* Append to list */
    if (entries->list) {
      entry = entries->list->prev;
      list->prev->next = entry->next;
      list->prev = entry;
      entry->next->prev = list_end;
      entry->next = list;
    } else
      entries->list = list;
  } else {
    MeloPlaylistEntry *list_end = list->prev;

    /* Insert before */
    list->prev->next = entry;
    entry->prev->next = list;
    list->prev = entry->prev;
    entry->prev = list_end;
    if (entries->list == entry)
      entries->list = list;
  }
  entries->count += count;
  if (entries->current &&
      req->dest->indices[req->dest->n_indices - 1] <= entries->current_index)
    entries->current_index += count;

  /* Update parent */
  for (i = 0; i < count; i++) {
    list->parent = parent;
    list = list->next;
  }

  /* Update current recursively */
  while (current && melo_playlist_list_get_index (entries, current, &i)) {
    /* Update current */
    entries->current = current;
    entries->current_index = i;

    /* Move to parent */
    current = current->parent;
    entries = current && current->parent ? &current->parent->children
                                         : &playlist->entries;
  }

  /* Set event */
  event.event_case = PLAYLIST__EVENT__EVENT_MOVE;
  event.move = req;

  /* Generate message */
  msg = melo_message_new (playlist__event__get_packed_size (&event));
  if (msg) {
    /* Pack event */
    melo_message_set_size (
        msg, playlist__event__pack (&event, melo_message_get_data (msg)));

    /* Broadcast move event */
    melo_events_broadcast (&playlist->events, melo_message_ref (msg));

    /* Broadcast move event to current playlist */
    if (melo_playlist_current == playlist) {
      melo_events_broadcast (&melo_playlist_events, msg);
      melo_playlist_update_player_control (playlist);
    } else
      melo_message_unref (msg);
  }

  return true;
}

static bool
melo_playlist_delete (
    MeloPlaylist *playlist, Playlist__Range *req, MeloAsyncData *async)
{
  Playlist__Event event = PLAYLIST__EVENT__INIT;
  MeloPlaylistEntry *list, *e, *current = NULL;
  unsigned int count;
  MeloMessage *msg;

  /* Extract entries to temporary list */
  list = melo_playlist_extract (playlist, req, &count, &current);
  if (!list)
    return false;

  /* Reset player */
  if (current) {
    melo_playlist_reset_current (playlist);
    melo_player_reset ();
  }

  /* Delete list */
  while (count--) {
    e = list;
    list = list->next;
    e->flags |= MELO_PLAYLIST_FLAG_SHUFFLE_DELETED;
    melo_playlist_entry_unref (e);
  }

  /* Set event */
  event.event_case = PLAYLIST__EVENT__EVENT_DELETE;
  event.delete_ = req;

  /* Generate message */
  msg = melo_message_new (playlist__event__get_packed_size (&event));
  if (msg) {
    /* Pack event */
    melo_message_set_size (
        msg, playlist__event__pack (&event, melo_message_get_data (msg)));

    /* Broadcast delete event */
    melo_events_broadcast (&playlist->events, melo_message_ref (msg));

    /* Broadcast delete event to current playlist */
    if (melo_playlist_current == playlist) {
      melo_events_broadcast (&melo_playlist_events, msg);
      melo_playlist_update_player_control (playlist);
    } else
      melo_message_unref (msg);
  }

  return true;
}

static MeloPlaylistBackup *
melo_playlist_backup_shuffle (MeloPlaylistList *entries)
{
  MeloPlaylistBackup *list;
  MeloPlaylistEntry *entry;
  unsigned int i;
  GRand *r;

  /* Prepare random generator */
  r = g_rand_new_with_seed (time (NULL));
  if (!r)
    return NULL;

  /* Allocate backup list */
  list = malloc (sizeof (*list) + (sizeof (*list->list) * entries->count));
  if (!list) {
    g_rand_free (r);
    return NULL;
  }

  /* Save each entry in list */
  list->count = 0;
  entry = entries->list;
  for (i = 0; i < entries->count; i++) {
    /* Save entry */
    entry->flags &= ~MELO_PLAYLIST_FLAG_SHUFFLE_INSERTED;
    list->list[i].entry = melo_playlist_entry_ref (entry);

    /* Save children order */
    if (entry->flags & MELO_PLAYLIST_FLAG_SORTABLE)
      list->list[i].children = melo_playlist_backup_shuffle (&entry->children);
    else
      list->list[i].children = NULL;

    /* Go to next entry */
    entry = entry->next;
    list->count++;
  }

  /* Shuffle the list */
  entries->list = 0;
  entries->count = 0;
  while (entries->count < list->count) {
    /* Get next entry randomly */
    i = g_rand_int_range (r, 0, list->count);
    entry = list->list[i].entry;
    if (entry->flags & MELO_PLAYLIST_FLAG_SHUFFLE_INSERTED)
      continue;

    /* Prepend entry to list */
    melo_playlist_list_prepend (entries, entry);
    entry->flags |= MELO_PLAYLIST_FLAG_SHUFFLE_INSERTED;

    /* Update current */
    if (entry == entries->current)
      entries->current_index = 0;
  }

  /* Free random generator */
  g_rand_free (r);

  return list;
}

static void
melo_playlist_backup_restore (MeloPlaylistBackup *list,
    MeloPlaylistList *entries, MeloPlaylistEntry *parent, bool remove)
{
  MeloPlaylistList tmp = {0};
  unsigned int i;

  /* Prepend first all added medias since shuffle is enabled */
  for (i = 0; i < entries->count; i++) {
    MeloPlaylistEntry *entry = entries->list;
    MeloPlaylistEntry *e = entries->list->next;

    /* Append media to temprary list */
    if (entry->flags & MELO_PLAYLIST_FLAG_SHUFFLE_ADDED)
      melo_playlist_list_append (&tmp, entry);
    entry->flags &= ~MELO_PLAYLIST_FLAG_SHUFFLE_ADDED;
    entry->parent = parent;

    /* Move to next entry */
    entries->list = e;
  }

  /* Restore list order */
  for (i = 0; i < list->count; i++) {
    MeloPlaylistEntry *entry = list->list[i].entry;
    bool remove_entry = remove;

    /* Append or delete entry */
    if (entry->flags & MELO_PLAYLIST_FLAG_SHUFFLE_DELETED || remove)
      remove_entry = true;
    else
      melo_playlist_list_append (&tmp, entry);
    entry->parent = parent;

    /* Restore children order */
    if (list->list[i].children)
      melo_playlist_backup_restore (
          list->list[i].children, &entry->children, entry, remove_entry);

    /* Release backup reference */
    melo_playlist_entry_unref (entry);
  }

  /* Update entries */
  *entries = tmp;

  /* Release backup list */
  free (list);
}

static bool
melo_playlist_shuffle (
    MeloPlaylist *playlist, bool enable, MeloAsyncData *async)
{
  MeloMessage *msg;

  /* Perform shuffle operation */
  if (enable && !playlist->shuffle) {
    /* Save current playlist order */
    playlist->shuffle = melo_playlist_backup_shuffle (&playlist->entries);
  } else if (!enable && playlist->shuffle) {
    MeloPlaylistList *entries = &playlist->entries;
    MeloPlaylistEntry *current = NULL;

    /* Get current */
    while (entries->current) {
      current = entries->current;
      entries = &current->children;
    }

    /* Restore the original playlist order */
    melo_playlist_backup_restore (
        playlist->shuffle, &playlist->entries, NULL, false);
    playlist->shuffle = NULL;

    /* Restore current */
    while (current) {
      entries =
          current->parent ? &current->parent->children : &playlist->entries;
      entries->current = current;
      melo_playlist_list_get_index (entries, current, &entries->current_index);
      current = current->parent;
    }
  } else
    return true;

  /* Generate shuffle event */
  msg = melo_playlist_message_shuffle (playlist);

  /* Broadcast message */
  if (msg) {
    /* Broadcast shuffle event */
    melo_events_broadcast (&playlist->events, melo_message_ref (msg));

    /* Broadcast shuffle event to current playlist */
    if (melo_playlist_current == playlist) {
      melo_events_broadcast (&melo_playlist_events, msg);
      melo_playlist_update_player_control (playlist);
    } else
      melo_message_unref (msg);
  }

  return true;
}

/**
 * melo_playlist_handle_request:
 * @id: the id of the playlist or %NULL for current playlist
 * @msg: the #MeloMessage to handle
 * @cb: the function to call when a response is sent
 * @user_data: data to pass to @cb
 *
 * This function is called when a new playlist request is received by the
 * application. This function will handle the request for a playlist pointed
 by
 * @id, or the current playlist if @id is NULL.

 * If the request is malformed or an internal error occurs, the function will
 * return %false, otherwise %true will be returned.
 * After returning, many asynchronous tasks related to the request can still
 be
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
    if (request->play) {
      MeloPlaylistEntry *entry;

      /* Get entry */
      entry = melo_playlist_get_entry (
          playlist, request->play->indices, request->play->n_indices, NULL);

      /* Play media */
      ret = melo_playlist_play (playlist, request->play->indices,
          request->play->n_indices, entry, false);

      /* Failed to play media */
      if (!ret && entry) {
        unsigned int count = request->play->n_indices;
        MeloPlaylistList *entries =
            entry->parent ? &entry->parent->children : &playlist->entries;

        /* Try to play next medias */
        ret = melo_playlist_play_recursive (
            playlist, entries, entry, request->play->indices, count, true);
      }
    }
    break;
  case PLAYLIST__REQUEST__REQ_MOVE:
    ret = melo_playlist_move (playlist, request->move, &async);
    break;
  case PLAYLIST__REQUEST__REQ_DELETE:
    ret = melo_playlist_delete (playlist, request->delete_, &async);
    break;
  case PLAYLIST__REQUEST__REQ_SHUFFLE:
    ret = melo_playlist_shuffle (playlist, request->shuffle, &async);
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
 * This function can be called to cancel a running or a pending request. If
 * the request exists, the asynchronous tasks will be cancelled and @cb will
 * be called with a NULL-message to signal end of request. If the request is
 * already finished or a cancellation is already pending, this function will
 * do nothing.
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

static void
melo_playlist_entry_set_playlist (
    MeloPlaylistEntry *entry, MeloPlaylist *playlist)
{
  MeloPlaylistEntry *e;
  unsigned int i;

  entry->playlist = playlist;

  e = entry->children.list;
  for (i = 0; i < entry->children.count; i++) {
    melo_playlist_entry_set_playlist (e, playlist);
    e = e->next;
  }
}

static bool
melo_playlist_handle_entry (MeloPlaylistEntry *entry, bool play)
{
  MeloPlaylistList *entries;
  MeloPlaylist *playlist;

  /* Invalid entry */
  if (!entry || entry->parent) {
    MELO_LOGE ("invalid playlist entry to %s: entry has a parent",
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
  melo_playlist_entry_set_playlist (entry, playlist);
  if (playlist->shuffle)
    entry->flags |= MELO_PLAYLIST_FLAG_SHUFFLE_ADDED;

  /* Broadcast media addition */
  melo_events_broadcast (
      &melo_playlist_events, melo_playlist_message_media (entry, true));

  /* Play media */
  if (play) {
    unsigned int idx = 0;

    /* Play media on top of playlist */
    melo_playlist_play (playlist, &idx, 1, entry, false);
  } else
    entries->current_index++;

  /* Update player controls */
  melo_playlist_update_player_control (playlist);

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
 * This function will add the media to the current playlist and will play it
 * on the player pointed by @player_id. It takes the ownership of @tags.
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
 * This function will add the entry to the current playlist and will play it
 * on the player pointed by the entry. It takes the ownership of @entry.
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
 * @player_id: (nullable): the ID of the media player to use
 * @path: (nullable): the media path / URI / URL
 * @name: the display name
 * @tags: (nullable): the media tags
 *
 * Creates a new #MeloPlaylistEntry which will contain a media to hold in the
 * playlist. The newly created entry should be added to the playlist with
 * melo_playlist_add_entry() or melo_playlist_play_entry().
 *
 * If @player_id is %NULL, the entry is seen as a sortable collection of media
 * entries.
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

  /* Set flags */
  entry->flags = MELO_PLAYLIST_FLAG_PLAYABLE;
  if (!player_id)
    entry->flags |= MELO_PLAYLIST_FLAG_SORTABLE;

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
    melo_playlist_list_clear (&entry->children);
    melo_tags_unref (entry->tags);
    g_free (entry->name);
    g_free (entry->path);
    g_free (entry->player);
    free (entry);
  } else if (ref_count < 1)
    MELO_LOGC ("negative reference counter %d", ref_count - 1);
}

/**
 * melo_playlist_entry_get_parent:
 * @entry: a #MeloPlaylistEntry
 *
 * This function returns the parent #MeloPlaylistEntry if applicable,
 * otherwise, %NULL is returned. It should be used only on an entry returned
 * by melo_playlist_entry_add_media().
 *
 * Returns: a #MeloPlaylistEntry reference of the @entry parent, %NULL
 *     otherwise. The reference should be released after usage with
 *     melo_playlist_entry_unref().
 */
MeloPlaylistEntry *
melo_playlist_entry_get_parent (MeloPlaylistEntry *entry)
{
  if (!entry || !entry->parent)
    return NULL;

  return melo_playlist_entry_ref (entry->parent);
}

/**
 * melo_playlist_entry_has_player:
 * @entry: a #MeloPlaylistEntry
 *
 * This function can be used to check if @entry has a player ID set or not.
 *
 * Returns: %true if the entry has a player ID set, %false otherwise.
 */
bool
melo_playlist_entry_has_player (MeloPlaylistEntry *entry)
{
  return entry && entry->player;
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
 * otherwise, the name will not be replaces and tags will be merged with
 * current one thanks to melo_tags_merge().
 *
 * Returns: %true if the entry has been updated successfully, %false
 * otherwise.
 */
bool
melo_playlist_entry_update (
    MeloPlaylistEntry *entry, const char *name, MeloTags *tags, bool reset)
{
  MeloPlaylist *playlist;
  MeloMessage *msg = NULL;

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
  playlist = entry->playlist;
  if (!playlist)
    return true;

  /* Generate entry update event */
  msg = melo_playlist_message_media (entry, false);

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
 * @player_id: (nullable): the ID of the media player to use
 * @entry: (nullable): a #MeloPlaylistEntry
 * @path: (nullable): the media path / URI / URL
 * @name: the display name
 * @tags: (nullable): the media tags
 * @ref: (nullable): a pointer to store a reference of the media
 *
 * This function can be used to add a media to an existing playlist entry.
 * The media will be added on top of the current list. If @path is set to
 * %NULL, the newly added media will not be playable. This function takes the
 * ownership of @tags.
 *
 * Returns: %true if the media has been added to the entry successfully,
 * %false otherwise.
 */
bool
melo_playlist_entry_add_media (MeloPlaylistEntry *entry, const char *player_id,
    const char *path, const char *name, MeloTags *tags, MeloPlaylistEntry **ref)
{
  MeloPlaylistEntry *media, *e;
  MeloPlaylist *playlist;
  MeloMessage *msg;

  /* Cannot add entry to invalid entry */
  if (!entry) {
    MELO_LOGE ("cannot add media to entry");
    return false;
  }

  /* Allocate new entry to hold media */
  media = melo_playlist_entry_new (player_id, path, name, tags);
  if (!media) {
    MELO_LOGE ("failed to add media to entry");
    return false;
  }

  /* Not playable / sortable when one of parents have a player set */
  for (e = entry; e; e = e->parent) {
    if (e->player) {
      media->flags = 0;
      break;
    }
  }

  /* Add parent */
  media->parent = entry;
  media->playlist = entry->playlist;

  /* Export media reference */
  if (ref)
    *ref = melo_playlist_entry_ref (media);

  /* Add media item to entry */
  melo_playlist_list_prepend (&entry->children, media);
  entry->children.current_index = 0;
  entry->children.current = media;
  if (media->playlist && media->playlist->shuffle)
    media->flags |= MELO_PLAYLIST_FLAG_SHUFFLE_ADDED;

  /* Get playlist */
  playlist = entry->playlist;
  if (!playlist)
    return true;

  /* Generate media add event */
  msg = melo_playlist_message_media (media, true);
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

static bool
melo_playlist_handle_control (bool next)
{
  unsigned int indices[PLAYLIST_MAX_DEPTH] = {0}, count = 0;
  MeloPlaylistList *entries;
  MeloPlaylistEntry *entry;
  MeloPlaylist *playlist;
  bool ret = false;

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

  /* Get current media */
  indices[count++] = entries->current_index;
  while (entry->children.current && count < PLAYLIST_MAX_DEPTH) {
    entries = &entry->children;
    entry = entry->children.current;
    indices[count++] = entries->current_index;
  }

  /* Play recursively */
  ret = melo_playlist_play_recursive (
      playlist, entries, entry, indices, count, next);

  /* Release playlist reference */
  g_object_unref (playlist);

  return ret;
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
 * player for playing. If no next media is found in the playlist, the
 * functions will return %false.
 *
 * Returns: %true if the next media in playlist is now playing, %false
 *     otherwise.
 */
bool
melo_playlist_play_next (void)
{
  return melo_playlist_handle_control (true);
}

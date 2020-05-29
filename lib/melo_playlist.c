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

typedef struct _MeloPlaylistMedia {
  char *player;
  char *path;

  char *name;
  MeloTags *tags;
} MeloPlaylistMedia;

struct _MeloPlaylist {
  /* Parent instance */
  GObject parent_instance;

  char *id;
  GList *list;
  GList *current;
  unsigned int current_index;
  unsigned int count;

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
  gboolean removed = FALSE;

  /* Free playlist list */
  while (playlist->list) {
    MeloPlaylistMedia *media = playlist->list->data;

    /* Remove from list */
    playlist->list = g_list_delete_link (playlist->list, playlist->list);

    /* Free media */
    melo_tags_unref (media->tags);
    g_free (media->name);
    g_free (media->path);
    g_free (media->player);
    free (media);
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

static MeloMessage *
melo_playlist_message_add (const char *name, MeloTags *tags)
{
  Playlist__Event pmsg = PLAYLIST__EVENT__INIT;
  Playlist__Media media = PLAYLIST__MEDIA__INIT;
  Tags__Tags media_tags = TAGS__TAGS__INIT;
  MeloMessage *msg;

  /* Set event type */
  pmsg.event_case = PLAYLIST__EVENT__EVENT_ADD;
  pmsg.add = &media;

  /* Set media */
  media.name = (char *) name;

  /* Set tags */
  if (tags) {
    media_tags.title = (char *) melo_tags_get_title (tags);
    media_tags.artist = (char *) melo_tags_get_artist (tags);
    media_tags.album = (char *) melo_tags_get_album (tags);
    media_tags.genre = (char *) melo_tags_get_genre (tags);
    media_tags.track = melo_tags_get_track (tags);
    media_tags.cover = (char *) melo_tags_get_cover (tags);
    media.tags = &media_tags;
  }

  /* Generate message */
  msg = melo_message_new (playlist__event__get_packed_size (&pmsg));
  if (msg)
    melo_message_set_size (
        msg, playlist__event__pack (&pmsg, melo_message_get_data (msg)));

  return msg;
}

static MeloMessage *
melo_playlist_message_play (unsigned int index)
{
  Playlist__Event pmsg = PLAYLIST__EVENT__INIT;
  MeloMessage *msg;

  /* Set event type */
  pmsg.event_case = PLAYLIST__EVENT__EVENT_PLAY;

  /* Set current index */
  pmsg.play = index;

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

static bool
melo_playlist_get_media_list (MeloPlaylist *playlist,
    Playlist__Request__GetMediaList *req, MeloAsyncData *async)
{
  Playlist__Response resp = PLAYLIST__RESPONSE__INIT;
  Playlist__Response__MediaList media_list =
      PLAYLIST__RESPONSE__MEDIA_LIST__INIT;
  Playlist__Media **medias_ptr = NULL;
  Playlist__Media *medias = NULL;
  Tags__Tags *tags = NULL;
  MeloMessage *msg;

  /* Set response type */
  resp.resp_case = PLAYLIST__RESPONSE__RESP_MEDIA_LIST;
  resp.media_list = &media_list;

  /* Generate media list */
  if (req->offset < playlist->count) {
    unsigned int i;
    GList *l;

    /* Set message */
    media_list.offset = req->offset;
    media_list.count = playlist->count - req->offset;
    media_list.current = playlist->current_index;

    /* Allocate buffer to store media list and tags */
    medias_ptr = malloc (sizeof (*medias_ptr) * media_list.count);
    medias = malloc (sizeof (*medias) * media_list.count);
    tags = malloc (sizeof (*tags) * media_list.count);

    /* Set list */
    media_list.n_medias = media_list.count;
    media_list.medias = medias_ptr;

    /* Fill list */
    for (l = g_list_nth (playlist->list, req->offset), i = 0; l != NULL;
         l = l->next, i++) {
      MeloPlaylistMedia *m = l->data;

      /* Set media */
      playlist__media__init (&medias[i]);
      medias[i].name = (char *) m->name;
      if (m->tags) {
        tags__tags__init (&tags[i]);
        tags[i].title = (char *) melo_tags_get_title (m->tags);
        tags[i].artist = (char *) melo_tags_get_artist (m->tags);
        tags[i].album = (char *) melo_tags_get_album (m->tags);
        tags[i].genre = (char *) melo_tags_get_genre (m->tags);
        tags[i].track = melo_tags_get_track (m->tags);
        tags[i].cover = (char *) melo_tags_get_cover (m->tags);
        medias[i].tags = &tags[i];
      }
      media_list.medias[i] = &medias[i];
    }

  } else {
    /* Set default message */
    media_list.offset = playlist->count;
    media_list.count = 0;
    media_list.current = playlist->current_index;
  }

  /* Generate message */
  msg = melo_message_new (playlist__response__get_packed_size (&resp));
  if (msg)
    melo_message_set_size (
        msg, playlist__response__pack (&resp, melo_message_get_data (msg)));

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
  MeloMessage *msg;

  /* Set response type */
  resp.resp_case = PLAYLIST__RESPONSE__RESP_CURRENT;

  /* Set current media index */
  resp.current = playlist->current_index;

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
melo_playlist_play (MeloPlaylist *playlist, unsigned int index)
{
  MeloPlaylistMedia *media;
  GList *l;

  /* Play media */
  l = g_list_nth (playlist->list, index);
  if (!l)
    return false;

  /* Switch playlist */
  G_LOCK (melo_playlist_mutex);
  if (playlist != melo_playlist_current)
    melo_playlist_current = playlist;
  G_UNLOCK (melo_playlist_mutex);

  /* Play this media */
  media = l->data;
  playlist->current = l;
  melo_player_play_media (
      media->player, media->path, media->name, melo_tags_ref (media->tags));
  playlist->current_index = index;

  /* Broadcast media addition */
  melo_events_broadcast (
      &melo_playlist_events, melo_playlist_message_play (index));

  /* Update player controls */
  melo_player_update_playlist_controls (
      playlist->current && playlist->current->next != NULL,
      playlist->list != playlist->current);

  return true;
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
    ret = melo_playlist_play (playlist, request->play);
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
melo_playlist_handle_media (const char *player_id, const char *path,
    const char *name, MeloTags *tags, bool play)
{
  MeloPlaylistMedia *media;
  MeloPlaylist *playlist;

  /* Find a name from path */
  if (!name && path) {
    name = strrchr (path, '/');
    if (name)
      name++;
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

  /* Allocate new media item */
  media = calloc (1, sizeof (*media));
  if (!media)
    return false;

  /* Fill media item */
  media->player = g_strdup (player_id);
  media->path = g_strdup (path);
  media->name = g_strdup (name);
  media->tags = tags;

  /* Add media item to list */
  playlist->list = g_list_prepend (playlist->list, media);
  playlist->count++;

  /* Play media */
  if (play) {
    playlist->current = playlist->list;
    melo_player_play_media (player_id, path, name, melo_tags_ref (tags));
    playlist->current_index = 0;
  } else
    playlist->current_index++;

  /* Broadcast media addition */
  melo_events_broadcast (
      &melo_playlist_events, melo_playlist_message_add (name, tags));

  /* Broadcast media playing */
  if (play)
    melo_events_broadcast (&melo_playlist_events,
        melo_playlist_message_play (playlist->current_index));

  /* Update player controls */
  melo_player_update_playlist_controls (
      playlist->current && playlist->current->next != NULL,
      playlist->list != playlist->current);

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
  return melo_playlist_handle_media (player_id, path, name, tags, false);
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
  return melo_playlist_handle_media (player_id, path, name, tags, true);
}

static bool
melo_playlist_handle_control (bool next)
{
  MeloPlaylistMedia *media;
  MeloPlaylist *playlist;

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

  /* No previous / next media to play */
  if (!playlist->current || (!next && !playlist->current->next) ||
      (next && !playlist->current->prev)) {
    g_object_unref (playlist);
    return false;
  }

  /* Update current playing media */
  if (next) {
    playlist->current = playlist->current->prev;
    playlist->current_index--;
  } else {
    playlist->current = playlist->current->next;
    playlist->current_index++;
  }
  media = playlist->current->data;

  /* Broadcast media playing */
  melo_events_broadcast (&melo_playlist_events,
      melo_playlist_message_play (playlist->current_index));

  /* Play previous / next media in playlist */
  melo_player_play_media (
      media->player, media->path, media->name, melo_tags_ref (media->tags));

  /* Update player controls */
  melo_player_update_playlist_controls (
      playlist->current && playlist->current->next != NULL,
      playlist->list != playlist->current);

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

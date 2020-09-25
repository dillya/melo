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

#define MELO_LOG_TAG "player"
#include "melo/melo_log.h"

#include "melo_events.h"
#include "melo_player_priv.h"
#include "melo_playlist_priv.h"

#include "player.pb-c.h"

/* Global player list */
G_LOCK_DEFINE_STATIC (melo_player_mutex);
static GHashTable *melo_player_list;

/* Player event listeners */
static MeloEvents melo_player_events;

/* Player settings */
static MeloSettings *melo_player_settings;
static MeloSettingsEntry *melo_player_settings_volume;
static MeloSettingsEntry *melo_player_settings_mute;
static guint melo_player_settings_source;

/* Current player */
static MeloPlayer *melo_player_current;

/* Current playlist controls */
static bool melo_player_prev;
static bool melo_player_next;

/* Global volume */
static float melo_player_volume = 1.f;
static bool melo_player_mute;

/* Melo player private data */
typedef struct _MeloPlayerPrivate {
  char *id;
  char *name;
  char *description;
  char *icon;

  /* Media */
  char *media_name;
  MeloTags *tags;

  /* Status */
  MeloPlayerState state;
  MeloPlayerStreamState stream_state;
  unsigned int stream_state_percent;
  unsigned int duration;

  /* Playlist entry */
  MeloPlaylistEntry *playlist_entry;
  MeloPlaylistEntry *current_playlist_entry;

  /* Audio sink */
  GstElement *sink;
  GstElement *volume;

  /* Settings */
  MeloSettings *settings;
} MeloPlayerPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (MeloPlayer, melo_player, G_TYPE_OBJECT)

/* Melo player properties */
enum { PROP_0, PROP_ID, PROP_NAME, PROP_DESCRIPTION, PROP_ICON };

static void melo_player_constructed (GObject *object);
static void melo_player_finalize (GObject *object);

static void melo_player_set_property (
    GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void melo_player_get_property (
    GObject *object, guint property_id, GValue *value, GParamSpec *pspec);

/* Protobuf message */
static MeloMessage *melo_player_message_add (MeloPlayerPrivate *priv);
static MeloMessage *melo_player_message_remove (MeloPlayerPrivate *priv);

static void
melo_player_class_init (MeloPlayerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  /* Override constructed to capture ID and register the player */
  object_class->constructed = melo_player_constructed;

  /* Override finalize to un-register the player and free private data */
  object_class->finalize = melo_player_finalize;

  /* Override properties accessors */
  object_class->set_property = melo_player_set_property;
  object_class->get_property = melo_player_get_property;

  /**
   * MeloPlayer:id:
   *
   * The unique ID of the player. This must be set during the construct and it
   * can be only read after instantiation. The value provided at construction
   * will be used to register the player in the global list which will be
   * exported to user interface.
   */
  g_object_class_install_property (object_class, PROP_ID,
      g_param_spec_string ("id", "ID", "Player unique ID.", NULL,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  /**
   * MeloPlayer:name:
   *
   * The name of the player to display. This must be set during the construct
   * and it can be only read after instantiation.
   */
  g_object_class_install_property (object_class, PROP_NAME,
      g_param_spec_string ("name", "Name", "Player name.", NULL,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  /**
   * MeloPlayer:description:
   *
   * The description of the player. This must be set during the construct and
   * it can be only read after instantiation.
   */
  g_object_class_install_property (object_class, PROP_DESCRIPTION,
      g_param_spec_string ("description", "Description", "Player description.",
          NULL,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  /**
   * MeloPlayer:icon:
   *
   * The icon of the player. This must be set during the construct and it can
   * be only read after instantiation.
   */
  g_object_class_install_property (object_class, PROP_ICON,
      g_param_spec_string ("icon", "Icon", "Player icon.", NULL,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
}

static void
melo_player_init (MeloPlayer *self)
{
}

static void
melo_player_constructed (GObject *object)
{
  MeloPlayer *player = MELO_PLAYER (object);
  MeloPlayerPrivate *priv = melo_player_get_instance_private (player);
  MeloPlayerClass *class;

  /* Register player */
  if (priv->id) {
    gboolean added = FALSE;
    /* Lock player list */
    G_LOCK (melo_player_mutex);

    /* Create player list */
    if (!melo_player_list)
      melo_player_list = g_hash_table_new (g_str_hash, g_str_equal);

    /* Insert player into list */
    if (melo_player_list &&
        g_hash_table_contains (melo_player_list, priv->id) == FALSE) {
      g_hash_table_insert (melo_player_list, (gpointer) priv->id, player);
      added = TRUE;
    }

    /* Unlock player list */
    G_UNLOCK (melo_player_mutex);

    /* Player added */
    if (added == TRUE) {
      /* Broadcast 'add' message */
      melo_events_broadcast (
          &melo_player_events, melo_player_message_add (priv));

      MELO_LOGI ("player '%s' added", priv->id);
    } else
      MELO_LOGW ("failed to add player '%s' to global list", priv->id);
  }

  /* Create settings */
  class = MELO_PLAYER_GET_CLASS (player);
  if (class->settings) {
    /* Populate settings */
    priv->settings = melo_settings_new (priv->id);
    class->settings (player, priv->settings);

    /* Load values */
    melo_settings_load (priv->settings);
  }

  /* Chain constructed */
  G_OBJECT_CLASS (melo_player_parent_class)->constructed (object);
}

static void
melo_player_finalize (GObject *object)
{
  MeloPlayer *player = MELO_PLAYER (object);
  MeloPlayerPrivate *priv = melo_player_get_instance_private (player);

  /* Release settings */
  if (priv->settings)
    g_object_unref (priv->settings);

  /* Release tags and name */
  melo_tags_unref (priv->tags);
  g_free (priv->media_name);

  /* Release audio sink */
  if (priv->sink)
    gst_object_unref (priv->sink);

  /* Un-register player */
  if (priv->id) {
    gboolean removed = FALSE;

    /* Lock player list */
    G_LOCK (melo_player_mutex);

    /* Reset current player */
    if (player == melo_player_current)
      melo_player_current = NULL;

    /* Remove player from list */
    if (melo_player_list) {
      g_hash_table_remove (melo_player_list, priv->id);
      removed = TRUE;
    }

    /* Destroy player list */
    if (melo_player_list && !g_hash_table_size (melo_player_list)) {
      g_hash_table_destroy (melo_player_list);
      melo_player_list = NULL;
    }

    /* Unlock player list */
    G_UNLOCK (melo_player_mutex);

    /* Player removed */
    if (removed == TRUE) {
      /* Broadcast 'remove' message */
      melo_events_broadcast (
          &melo_player_events, melo_player_message_remove (priv));

      MELO_LOGI ("player '%s' removed", priv->id);
    }
  }

  /* Free playlist entries */
  melo_playlist_entry_unref (priv->current_playlist_entry);
  melo_playlist_entry_unref (priv->playlist_entry);

  /* Free ID */
  g_free (priv->id);

  /* Chain finalize */
  G_OBJECT_CLASS (melo_player_parent_class)->finalize (object);
}

static void
melo_player_set_property (
    GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
  MeloPlayerPrivate *priv =
      melo_player_get_instance_private (MELO_PLAYER (object));

  switch (property_id) {
  case PROP_ID:
    g_free (priv->id);
    priv->id = g_value_dup_string (value);
    break;
  case PROP_NAME:
    g_free (priv->name);
    priv->name = g_value_dup_string (value);
    break;
  case PROP_DESCRIPTION:
    g_free (priv->description);
    priv->description = g_value_dup_string (value);
    break;
  case PROP_ICON:
    g_free (priv->icon);
    priv->icon = g_value_dup_string (value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
melo_player_get_property (
    GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
  MeloPlayerPrivate *priv =
      melo_player_get_instance_private (MELO_PLAYER (object));

  switch (property_id) {
  case PROP_ID:
    g_value_set_string (value, priv->id);
    break;
  case PROP_NAME:
    g_value_set_string (value, priv->name);
    break;
  case PROP_DESCRIPTION:
    g_value_set_string (value, priv->description);
    break;
  case PROP_ICON:
    g_value_set_string (value, priv->icon);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

/*
 * Static functions
 */

void
melo_player_settings_init (void)
{
  MeloSettingsGroup *group;

  if (melo_player_settings)
    return;

  /* Create new player settings */
  melo_player_settings = melo_settings_new ("player");
  if (!melo_player_settings) {
    MELO_LOGW ("failed to create player settings");
    return;
  }

  /* Create global group */
  group = melo_settings_add_group (
      melo_player_settings, "global", "Global", NULL, NULL, NULL);

  /* Add volume / mute settings (not exported) */
  melo_player_settings_volume = melo_settings_group_add_float (
      group, "volume", "Volume", NULL, 1.f, NULL, MELO_SETTINGS_FLAG_NO_EXPORT);
  melo_player_settings_mute = melo_settings_group_add_boolean (
      group, "mute", "Mute", NULL, false, NULL, MELO_SETTINGS_FLAG_NO_EXPORT);

  /* Load settings */
  melo_settings_load (melo_player_settings);

  /* Set volume / mute */
  melo_settings_entry_get_float (
      melo_player_settings_volume, &melo_player_volume, NULL);
  melo_settings_entry_get_boolean (
      melo_player_settings_mute, &melo_player_mute, NULL);
}

void
melo_player_settings_deinit (void)
{
  if (!melo_player_settings)
    return;

  /* Release settings */
  g_object_unref (melo_player_settings);
  melo_player_settings = NULL;
}

static MeloMessage *
melo_player_message_add (MeloPlayerPrivate *priv)
{
  Player__Event pmsg = PLAYER__EVENT__INIT;
  Player__Event__Desc pdesc = PLAYER__EVENT__DESC__INIT;
  MeloMessage *msg;

  /* Prepare message */
  pmsg.event_case = PLAYER__EVENT__EVENT_ADD;
  pmsg.add = &pdesc;

  /* Set description */
  pdesc.id = priv->id;
  pdesc.name = priv->name;
  pdesc.description = priv->description;
  pdesc.icon = priv->icon;

  /* Generate message */
  msg = melo_message_new (player__event__get_packed_size (&pmsg));
  if (msg)
    melo_message_set_size (
        msg, player__event__pack (&pmsg, melo_message_get_data (msg)));

  return msg;
}

static MeloMessage *
melo_player_message_remove (MeloPlayerPrivate *priv)
{
  Player__Event pmsg = PLAYER__EVENT__INIT;
  Player__Event__Desc pdesc = PLAYER__EVENT__DESC__INIT;
  MeloMessage *msg;

  /* Prepare message */
  pmsg.event_case = PLAYER__EVENT__EVENT_REMOVE;
  pmsg.remove = &pdesc;

  /* Set minimal description */
  pdesc.id = priv->id;

  /* Generate message */
  msg = melo_message_new (player__event__get_packed_size (&pmsg));
  if (msg)
    melo_message_set_size (
        msg, player__event__pack (&pmsg, melo_message_get_data (msg)));

  return msg;
}

static MeloMessage *
melo_player_message_media (const char *name, MeloTags *tags)
{
  Player__Event pmsg = PLAYER__EVENT__INIT;
  Player__Event__Media media = PLAYER__EVENT__MEDIA__INIT;
  Tags__Tags media_tags = TAGS__TAGS__INIT;
  MeloMessage *msg;

  /* Set event type */
  pmsg.event_case = PLAYER__EVENT__EVENT_MEDIA;
  pmsg.media = &media;

  /* Set name */
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
  msg = melo_message_new (player__event__get_packed_size (&pmsg));
  if (msg)
    melo_message_set_size (
        msg, player__event__pack (&pmsg, melo_message_get_data (msg)));

  return msg;
}

static MeloMessage *
melo_player_message_status (MeloPlayerState state,
    MeloPlayerStreamState stream_state, unsigned int stream_state_value)
{
  Player__Event pmsg = PLAYER__EVENT__INIT;
  Player__Event__Status status = PLAYER__EVENT__STATUS__INIT;
  MeloMessage *msg;

  /* Set event type */
  pmsg.event_case = PLAYER__EVENT__EVENT_STATUS;
  pmsg.status = &status;

  /* Set state */
  switch (state) {
  case MELO_PLAYER_STATE_PLAYING:
    status.state = PLAYER__STATE__STATE_PLAYING;
    break;
  case MELO_PLAYER_STATE_PAUSED:
    status.state = PLAYER__STATE__STATE_PAUSED;
    break;
  case MELO_PLAYER_STATE_STOPPED:
    status.state = PLAYER__STATE__STATE_STOPPED;
    break;
  case MELO_PLAYER_STATE_NONE:
  default:
    status.state = PLAYER__STATE__STATE_NONE;
  }

  /* Set stream state */
  switch (stream_state) {
  case MELO_PLAYER_STREAM_STATE_LOADING:
    status.stream_state = PLAYER__EVENT__STATUS__STREAM_STATE__LOADING;
    break;
  case MELO_PLAYER_STREAM_STATE_BUFFERING:
    status.stream_state = PLAYER__EVENT__STATUS__STREAM_STATE__BUFFERING;
    break;
  case MELO_PLAYER_STREAM_STATE_NONE:
  default:
    status.stream_state = PLAYER__EVENT__STATUS__STREAM_STATE__NONE;
  }
  status.value = stream_state_value;

  /* Generate message */
  msg = melo_message_new (player__event__get_packed_size (&pmsg));
  if (msg)
    melo_message_set_size (
        msg, player__event__pack (&pmsg, melo_message_get_data (msg)));

  return msg;
}

static MeloMessage *
melo_player_message_position (unsigned int position, unsigned int duration)
{
  Player__Event pmsg = PLAYER__EVENT__INIT;
  Player__Event__Position pos = PLAYER__EVENT__POSITION__INIT;
  MeloMessage *msg;

  /* Set event type */
  pmsg.event_case = PLAYER__EVENT__EVENT_POSITION;
  pmsg.position = &pos;

  /* Set position */
  pos.position = position;
  pos.duration = duration;

  /* Generate message */
  msg = melo_message_new (player__event__get_packed_size (&pmsg));
  if (msg)
    melo_message_set_size (
        msg, player__event__pack (&pmsg, melo_message_get_data (msg)));

  return msg;
}

static MeloMessage *
melo_player_message_volume (float volume, bool mute)
{
  Player__Event pmsg = PLAYER__EVENT__INIT;
  Player__Event__Volume vol = PLAYER__EVENT__VOLUME__INIT;
  MeloMessage *msg;

  /* Set event type */
  pmsg.event_case = PLAYER__EVENT__EVENT_VOLUME;
  pmsg.volume = &vol;

  /* Set volume and mute */
  vol.volume = volume;
  vol.mute = mute;

  /* Generate message */
  msg = melo_message_new (player__event__get_packed_size (&pmsg));
  if (msg)
    melo_message_set_size (
        msg, player__event__pack (&pmsg, melo_message_get_data (msg)));

  return msg;
}

static MeloMessage *
melo_player_message_error (const char *error)
{
  Player__Event pmsg = PLAYER__EVENT__INIT;
  MeloMessage *msg;

  /* Set event type */
  pmsg.event_case = PLAYER__EVENT__EVENT_ERROR;

  /* Set error message */
  pmsg.error = (char *) error;

  /* Generate message */
  msg = melo_message_new (player__event__get_packed_size (&pmsg));
  if (msg)
    melo_message_set_size (
        msg, player__event__pack (&pmsg, melo_message_get_data (msg)));

  return msg;
}

static MeloMessage *
melo_player_message_playlist (bool prev, bool next)
{
  Player__Event pmsg = PLAYER__EVENT__INIT;
  Player__Event__Playlist playlist = PLAYER__EVENT__PLAYLIST__INIT;
  MeloMessage *msg;

  /* Set event type */
  pmsg.event_case = PLAYER__EVENT__EVENT_PLAYLIST;
  pmsg.playlist = &playlist;

  /* Set playlist controls */
  playlist.prev = prev;
  playlist.next = next;

  /* Generate message */
  msg = melo_message_new (player__event__get_packed_size (&pmsg));
  if (msg)
    melo_message_set_size (
        msg, player__event__pack (&pmsg, melo_message_get_data (msg)));

  return msg;
}

static void
melo_player_list_cb (gpointer key, gpointer value, gpointer user_data)
{
  MeloPlayer *player = value;
  MeloAsyncData *data = user_data;
  MeloPlayerPrivate *priv = melo_player_get_instance_private (player);
  MeloMessage *msg;

  /* Generate 'add' message */
  msg = melo_player_message_add (priv);
  data->cb (msg, data->user_data);
  melo_message_unref (msg);
}

/**
 * melo_player_add_event_listener:
 * @cb: the function to call when a new event occurred
 * @user_data: data to pass to @cb
 *
 * This function will add the @cb and @user_data to internal list of event
 * listener and the @cb will be called when a new event occurs.
 *
 * The @cb / @user_data couple is used to identify an event listener, so a
 * second call to this function with the same parameters will fails. You must
 * use the same couple in melo_player_remove_event_listener() to remove this
 * event listener.
 *
 * Returns: %true if the listener has been added successfully, %false otherwise.
 */
bool
melo_player_add_event_listener (MeloAsyncCb cb, void *user_data)
{
  bool ret;

  /* Add event listener */
  ret = melo_events_add_listener (&melo_player_events, cb, user_data);

  /* Send status to new event listener */
  if (ret) {
    MeloPlayer *player;
    MeloMessage *msg;

    /* Send player list to new listener */
    if (melo_player_list) {
      MeloAsyncData data = {.cb = cb, .user_data = user_data};

      /* Send player list */
      g_hash_table_foreach (melo_player_list, melo_player_list_cb, &data);
    }

    /* Get current player */
    G_LOCK (melo_player_mutex);
    player = melo_player_current ? g_object_ref (melo_player_current) : NULL;
    G_UNLOCK (melo_player_mutex);

    /* Send current player status */
    if (player) {
      MeloPlayerPrivate *priv = melo_player_get_instance_private (player);
      MeloPlayerClass *class = MELO_PLAYER_GET_CLASS (player);
      unsigned int position = 0;

      /* Get current position */
      if (class->get_position)
        position = class->get_position (player);

      /* Send current state */
      msg = melo_player_message_media (priv->media_name, priv->tags);
      if (msg) {
        cb (msg, user_data);
        melo_message_unref (msg);
      }
      msg = melo_player_message_status (
          priv->state, priv->stream_state, priv->stream_state_percent);
      if (msg) {
        cb (msg, user_data);
        melo_message_unref (msg);
      }
      msg = melo_player_message_position (position, priv->duration);
      if (msg) {
        cb (msg, user_data);
        melo_message_unref (msg);
      }

      /* Release player */
      g_object_unref (player);
    }

    /* Send volume and playlist status */
    msg = melo_player_message_volume (melo_player_volume, melo_player_mute);
    if (msg) {
      cb (msg, user_data);
      melo_message_unref (msg);
    }
    msg = melo_player_message_playlist (melo_player_prev, melo_player_next);
    if (msg) {
      cb (msg, user_data);
      melo_message_unref (msg);
    }
  }

  return ret;
}

/**
 * melo_player_remove_event_listener:
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
melo_player_remove_event_listener (MeloAsyncCb cb, void *user_data)
{
  return melo_events_remove_listener (&melo_player_events, cb, user_data);
}

/**
 * melo_player_handle_request:
 * @msg: the #MeloMessage to handle
 * @cb: the function to call when a response is sent
 * @user_data: data to pass to @cb
 *
 * This function is called when a new player request is received by the
 * application. This function will handle and forward the request to the
 * destination current player instance.
 *
 * If the request is malformed or an internal error occurs, the function will
 * return %false, otherwise %true will be returned.
 *
 * Returns: %true if the message has been handled by the player, %false
 *     otherwise.
 */
bool
melo_player_handle_request (MeloMessage *msg, MeloAsyncCb cb, void *user_data)
{
  Player__Request *request;
  MeloPlayerClass *class;
  MeloPlayer *player;
  bool ret = false;

  if (!msg)
    return false;

  /* Unpack request */
  request = player__request__unpack (
      NULL, melo_message_get_size (msg), melo_message_get_cdata (msg, NULL));
  if (!request) {
    MELO_LOGE ("failed to unpack request");
    return false;
  }

  /* Get current player */
  G_LOCK (melo_player_mutex);
  player = melo_player_current ? g_object_ref (melo_player_current) : NULL;
  G_UNLOCK (melo_player_mutex);

  /* Get class */
  class = player ? MELO_PLAYER_GET_CLASS (player) : NULL;

  /* Handle request */
  switch (request->req_case) {
  case PLAYER__REQUEST__REQ_SET_STATE:
    if (class && class->set_state) {
      MeloPlayerState state;

      /* Convert state */
      switch (request->set_state) {
      case PLAYER__STATE__STATE_PLAYING:
        state = MELO_PLAYER_STATE_PLAYING;
        break;
      case PLAYER__STATE__STATE_PAUSED:
        state = MELO_PLAYER_STATE_PAUSED;
        break;
      case PLAYER__STATE__STATE_STOPPED:
        state = MELO_PLAYER_STATE_STOPPED;
        break;
      case PLAYER__STATE__STATE_NONE:
      default:
        state = MELO_PLAYER_STATE_NONE;
      }

      /* Set new state */
      ret = class->set_state (player, state);
      if (ret)
        melo_player_update_state (player, state);
    }
    break;
  case PLAYER__REQUEST__REQ_SET_POSITION:
    if (class && class->set_position)
      ret = class->set_position (player, request->set_position);
    break;
  case PLAYER__REQUEST__REQ_SET_VOLUME:
    melo_player_update_volume (player, request->set_volume, melo_player_mute);
    ret = true;
    break;
  case PLAYER__REQUEST__REQ_SET_MUTE:
    melo_player_update_volume (player, melo_player_volume, request->set_mute);
    ret = true;
    break;
  case PLAYER__REQUEST__REQ_PLAY_PREVIOUS:
    ret = melo_playlist_play_previous ();
    break;
  case PLAYER__REQUEST__REQ_PLAY_NEXT:
    ret = melo_playlist_play_next ();
    break;
  default:
    MELO_LOGE ("request %u not supported", request->req_case);
  }

  /* Release player */
  if (player)
    g_object_unref (player);

  /* Free request */
  player__request__free_unpacked (request, NULL);

  /* End of request */
  if (ret && cb)
    cb (NULL, user_data);

  return ret;
}

static MeloPlayer *
melo_player_get_by_id (const char *id)
{
  MeloPlayer *player = NULL;

  /* Lock player list */
  G_LOCK (melo_player_mutex);

  if (melo_player_list && id)
    player = g_hash_table_lookup (melo_player_list, id);
  if (player)
    g_object_ref (player);

  /* Unlock player list */
  G_UNLOCK (melo_player_mutex);

  return player;
}

/**
 * melo_player_get_asset:
 * @id: the id of a #MeloPlayer
 * @asset: the id of the asset to get
 *
 * This function returns the URI of an asset identified by @asset from the
 * current player identified by @id. If the asset doesn't exist, %NULL is
 * returned.
 *
 * Returns: the URI of the asset or %NULL.
 */
char *
melo_player_get_asset (const char *id, const char *asset)
{
  MeloPlayerClass *class;
  MeloPlayer *player;
  char *ret = NULL;

  if (!id || !asset)
    return NULL;

  /* Find player */
  player = melo_player_get_by_id (id);
  if (!player)
    return NULL;
  class = MELO_PLAYER_GET_CLASS (player);

  /* Get asset from player */
  if (class->get_asset)
    ret = class->get_asset (player, asset);

  /* Release player */
  g_object_unref (player);

  return ret;
}

bool
melo_player_play_media (const char *id, const char *path, const char *name,
    MeloTags *tags, MeloPlaylistEntry *entry)
{
  MeloPlayerClass *class;
  MeloPlayer *old, *player;
  MeloPlayerPrivate *priv;
  bool ret = false;

  /* Find player */
  player = melo_player_get_by_id (id);
  if (!player)
    return false;
  priv = melo_player_get_instance_private (player);

  /* Replace current player */
  G_LOCK (melo_player_mutex);
  old = melo_player_current ? g_object_ref (melo_player_current) : NULL;
  melo_player_current = player;
  G_UNLOCK (melo_player_mutex);

  /* Stop previous player */
  if (old) {
    class = MELO_PLAYER_GET_CLASS (old);
    if (class->set_state)
      class->set_state (old, MELO_PLAYER_STATE_NONE);
    g_object_unref (old);
  }

  /* Replace playlist entry */
  melo_playlist_entry_unref (priv->playlist_entry);
  priv->playlist_entry = entry;

  /* Replace current playlist entry */
  melo_playlist_entry_unref (priv->current_playlist_entry);
  priv->current_playlist_entry = melo_playlist_entry_ref (entry);

  /* Replace player name */
  g_free (priv->media_name);
  priv->media_name = g_strdup (name);

  /* Replace player tags */
  melo_tags_unref (priv->tags);
  priv->tags = tags;

  /* Broadcast media change */
  melo_events_broadcast (&melo_player_events,
      melo_player_message_media (priv->media_name, priv->tags));

  /* Reset player */
  melo_player_update_status (
      player, MELO_PLAYER_STATE_PLAYING, MELO_PLAYER_STREAM_STATE_LOADING, 0);
  melo_player_update_duration (player, 0, 0);

  /* Set default volume */
  if (old != player)
    melo_player_update_volume (player, melo_player_volume, melo_player_mute);

  /* Play media */
  class = MELO_PLAYER_GET_CLASS (player);
  if (class->play)
    ret = class->play (player, path);

  /* Release player */
  g_object_unref (player);

  return ret;
}

void
melo_player_update_playlist_controls (bool prev, bool next)
{
  /* Update controls */
  melo_player_prev = prev;
  melo_player_next = next;

  /* Broadcast playlist controls change */
  melo_events_broadcast (
      &melo_player_events, melo_player_message_playlist (prev, next));
}

/*
 * Protected functions
 */

/**
 * melo_player_get_sink:
 * @player: a #MeloPlayer
 * @name: (nullable): the name of the gstreamer element, can be %NULL
 *
 * This functions can be used by a player implementation to get a gstreamer
 * sink element containing an audio converter and resampler, a volume control
 * and an audio sink with all capabilities set accurately with global
 * configuration.
 * Then, the volume control is automatically handled by the #MeloPlayer class
 * itself.
 *
 * Returns: (transfer full): a newly #GstElement to use as audio sink.
 */
GstElement *
melo_player_get_sink (MeloPlayer *player, const char *name)
{
  MeloPlayerPrivate *priv = melo_player_get_instance_private (player);

  /* Create audio sink */
  if (!priv->sink) {
    GstElement *convert, *resample, *filter, *audiosink;
    GstPad *gpad, *pad;
    GstCaps *caps;

    /* Create a new bin for audio sink */
    priv->sink = gst_bin_new (name);

    /* Create converter / resampler / volume and sink */
    convert = gst_element_factory_make ("audioconvert", NULL);
    resample = gst_element_factory_make ("audioresample", NULL);
    priv->volume = gst_element_factory_make ("volume", NULL);
    filter = gst_element_factory_make ("capsfilter", NULL);
    audiosink = gst_element_factory_make ("autoaudiosink", NULL);
    if (!convert || !resample || !priv->volume || !filter || !audiosink) {
      MELO_LOGE ("failed to create audio sink for %s", priv->id);
      gst_object_unref (convert);
      gst_object_unref (resample);
      gst_object_unref (priv->volume);
      gst_object_unref (filter);
      gst_object_unref (audiosink);
      gst_object_unref (priv->sink);
      priv->sink = NULL;
      return NULL;
    }

    /* Generate caps */
    caps = gst_caps_new_simple ("audio/x-raw", "format", G_TYPE_STRING, "S32LE",
        "layout", G_TYPE_STRING, "interleaved", "rate", G_TYPE_INT, 44100,
        "channels", G_TYPE_INT, 2, NULL);

    /* Setup caps for audio sink */
    g_object_set (filter, "caps", caps, NULL);
    gst_caps_unref (caps);

    /* Add and connect convert -> resample -> volume -> audiosink to bin */
    gst_bin_add_many (GST_BIN (priv->sink), convert, resample, priv->volume,
        filter, audiosink, NULL);
    gst_element_link_many (
        convert, resample, priv->volume, filter, audiosink, NULL);

    /* Create sink pad on bin and connect to sink pad of audioconver */
    pad = gst_element_get_static_pad (convert, "sink");
    gpad = gst_ghost_pad_new ("sink", pad);
    gst_element_add_pad (priv->sink, gpad);
    gst_object_unref (pad);
  }

  return gst_object_ref (priv->sink);
}

/**
 * melo_player_update_media:
 * @player: a #MeloPlayer
 * @name: the display name
 * @tags: the #MeloTags to set
 * @merge_flags: a combination of #MeloTagsMergeFlag to apply on player tags
 *
 * This function should be used by the derived #MeloPlayer class implementations
 * to update the current media: the displayed name and tags will be totally
 * replaced and new sub-media will be added to the current playlist. If you only
 * need to update a part of tags and avoid creating a new sub-media, the
 * melo_player_update_tags() should be called. The previously used name and
 * tags are freed by this function. The function takes owner-ship of the name
 * and the tags.
 *
 * The @merge_flags is used during merge of @tags and the current player tags:
 * setting MELO_TAGS_MERGE_FLAG_SKIP_COVER can be useful if you want to preserve
 * the initial player cover.
 *
 * It will send an event to all listeners in order to notify of media change.
 *
 * This function is a protected function and is only intended for derived class.
 */
void
melo_player_update_media (MeloPlayer *player, const char *name, MeloTags *tags,
    unsigned int merge_flags)
{
  MeloPlayerPrivate *priv;
  MeloTags *t;

  if (!player)
    return;
  priv = melo_player_get_instance_private (player);

  /* Replace name */
  g_free (priv->media_name);
  priv->media_name = g_strdup (name);

  /* Replace player tags */
  melo_tags_ref (tags);
  t = melo_tags_new ();
  melo_tags_merge (t, tags, merge_flags);
  melo_tags_merge (
      t, priv->tags, (~merge_flags & MELO_TAGS_MERGE_FLAG_SKIP_ALL));
  priv->tags = t;

  /* Update current player */
  if (player == melo_player_current) {
    MeloPlaylistEntry *entry;

    /* Update playlist current media */
    melo_tags_ref (tags);
    melo_playlist_entry_add_media (
        priv->playlist_entry, NULL, name, tags, &entry);

    /* Replace current playlist entry */
    melo_playlist_entry_unref (priv->current_playlist_entry);
    priv->current_playlist_entry = entry;

    /* Broadcast media change */
    melo_events_broadcast (&melo_player_events,
        melo_player_message_media (priv->media_name, priv->tags));
  }

  /* Release tags */
  melo_tags_unref (tags);
}

/**
 * melo_player_update_tags:
 * @player: a #MeloPlayer
 * @tags: the #MeloTags to set
 * @merge_flags: a combination of #MeloTagsMergeFlag to apply on current tags
 *
 * This function should be used by the derived #MeloPlayer class implementations
 * to update the current media tags: the currently set tags will be merged with
 * the tags from @tags with melo_tags_merge(). The function takes the owner-ship
 * of @tags.
 * It will send an event to all listeners in order to notify of media change.
 *
 * This function is a protected function and is only intended for derived class.
 */
void
melo_player_update_tags (
    MeloPlayer *player, MeloTags *tags, unsigned int merge_flags)
{
  MeloPlayerPrivate *priv;

  if (!player)
    return;

  /* Merge tags */
  priv = melo_player_get_instance_private (player);
  priv->tags = melo_tags_merge (tags, priv->tags, merge_flags);

  /* Update current player */
  if (player == melo_player_current) {
    /* Update playlist current media */
    melo_tags_ref (tags);
    melo_playlist_entry_update (
        priv->current_playlist_entry, priv->media_name, tags, true);

    /* Broadcast media change */
    melo_events_broadcast (&melo_player_events,
        melo_player_message_media (priv->media_name, priv->tags));
  }
}

/**
 * melo_player_set_status:
 * @player: a #MeloPlayer
 * @state: the #MeloPlayerState to set
 * @stream_state: the #MeloPlayerStreamState to set
 * @stream_state_percent: the value associated to @stream_state, between 0 and
 *     100
 *
 * This function should be used by the derived #MeloPlayer class implementations
 * to update the current status (state and stream state) of the player.
 * When @stream_state is different from #MELO_PLAYER_STREAM_STATE_NONE, the
 * value of @stream_state_percent must be set with as a percentage, so between 0
 * and 100.
 * It will send an event to all listeners in order to notify of status change.
 *
 * This function is a protected function and is only intended for derived class.
 */
void
melo_player_update_status (MeloPlayer *player, MeloPlayerState state,
    MeloPlayerStreamState stream_state, unsigned int stream_state_percent)
{
  MeloPlayerPrivate *priv;

  if (!player)
    return;

  /* Update state */
  priv = melo_player_get_instance_private (player);
  priv->state = state;

  /* Update status */
  melo_player_update_stream_state (player, stream_state, stream_state_percent);
}

/**
 * melo_player_update_state:
 * @player: a #MeloPlayer
 * @state: the #MeloPlayerState to set
 *
 * This function should be used by the derived #MeloPlayer class implementations
 * to update the current state of the player.
 * It will send an event to all listeners in order to notify of state change.
 *
 * This function is a protected function and is only intended for derived class.
 */
void
melo_player_update_state (MeloPlayer *player, MeloPlayerState state)
{
  MeloPlayerPrivate *priv;

  if (!player)
    return;

  /* Update state */
  priv = melo_player_get_instance_private (player);
  priv->state = state;

  /* Broadcast state change */
  if (player == melo_player_current)
    melo_events_broadcast (&melo_player_events,
        melo_player_message_status (
            state, priv->stream_state, priv->stream_state_percent));
}

/**
 * melo_player_update_stream_state:
 * @player: a #MeloPlayer
 * @stream_state: the #MeloPlayerStreamState to set
 * @percent: the value associated to @stream_state, between 0 and 100
 *
 * This function should be used by the derived #MeloPlayer class implementations
 * to update the current stream state of the player.
 * When @stream_state is different from #MELO_PLAYER_STREAM_STATE_NONE, the
 * value of @percent must be set with as a percentage, so between 0 and 100.
 * It will send an event to all listeners in order to notify of state change.
 *
 * This function is a protected function and is only intended for derived class.
 */
void
melo_player_update_stream_state (MeloPlayer *player,
    MeloPlayerStreamState stream_state, unsigned int percent)
{
  MeloPlayerPrivate *priv;
  bool update_position;

  if (!player)
    return;

  /* Clamp percent */
  if (percent > 100)
    percent = 100;
  if (stream_state == MELO_PLAYER_STREAM_STATE_NONE)
    percent = 0;

  /* Update current state */
  priv = melo_player_get_instance_private (player);
  update_position = priv->stream_state == MELO_PLAYER_STREAM_STATE_BUFFERING &&
                    stream_state != priv->stream_state;
  priv->stream_state = stream_state;
  priv->stream_state_percent = percent;

  /* Broadcast stream state change */
  if (player == melo_player_current)
    melo_events_broadcast (&melo_player_events,
        melo_player_message_status (priv->state, stream_state, percent));

  /* Update position */
  if (update_position) {
    MeloPlayerClass *class = MELO_PLAYER_GET_CLASS (player);

    /* Get current position */
    if (class->get_position)
      melo_player_update_position (player, class->get_position (player));
  }
}

/**
 * melo_player_update_position:
 * @player: a #MeloPlayer
 * @position: the current playing position in the media (in ms)
 *
 * This function should be used by the derived #MeloPlayer class implementations
 * to update the player position (at start or after a seek).
 * It will send an event to all listeners in order to notify of position update.
 *
 * This function is a protected function and is only intended for derived class.
 */
void
melo_player_update_position (MeloPlayer *player, unsigned int position)
{
  MeloPlayerPrivate *priv;

  if (!player)
    return;

  /* Update position */
  priv = melo_player_get_instance_private (player);

  /* Broadcast position change */
  if (player == melo_player_current)
    melo_events_broadcast (&melo_player_events,
        melo_player_message_position (position, priv->duration));
}

/**
 * melo_player_update_duration:
 * @player: a #MeloPlayer
 * @position: the current playing position in the media (in ms)
 * @duration: the duration of the current media, 0 for unknown or live (in ms)
 *
 * This function should be used by the derived #MeloPlayer class implementations
 * to update the player position (at start or after a seek) and the media
 * duration.
 * It will send an event to all listeners in order to notify of position and/or
 * duration update.
 *
 * This function is a protected function and is only intended for derived class.
 */
void
melo_player_update_duration (
    MeloPlayer *player, unsigned int position, unsigned int duration)
{
  MeloPlayerPrivate *priv;

  if (!player)
    return;

  /* Update duration */
  priv = melo_player_get_instance_private (player);
  priv->duration = duration;

  /* Broadcast position and duration change */
  if (player == melo_player_current)
    melo_events_broadcast (
        &melo_player_events, melo_player_message_position (position, duration));
}

static gboolean
melo_player_save_volume (gpointer user_data)
{
  /* Save settings */
  melo_settings_save (melo_player_settings);

  /* Remove timeout */
  melo_player_settings_source = 0;
  return FALSE;
}

/**
 * melo_player_update_volume:
 * @player: (nullable): a #MeloPlayer
 * @volume: the volume to set, between 0 and 1
 * @mute: enable / disable mute
 *
 * This function should be used by the derived #MeloPlayer class implementations
 * to update the player volume and mute status.
 * It will send an event to all listeners in order to notify of volume and/or
 * mute update.
 *
 * This function is a protected function and is only intended for derived class.
 */
void
melo_player_update_volume (MeloPlayer *player, float volume, bool mute)
{
  /* Clamp volume */
  if (volume < 0.f)
    volume = 0.f;
  if (volume > 1.f)
    volume = 1.f;

  /* Update volume */
  if (player) {
    MeloPlayerPrivate *priv = melo_player_get_instance_private (player);
    if (priv->volume)
      g_object_set (priv->volume, "volume", volume, "mute", mute, NULL);
  }
  melo_player_volume = volume;
  melo_player_mute = mute;

  /* Broadcast volume change */
  if (player == melo_player_current) {
    melo_events_broadcast (
        &melo_player_events, melo_player_message_volume (volume, mute));

    /* Update volume */
    if (melo_player_settings) {
      /* Set new values */
      melo_settings_entry_set_float (melo_player_settings_volume, volume);
      melo_settings_entry_set_boolean (melo_player_settings_mute, mute);

      /* Delay save */
      if (!melo_player_settings_source)
        melo_player_settings_source =
            g_timeout_add_seconds (10, melo_player_save_volume, NULL);
    }
  }
}

/**
 * melo_player_eos:
 * @player: a #MeloPlayer
 *
 * This function should be used by the derived #MeloPlayer class implementations
 * to signal an end of stream, when media finished to play. The next media in
 * the playlist is played automatically or if the playlist is finished, the
 * player state is set to #MELO_PLAYER_STATE_STOPPED and if a med
 *
 * This function is a protected function and is only intended for derived class.
 */
void
melo_player_eos (MeloPlayer *player)
{
  if (!player)
    return;

  /* Play next media in playlist */
  if (melo_playlist_play_next ())
    return;

  /* Set state to STOPPED */
  melo_player_update_state (player, MELO_PLAYER_STATE_STOPPED);
}

/**
 * melo_player_error:
 * @player: a #MeloPlayer
 * @error: (nullable): the error message to set
 *
 * This function should be used by the derived #MeloPlayer class implementations
 * to signal an error. If the error is not recoverable, the player should be
 * stopped (stopped state) before this call, with melo_player_update_state(). If
 * the state is not set to stopped, the next media in playlist will be played.
 * It will send an event to all listeners in order to notify the error.
 *
 * This function is a protected function and is only intended for derived class.
 */
void
melo_player_error (MeloPlayer *player, const char *error)
{
  MeloPlayerPrivate *priv;

  if (!player)
    return;

  /* Get private data */
  priv = melo_player_get_instance_private (player);

  /* Broadcast error */
  if (player == melo_player_current)
    melo_events_broadcast (
        &melo_player_events, melo_player_message_error (error));

  /* Play next media in playlist */
  if (player == melo_player_current && priv->state != MELO_PLAYER_STATE_NONE &&
      priv->state != MELO_PLAYER_STATE_STOPPED && !melo_playlist_play_next ())
    melo_player_update_state (player, MELO_PLAYER_STATE_STOPPED);
}

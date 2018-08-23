/*
 * melo_player.c: Player base class
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

#include "melo_event.h"
#include "melo_player.h"

/**
 * SECTION:melo_player
 * @title: MeloPlayer
 * @short_description: Base class for Melo Player
 *
 * #MeloPlayer is the main class to handle media playing with a full control
 * interface.
 *
 * The #MeloPlayerState and #MeloPlayerStatus are provided to give maximum
 * informations on current media handled by the player.
 * In addition to playing a single media, the #MeloPlayer can be associated
 * with a #MeloPlaylist which handles a complete play list of media. This
 * association should be done just after instantiation and before associating
 * the player instance with a #MeloBrowser or registering into a #MeloModule.
 *
 * A complete safe status handling (with #MeloTags) is available with
 * #MeloPlayer through all status helpers. Actually, the #MeloPlayer subclass
 * does not need to handle a #MeloPlayerStatus instance since the base class
 * already embed one instance and offers many safe-thread helpers to change the
 * values. It is highly recommended to use the internal status.
 * Moreover, the event system of Melo (see #MeloEvent) is supported internally
 * to generate automatically all player events when using the status helpers.
 *
 * The path provided in melo_player_add(), melo_player_load() and
 * melo_player_play() has no rules but it is recommended to use a path using
 * same schema than URI.
 *
 * Every instance of #MeloPlayer is automatically stored in a global list in
 * order to get a #MeloPlayer from any place with only the ID provided during
 * instantiation with melo_player_new().
 */

/* Internal player list */
G_LOCK_DEFINE_STATIC (melo_player_mutex);
static GHashTable *melo_player_hash = NULL;
static GList *melo_player_list = NULL;

struct _MeloPlayerStatusPrivate {
  GMutex mutex;
  gint ref_count;

  /* Strings */
  gchar *name;
  gchar *error;

  /* Tags */
  MeloTags *tags;
};

struct _MeloPlayerPrivate {
  GMutex mutex;
  gchar *id;
  gchar *name;
  MeloPlayerInfo info;
  MeloPlayerStatus *status;
  gint64 last_update;
};

enum {
  PROP_0,
  PROP_ID,
  PROP_NAME,
  PROP_LAST
};

static void melo_player_set_property (GObject *object, guint property_id,
                                      const GValue *value, GParamSpec *pspec);
static void melo_player_get_property (GObject *object, guint property_id,
                                      GValue *value, GParamSpec *pspec);

static MeloPlayerStatus *melo_player_status_new (MeloPlayerState state,
                                                 const gchar *name,
                                                 MeloTags *tags);

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (MeloPlayer, melo_player, G_TYPE_OBJECT)

static void
melo_player_finalize (GObject *gobject)
{
  MeloPlayer *player = MELO_PLAYER (gobject);
  MeloPlayerPrivate *priv = melo_player_get_instance_private (player);

  /* Free player status */
  melo_player_status_unref (priv->status);

  /* Send a delete player event */
  melo_event_player_delete (priv->id);

  /* Lock player list */
  G_LOCK (melo_player_mutex);

  /* Remove object from player list */
  melo_player_list = g_list_remove (melo_player_list, player);
  g_hash_table_remove (melo_player_hash, priv->id);

  /* Unlock player list */
  G_UNLOCK (melo_player_mutex);

  /* Free player ID and name */
  g_free (priv->id);
  g_free (priv->name);

  /* Unref attached playlist */
  if (player->playlist)
    g_object_unref (player->playlist);

  /* Free player mutex */
  g_mutex_clear (&priv->mutex);

  /* Chain up to the parent class */
  G_OBJECT_CLASS (melo_player_parent_class)->finalize (gobject);
}

static void
melo_player_class_init (MeloPlayerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  /* Add custom finalize() function */
  object_class->finalize = melo_player_finalize;
  object_class->set_property = melo_player_set_property;
  object_class->get_property = melo_player_get_property;

  /**
   * MeloPlayer:id:
   *
   * The ID of the player. This must be set during the construct and it can
   * be only read after instantiation.
   */
  g_object_class_install_property (object_class, PROP_ID,
      g_param_spec_string ("id", "ID", "Player ID", NULL,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                           G_PARAM_STATIC_STRINGS));

  /**
   * MeloPlayer:name:
   *
   * The name of the player. This must be set during the construct and it can
   * be only read after instantiation.
   */
  g_object_class_install_property (object_class, PROP_NAME,
      g_param_spec_string ("name", "Name", "Player name", NULL,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                           G_PARAM_STATIC_STRINGS));
}

static void
melo_player_init (MeloPlayer *self)
{
  MeloPlayerPrivate *priv = melo_player_get_instance_private (self);

  self->priv = priv;
  priv->id = NULL;
  priv->name = NULL;
  priv->info.playlist_id = NULL;
  priv->status = melo_player_status_new (MELO_PLAYER_STATE_NONE, NULL, NULL);

  /* Init player mutex */
  g_mutex_init (&priv->mutex);
}

/**
 * melo_player_get_id:
 * @player: the player
 *
 * Get the #MeloPlayer ID.
 *
 * Returns: the ID of the #MeloPlayer.
 */
const gchar *
melo_player_get_id (MeloPlayer *player)
{
  return player->priv->id;
}

/**
 * melo_player_get_name:
 * @player: the player
 *
 * Get the #MeloPlayer name.
 *
 * Returns: the name of the #MeloPlayer.
 */
const gchar *
melo_player_get_name (MeloPlayer *player)
{
  return player->priv->name;
}

static void
melo_player_set_property (GObject *object, guint property_id,
                          const GValue *value, GParamSpec *pspec)
{
  MeloPlayer *player = MELO_PLAYER (object);

  switch (property_id) {
    case PROP_ID:
      g_free (player->priv->id);
      player->priv->id = g_value_dup_string (value);
      break;
    case PROP_NAME:
      g_free (player->priv->name);
      player->priv->name = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
melo_player_get_property (GObject *object, guint property_id, GValue *value,
                          GParamSpec *pspec)
{
  MeloPlayer *player = MELO_PLAYER (object);

  switch (property_id) {
    case PROP_ID:
      g_value_set_string (value, melo_player_get_id (player));
      break;
    case PROP_NAME:
      g_value_set_string (value, melo_player_get_name (player));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

/**
 * melo_player_get_info:
 * @player: the player
 *
 * Get the details of the #MeloPlayer.
 *
 * Returns: a #MeloPlayerInfo or %NULL if get_info callback is not defined.
 */
const MeloPlayerInfo *
melo_player_get_info (MeloPlayer *player)
{
  MeloPlayerClass *pclass = MELO_PLAYER_GET_CLASS (player);
  MeloPlayerPrivate *priv = player->priv;

  /* Copy info from sub module */
  if (pclass->get_info)
    priv->info = *pclass->get_info (player);

  /* Update player name */
  if (!priv->info.name)
    priv->info.name = priv->name;

  /* Update playlist ID */
  if (!priv->info.playlist_id && player->playlist)
    priv->info.playlist_id = melo_playlist_get_id (player->playlist);

  /* Update available controls */
  if (pclass->set_state)
    priv->info.control.state = TRUE;
  if (pclass->prev)
    priv->info.control.prev = TRUE;
  if (pclass->next)
    priv->info.control.next = TRUE;
  if (pclass->set_volume)
    priv->info.control.volume = TRUE;
  if (pclass->set_mute)
    priv->info.control.mute = TRUE;

  return &priv->info;
}

/**
 * melo_player_get_player_by_id:
 * @id: the #MeloPlayer ID to retrieve
 *
 * Get an instance of the #MeloPlayer with its ID.
 *
 * Returns: (transfer full): the #MeloPlayer instance or %NULL if not found.
 * Use g_object_unref() after usage.
 */
MeloPlayer *
melo_player_get_player_by_id (const gchar *id)
{
  MeloPlayer *play;

  /* Lock player list */
  G_LOCK (melo_player_mutex);

  /* Find player by id */
  play = g_hash_table_lookup (melo_player_hash, id);

  /* Increment reference count */
  if (play)
    g_object_ref (play);

  /* Unlock player list */
  G_UNLOCK (melo_player_mutex);

  return play;
}

/**
 * melo_player_get_list:
 *
 * Get a #GList of all #MeloPlayer instance registered.
 *
 * Returns: (transfer full): a #GList of all #MeloPlayer instance. You must
 * free list and its data when you are done with it. You can use
 * g_list_free_full() with g_object_unref() to do this.
 */
GList *
melo_player_get_list ()
{
  GList *list = NULL;

  /* Lock player list */
  G_LOCK (melo_player_mutex);

  /* Copy list */
  if (melo_player_list)
    list = g_list_copy_deep (melo_player_list, (GCopyFunc) g_object_ref, NULL);

  /* Unlock player list */
  G_UNLOCK (melo_player_mutex);

  return list;
}

/**
 * melo_player_new:
 * @type: the type ID of the #MeloPlayer subtype to instantiate
 * @id: the #MeloPlayer ID to use for the new instance
 * @name: the #MeloPlayer name to use for the new instance
 *
 * Instantiate a new #MeloPlayer subtype with the @id and @name provided. The
 * new player instance is stored in a global list to be retrieved in a list
 * with melo_player_get_list() or by its ID with melo_player_get_player_by_id().
 *
 * Returns: (transfer full): the new #MeloPlayer instance or %NULL if failed.
 */
MeloPlayer *
melo_player_new (GType type, const gchar *id, const gchar *name)
{
  MeloPlayer *play;

  g_return_val_if_fail (id, NULL);
  g_return_val_if_fail (g_type_is_a (type, MELO_TYPE_PLAYER), NULL);

  /* Lock player list */
  G_LOCK (melo_player_mutex);

  /* Create player list */
  if (!melo_player_hash)
    melo_player_hash = g_hash_table_new_full (g_str_hash, g_str_equal,
                                               g_free, NULL);

  /* Check if ID is already used */
  if (g_hash_table_lookup (melo_player_hash, id))
    goto failed;

  /* Create a new instance of player */
  play = g_object_new (type, "id", id, "name", name, NULL);
  if (!play)
    goto failed;

  /* Add new player instance to player list */
  g_hash_table_insert (melo_player_hash, g_strdup (id), play);
  melo_player_list = g_list_append (melo_player_list, play);

  /* Unlock player list */
  G_UNLOCK (melo_player_mutex);

  /* Send a new player event */
  melo_event_player_new (id, melo_player_get_info(play));

  return play;

failed:
  G_UNLOCK (melo_player_mutex);
  return NULL;
}

/**
 * melo_player_set_playlist:
 * @player: the player
 * @playlist: the #MeloPlaylist to associate with
 *
 * Associate a #MeloPlaylist to this player in order to handle the play list
 * media.
 * This function should be called just after instantiation of the #MeloPlayer
 * and just before association of the player with a #meloBrowser or registration
 * in a #MeloModule.
 *
 * This functions takes a reference on the #MeloPlaylist.
 */
void
melo_player_set_playlist (MeloPlayer *player, MeloPlaylist *playlist)
{
  if (player->playlist)
    g_object_unref (player->playlist);

  player->playlist = g_object_ref (playlist);
}

/**
 * melo_player_get_playlist:
 * @player: the player
 *
 * Get the associated #MeloPlaylist instance.
 *
 * Returns: (transfer full): an instance of the #MeloPlaylist associated with
 * the current player. After usage, use g_object_unref().
*/
MeloPlaylist *
melo_player_get_playlist (MeloPlayer *player)
{
  g_return_val_if_fail (player->playlist, NULL);

  return g_object_ref (player->playlist);
}

/**
 * melo_player_add:
 * @player: the player
 * @path: path (or URI) of the media to add
 * @name: display name of the media
 * @tags: a #MeloTags with tags of the media
 *
 * Add a media pointed by @path to the player. A reference of the #MeloTags is
 * taken internally, so if @tags is not needed after this call, the
 * melo_tags_unref() should be called.
 *
 * Returns: %TRUE if media has been successfully done; %FALSE otherwise.
 */
gboolean
melo_player_add (MeloPlayer *player, const gchar *path, const gchar *name,
                 MeloTags *tags)
{
  MeloPlayerClass *pclass = MELO_PLAYER_GET_CLASS (player);

  g_return_val_if_fail (pclass->add, FALSE);

  return pclass->add (player, path, name, tags);
}

/**
 * melo_player_load:
 * @player: the player
 * @path: path (or URI) of the media to load
 * @name: display name of the media
 * @tags: a #MeloTags with tags of the media
 * @insert: insert the media into the playlist
 * @stopped: set the player state to stopped
 *
 * Load a media pointed by @path to the player. A reference of the #MeloTags is
 * taken internally, so if @tags is not needed after this call, the
 * melo_tags_unref() should be called.
 * By default the player state is set to MELO_PLAYER_STATE_PAUSED_LOADING, but
 * if @stopped is %TRUE, the player state is set to MELO_PLAYER_STATE_STOPPED.
 *
 * Returns: %TRUE if media has been successfully done; %FALSE otherwise.
 */
gboolean
melo_player_load (MeloPlayer *player, const gchar *path,
                  const gchar *name, MeloTags *tags, gboolean insert,
                  gboolean stopped)
{
  MeloPlayerClass *pclass = MELO_PLAYER_GET_CLASS (player);

  g_return_val_if_fail (pclass->load, FALSE);

  return pclass->load (player, path, name, tags, insert, stopped);
}

/**
 * melo_player_play:
 * @player: the player
 * @path: path (or URI) of the media to play
 * @name: display name of the media
 * @tags: a #MeloTags with tags of the media
 * @insert: insert the media into the playlist
 *
 * Play a media pointed by @path to the player. A reference of the #MeloTags is
 * taken internally, so if @tags is not needed after this call, the
 * melo_tags_unref() should be called.
 *
 * Returns: %TRUE if media has been successfully done; %FALSE otherwise.
 */
gboolean
melo_player_play (MeloPlayer *player, const gchar *path, const gchar *name,
                  MeloTags *tags, gboolean insert)
{
  MeloPlayerClass *pclass = MELO_PLAYER_GET_CLASS (player);

  g_return_val_if_fail (pclass->play, FALSE);

  return pclass->play (player, path, name, tags, insert);
}

/**
 * melo_player_set_state:
 * @player: the player
 * @state: the #MeloPlayerState to apply
 *
 * Apply a new #MeloPlayerState to the player. If this state is unreachable, a
 * different state will be returned.
 *
 * Returns: the #MeloPlayerState applied, which could differ from @state.
 */
MeloPlayerState
melo_player_set_state (MeloPlayer *player, MeloPlayerState state)
{
  MeloPlayerClass *pclass = MELO_PLAYER_GET_CLASS (player);

  /* State control is not available */
  if (!pclass->set_state)
    return melo_player_get_state (player);

  /* Set new state */
  state = pclass->set_state (player, state);

  /* Update state */
  melo_player_set_status_state (player, state);

  return state;
}

/**
 * melo_player_prev:
 * @player: the player
 *
 * Play previous media in playlist. This function is available only if the
 * playlist is supported by the #MeloPlayer instance.
 *
 * Returns: %TRUE if successful, %FALSE otherwise.
 */
gboolean
melo_player_prev (MeloPlayer *player)
{
  MeloPlayerClass *pclass = MELO_PLAYER_GET_CLASS (player);

  g_return_val_if_fail (pclass->prev, FALSE);

  return pclass->prev (player);
}

/**
 * melo_player_next:
 * @player: the player
 *
 * Play next media in playlist. This function is available only if the playlist
 * is supported by the #MeloPlayer instance.
 *
 * Returns: %TRUE if successful, %FALSE otherwise.
 */
gboolean
melo_player_next (MeloPlayer *player)
{
  MeloPlayerClass *pclass = MELO_PLAYER_GET_CLASS (player);

  g_return_val_if_fail (pclass->next, FALSE);

  return pclass->next (player);
}

/**
 * melo_player_set_pos:
 * @player: the player
 * @pos: the new position (in ms)
 *
 * Seek current stream to @pos position (in ms).
 *
 * Returns: the effective position which could differ from @pos.
 */
gint
melo_player_set_pos (MeloPlayer *player, gint pos)
{
  MeloPlayerClass *pclass = MELO_PLAYER_GET_CLASS (player);

  g_return_val_if_fail (pclass->set_pos, 0);

  return pclass->set_pos (player, pos);
}

/**
 * melo_player_set_volume:
 * @player: the player
 * @volume: the new volume
 *
 * Set the player volume.
 *
 * Returns: the effective volume which could differ from @volume.
 */
gdouble
melo_player_set_volume (MeloPlayer *player, gdouble volume)
{
  MeloPlayerClass *pclass = MELO_PLAYER_GET_CLASS (player);

  /* Volume control is not available */
  if (!pclass->set_volume)
    return melo_player_get_volume (player);

  /* Set volume */
  volume = pclass->set_volume (player, volume);

  /* Update volume */
  melo_player_set_status_volume (player, volume);

  return volume;
}

/**
 * melo_player_set_mute:
 * @player: the player
 * @mute: the new mute state
 *
 * Set the player mute state.
 *
 * Returns: the effective mute state which could differ from @mute.
 */
gboolean
melo_player_set_mute (MeloPlayer *player, gboolean mute)
{
  MeloPlayerClass *pclass = MELO_PLAYER_GET_CLASS (player);

  /* Mute control is note available */
  if (!pclass->set_mute)
    return melo_player_get_mute (player);

  /* Set mute */
  mute = pclass->set_mute (player, mute);

  /* Update mute */
  melo_player_set_status_mute (player, mute);

  return mute;
}

/**
 * melo_player_get_status:
 * @player: the player
 * @timestamp: the base timestamp
 *
 * Get the current player status stored in a #MeloPlayerStatus. If the
 * @timestamp is more recent than last status update, %NULL will be returned.
 * It prevents copying same data each call when this function is called
 * periodically by a timer.
 * To use correctly the @timestamp value, for the first call, it should be set
 * to zero, than it should be the same value as returned in the
 * #MeloPlayerStatus.
 *
 * Returns: (transfer full): a reference to a #MeloPlayerStatus containing the
 * last player status. After use, call melo_player_status_unref().
 */
MeloPlayerStatus *
melo_player_get_status (MeloPlayer *player, gint64 *timestamp)
{
  MeloPlayerPrivate *priv = player->priv;
  MeloPlayerStatus *status;

  /* */
  if (timestamp) {
    if (*timestamp && *timestamp >= priv->last_update)
      return NULL;
    *timestamp = priv->last_update;
  }

  /* Copy status */
  g_mutex_lock (&priv->mutex);
  status = melo_player_status_ref (priv->status);
  g_mutex_unlock (&priv->mutex);

  /* Update position */
  status->pos = melo_player_get_pos (player);

  return status;
}

/**
 * melo_player_get_state:
 * @player: the player
 *
 * Get the current player state.
 *
 * Returns: a #MeloPlayerState containing current player state.
 */
MeloPlayerState
melo_player_get_state (MeloPlayer *player)
{
  MeloPlayerPrivate *priv = player->priv;
  MeloPlayerState state;

  /* Get player state */
  g_mutex_lock (&priv->mutex);
  state = priv->status->state;
  g_mutex_unlock (&priv->mutex);

  return state;
}

/**
 * melo_player_get_media_name:
 * @player: the player
 *
 * Get the display name of the current media loaded in the player.
 *
 * Returns: (transfer full): a string containing the display name of the current
 * media loaded. After use, call g_free().
 */
gchar *
melo_player_get_media_name (MeloPlayer *player)
{
  MeloPlayerPrivate *priv = player->priv;
  gchar *name;

  /* Copy media name */
  g_mutex_lock (&priv->mutex);
  name = melo_player_status_get_name (priv->status);
  g_mutex_unlock (&priv->mutex);

  return name;
}

/**
 * melo_player_get_pos:
 * @player: the player
 *
 * Get the current stream position (in ms).
 *
 * Returns: the current stream position (in ms).
 */
gint
melo_player_get_pos (MeloPlayer *player)
{
  MeloPlayerClass *pclass = MELO_PLAYER_GET_CLASS (player);

  g_return_val_if_fail (pclass->get_pos, 0);

  return pclass->get_pos (player);
}

/**
 * melo_player_get_volume:
 * @player: the player
 *
 * Get the current player volume.
 *
 * Returns: the current volume.
 */
gdouble
melo_player_get_volume (MeloPlayer *player)
{
  MeloPlayerPrivate *priv = player->priv;
  gdouble volume;

  /* Get current volume */
  g_mutex_lock (&priv->mutex);
  volume = priv->status->volume;
  g_mutex_unlock (&priv->mutex);

  return volume;
}

/**
 * melo_player_get_mute:
 * @player: the player
 *
 * Get the current player mute state.
 *
 * Returns: the current mute state.
 */
gboolean
melo_player_get_mute (MeloPlayer *player)
{
  MeloPlayerPrivate *priv = player->priv;
  gboolean mute;

  /* Get current mute */
  g_mutex_lock (&priv->mutex);
  mute = priv->status->mute;
  g_mutex_unlock (&priv->mutex);

  return mute;
}

/**
 * melo_player_get_tags:
 * @player: the player
 *
 * Get the tags as #MeloTags  of the current media loaded in the player.
 *
 * Returns: (transfer full): a reference of the #MeloTags of the current media
 * loaded. After use, call melo_tags_unref().
 */
MeloTags *
melo_player_get_tags (MeloPlayer *player)
{
  MeloPlayerPrivate *priv = player->priv;
  MeloTags *tags;

  /* Get tags */
  g_mutex_lock (&priv->mutex);
  tags = melo_player_status_get_tags (priv->status);
  g_mutex_unlock (&priv->mutex);

  return tags;
}

static MeloPlayerStatus *
melo_player_status_new (MeloPlayerState state, const gchar *name,
                        MeloTags *tags)
{
  MeloPlayerStatus *status;

  /* Allocate new status */
  status = g_slice_new0 (MeloPlayerStatus);
  if (!status)
    return NULL;

  /* Allocate private data */
  status->priv = g_slice_new0 (MeloPlayerStatusPrivate);
  if (!status->priv) {
    g_slice_free (MeloPlayerStatus, status);
    return NULL;
  }

  /* Init private mutex */
  g_mutex_init (&status->priv->mutex);

  /* Init reference counter */
  status->priv->ref_count = 1;

  /* Initialize status */
  status->state = state;
  status->volume = 1.0;
  status->priv->name = g_strdup (name);
  status->priv->tags = tags;

  return status;
}

static void
melo_player_updated (MeloPlayerPrivate *priv)
{
  priv->last_update = g_get_monotonic_time ();
}

/**
 * melo_player_reset_status:
 * @player: the player
 * @state: the new state to use
 * @name: the new display name to use, can be %NULL
 * @tags: the new #MeloTags to use, can be %NULL
 *
 * Reset the internal #MeloPlayerStatus with fresh values as @state, @name and
 * @tags. Other values are set to 0 or %NULL. It is very useful when a new media
 * is loaded in player.
 * This function should be only called by the #MeloPlayer subclass.
 *
 * Returns: %TRUE if the status has been reset, %FALSE otherwise.
 */
gboolean
melo_player_reset_status (MeloPlayer *player, MeloPlayerState state,
                          const gchar *name, MeloTags *tags)
{
  MeloPlayerPrivate *priv = player->priv;
  MeloPlayerStatus *status;

  /* Create a new status */
  status = melo_player_status_new (state, name, tags);
  if (!status)
    return FALSE;

  /* Lock player status access */
  g_mutex_lock (&priv->mutex);

  /* Copy values from previous status */
  status->volume = priv->status->volume;
  status->mute = priv->status->mute;
  status->has_prev = priv->status->has_prev;
  status->has_next = priv->status->has_next;

  /* Set new player status */
  melo_player_status_unref (priv->status);
  priv->status = status;

  /* Unlock player status access */
  g_mutex_unlock (&priv->mutex);

  return TRUE;
}

/**
 * melo_player_set_status_state:
 * @player: the player
 * @state: the new state to use
 *
 * Set the new state of the internal #MeloPlayerStatus.
 * This function should be only called by the #MeloPlayer subclass.
 */
void
melo_player_set_status_state (MeloPlayer *player, MeloPlayerState state)
{
  MeloPlayerPrivate *priv = player->priv;

  /* Update state */
  g_mutex_lock (&priv->mutex);
  priv-> status->state = state;
  g_mutex_unlock (&priv->mutex);

  /* Send 'player state' event */
  melo_event_player_state (priv->id, state);
  melo_player_updated (priv);
}

/**
 * melo_player_set_status_buffering:
 * @player: the player
 * @state: the new state to use
 * @percent: the new buffering percentage to use
 *
 * Set the new state and buffering percentage of the internal #MeloPlayerStatus.
 * This function should be only called by the #MeloPlayer subclass.
 */
void
melo_player_set_status_buffering (MeloPlayer *player, MeloPlayerState state,
                                  guint percent)
{
  MeloPlayerPrivate *priv = player->priv;

  /* Update state and buffer percent */
  g_mutex_lock (&priv->mutex);
  priv->status->state = state;
  priv->status->buffer_percent = percent;
  g_mutex_unlock (&priv->mutex);

  /* Send 'player buffering' event */
  melo_event_player_buffering (priv->id, state, percent);
  melo_player_updated (priv);
}

/**
 * melo_player_set_status_pos:
 * @player: the player
 * @pos: the new stream position to use
 *
 * Set the new stream position of the internal #MeloPlayerStatus. It doesn't
 * perform any seek in the stream!
 * This function should be only called by the #MeloPlayer subclass.
 */
void
melo_player_set_status_pos (MeloPlayer *player, gint pos)
{
  MeloPlayerPrivate *priv = player->priv;

  /* Update position */
  g_mutex_lock (&priv->mutex);
  priv->status->pos = pos;
  g_mutex_unlock (&priv->mutex);

  /* Send 'player seek' event */
  melo_event_player_seek (priv->id, pos);
  melo_player_updated (priv);
}

/**
 * melo_player_set_status_duration:
 * @player: the player
 * @duration: the new duration to use (in ms)
 *
 * Set the new duration (in ms) of the internal #MeloPlayerStatus.
 * This function should be only called by the #MeloPlayer subclass.
 */
void
melo_player_set_status_duration (MeloPlayer *player, gint duration)
{
  MeloPlayerPrivate *priv = player->priv;

  /* Update duration */
  g_mutex_lock (&priv->mutex);
  priv->status->duration = duration;
  g_mutex_unlock (&priv->mutex);

  /* Send 'player duration' event */
  melo_event_player_duration (priv->id, duration);
  melo_player_updated (priv);
}

/**
 * melo_player_set_status_playlist:
 * @player: the player
 * @has_prev: %TRUE if a media is before in playlist
 * @has_next: %TRUE if a media is after in playlist
 *
 * Set the new has previous / next media in playlist status of the internal
 * #MeloPlayerStatus.
 * This function should be only called by the #MeloPlayer subclass.
 */
void
melo_player_set_status_playlist (MeloPlayer *player, gboolean has_prev,
                                 gboolean has_next)
{
  MeloPlayerPrivate *priv = player->priv;

  /* Update playlist */
  g_mutex_lock (&priv->mutex);
  priv->status->has_prev = has_prev;
  priv->status->has_next = has_next;
  g_mutex_unlock (&priv->mutex);

  /* Send 'player playlist' event */
  melo_event_player_playlist (priv->id, has_prev, has_next);
  melo_player_updated (priv);
}

/**
 * melo_player_set_status_volume:
 * @player: the player
 * @volume: the new volume to use
 *
 * Set the new volume of the internal #MeloPlayerStatus.
 * This function should be only called by the #MeloPlayer subclass.
 */
void
melo_player_set_status_volume (MeloPlayer *player, gdouble volume)
{
  MeloPlayerPrivate *priv = player->priv;

  /* Update volume */
  g_mutex_lock (&priv->mutex);
  priv->status->volume = volume;
  g_mutex_unlock (&priv->mutex);

  /* Send 'player volume' event */
  melo_event_player_volume (priv->id, volume);
  melo_player_updated (priv);
}

/**
 * melo_player_set_status_mute:
 * @player: the player
 * @mute: the new mute state to use
 *
 * Set the new mute state of the internal #MeloPlayerStatus.
 * This function should be only called by the #MeloPlayer subclass.
 */
void
melo_player_set_status_mute (MeloPlayer *player, gboolean mute)
{
  MeloPlayerPrivate *priv = player->priv;

  /* Update mute */
  g_mutex_lock (&priv->mutex);
  priv->status->mute = mute;
  g_mutex_unlock (&priv->mutex);

  /* Send 'player mute' event */
  melo_event_player_mute (priv->id, mute);
  melo_player_updated (priv);
}

static void
melo_player_status_set_name (MeloPlayerStatus *status, const gchar *name)
{
  MeloPlayerStatusPrivate *priv = status->priv;

  /* Lock name access */
  g_mutex_lock (&priv->mutex);

  /* Update name */
  g_free (priv->name);
  priv->name = g_strdup (name);

  /* Unlock name access */
  g_mutex_unlock (&priv->mutex);
}

/**
 * melo_player_set_status_name:
 * @player: the player
 * @name: the new display name to use, can be %NULL
 *
 * Set the new media display name of the internal #MeloPlayerStatus.
 * This function should be only called by the #MeloPlayer subclass.
 */
void
melo_player_set_status_name (MeloPlayer *player, const gchar *name)
{
  MeloPlayerPrivate *priv = player->priv;

  /* Update name */
  g_mutex_lock (&priv->mutex);
  melo_player_status_set_name (priv->status, name);
  g_mutex_unlock (&priv->mutex);

  /* Send 'player name' event */
  melo_event_player_name (priv->id, name);
  melo_player_updated (priv);
}

static void
melo_player_status_set_error (MeloPlayerStatus *status, const gchar *error)
{
  MeloPlayerStatusPrivate *priv = status->priv;

  /* Lock error access */
  g_mutex_lock (&priv->mutex);

  /* Update error */
  g_free (priv->error);
  priv->error = g_strdup (error);
  if (error)
    status->state = MELO_PLAYER_STATE_ERROR;

  /* Unlock error access */
  g_mutex_unlock (&priv->mutex);
}

/**
 * melo_player_set_status_error:
 * @player: the player
 * @error: the new error string to use, can be %NULL
 *
 * Set the new error string of the internal #MeloPlayerStatus.
 * This function should be only called by the #MeloPlayer subclass.
 */
void
melo_player_set_status_error (MeloPlayer *player, const gchar *error)
{
  MeloPlayerPrivate *priv = player->priv;

  /* Update error */
  g_mutex_lock (&priv->mutex);
  melo_player_status_set_error (priv->status, error);
  g_mutex_unlock (&priv->mutex);

  /* Send 'player error' event */
  melo_event_player_error (priv->id, error);
  melo_player_updated (priv);
}

static void
melo_player_status_take_tags (MeloPlayerStatus *status, MeloTags *tags)
{
  MeloPlayerStatusPrivate *priv = status->priv;

  /* Lock tags access */
  g_mutex_lock (&priv->mutex);

  /* Free previous tags */
  if (priv->tags)
    melo_tags_unref (priv->tags);

  /* Set new tags */
  priv->tags = tags;

  /* Update timestamp */
  melo_tags_update (tags);

  /* Unlock tags access */
  g_mutex_unlock (&priv->mutex);
}

/**
 * melo_player_take_status_tags:
 * @player: the player
 * @tags: the new #MeloTags to use, can be %NULL
 *
 * Set the new #MeloTags of the internal #MeloPlayerStatus. It takes ownership
 * of @tags reference.
 * This function should be only called by the #MeloPlayer subclass.
 */
void
melo_player_take_status_tags (MeloPlayer *player, MeloTags *tags)
{
  MeloPlayerPrivate *priv = player->priv;

  /* Set new tags */
  g_mutex_lock (&priv->mutex);
  melo_player_status_take_tags (priv->status, tags);
  g_mutex_unlock (&priv->mutex);

  /* Send 'player tags' event */
  melo_event_player_tags (priv->id, tags);
  melo_player_updated (priv);
}

/**
 * melo_player_set_status_tags:
 * @player: the player
 * @tags: the new #MeloTags to use, can be %NULL
 *
 * Set the new #MeloTags of the internal #MeloPlayerStatus. A new reference of
 * @tags is used.
 * This function should be only called by the #MeloPlayer subclass.
 */
void
melo_player_set_status_tags (MeloPlayer *player, MeloTags *tags)
{
  if (!tags)
    return;

  melo_tags_ref (tags);
  melo_player_take_status_tags (player, tags);
}

/**
 * melo_player_status_ref:
 * @status: the player status
 *
 * Increment the reference counter of the #MeloPlayerStatus.
 *
 * Returns: (transfer full): a pointer of the #MeloPlayerStatus.
 */
MeloPlayerStatus *
melo_player_status_ref (MeloPlayerStatus *status)
{
  status->priv->ref_count++;
  return status;
}

/**
 * melo_player_status_unref:
 * @status: the player status
 *
 * Decrement the reference counter of the #MeloPlayerStatus. If it reaches zero,
 * the #MeloPlayerStatus is freed.
 */
void
melo_player_status_unref (MeloPlayerStatus *status)
{
  status->priv->ref_count--;
  if (status->priv->ref_count)
    return;

  /* Free status */
  g_free (status->priv->name);
  g_free (status->priv->error);
  if (status->priv->tags)
    melo_tags_unref (status->priv->tags);
  g_mutex_clear (&status->priv->mutex);
  g_slice_free (MeloPlayerStatusPrivate, status->priv);
  g_slice_free (MeloPlayerStatus, status);
}

/**
 * melo_player_status_get_error:
 * @status: the player status
 *
 * Get a copy of the current error string.
 *
 * Returns: (transfer full): a copy of the error string which must be freed with
 * g_free() after usage.
 */
gchar *
melo_player_status_get_error (const MeloPlayerStatus *status)
{
  MeloPlayerStatusPrivate *priv = status->priv;
  gchar *error;

  /* Lock error access */
  g_mutex_lock (&priv->mutex);

  /* Copy error */
  error = g_strdup (priv->error);

  /* Unlock error access */
  g_mutex_unlock (&priv->mutex);

  return error;
}

/**
 * melo_player_status_get_name:
 * @status: the player status
 *
 * Get a copy of the current media name string.
 *
 * Returns: (transfer full): a copy of the media name string which must be freed
 * with g_free() after usage.
 */
gchar *
melo_player_status_get_name (const MeloPlayerStatus *status)
{
  MeloPlayerStatusPrivate *priv = status->priv;
  gchar *name;

  /* Lock name access */
  g_mutex_lock (&priv->mutex);

  /* Copy name */
  name = g_strdup (priv->name);

  /* Unlock name access */
  g_mutex_unlock (&priv->mutex);

  return name;
}

/**
 * melo_player_status_get_tags:
 * @status: the player status
 *
 * Get the current #MeloTags stored in the status.
 *
 * Returns: (transfer full): a new reference of the current #MeloTags. After
 * use, the melo_tags_unref() must be called.
 */
MeloTags *
melo_player_status_get_tags (const MeloPlayerStatus *status)
{
  MeloPlayerStatusPrivate *priv = status->priv;
  MeloTags *tags = NULL;

  /* Lock tags access */
  g_mutex_lock (&priv->mutex);

  /* Get a reference to tags */
  if (priv->tags)
    tags = melo_tags_ref (priv->tags);

  /* Unlock tags access */
  g_mutex_unlock (&priv->mutex);

  return tags;
}

/**
 * melo_player_status_lock:
 * @status: the player status
 *
 * Lock the #MeloPlayerStatus in order to read sensitive values such as the
 * strings (see melo_player_status_lock_get_name() and
 * melo_player_status_lock_get_error()).
 */
void
melo_player_status_lock (const MeloPlayerStatus *status)
{
  /* Lock status access */
  g_mutex_lock (&status->priv->mutex);
}

/**
 * melo_player_status_unlock:
 * @status: the player status
 *
 * Unlock the #MeloPlayerStatus in order free exclusive access of the values.
 */
void
melo_player_status_unlock (const MeloPlayerStatus *status)
{
  /* Unlock status access */
  g_mutex_unlock (&status->priv->mutex);
}

/**
 * melo_player_status_lock_get_name:
 * @status: the player status
 *
 * Get the current media name string.
 * It should be called after melo_player_status_lock() call.
 *
 * Returns: the media name string in the #MeloPlayerStatus.
 */
const gchar *
melo_player_status_lock_get_name (const MeloPlayerStatus *status)
{
  return status->priv->name;
}

/**
 * melo_player_status_lock_get_error:
 * @status: the player status
 *
 * Get the current error string.
 * It should be called after melo_player_status_lock() call.
 *
 * Returns: the error string in the #MeloPlayerStatus.
 */
const gchar *
melo_player_status_lock_get_error (const MeloPlayerStatus *status)
{
  return status->priv->error;
}

static const gchar *melo_player_state_str[] = {
  [MELO_PLAYER_STATE_NONE] = "none",
  [MELO_PLAYER_STATE_LOADING] = "loading",
  [MELO_PLAYER_STATE_BUFFERING] = "buffering",
  [MELO_PLAYER_STATE_PLAYING] = "playing",
  [MELO_PLAYER_STATE_PAUSED_LOADING] = "paused_loading",
  [MELO_PLAYER_STATE_PAUSED_BUFFERING] = "paused_buffering",
  [MELO_PLAYER_STATE_PAUSED] = "paused",
  [MELO_PLAYER_STATE_STOPPED] = "stopped",
  [MELO_PLAYER_STATE_ERROR] = "error",
};

/**
 * melo_player_state_to_string:
 * @state: the player state
 *
 * Convert a #MeloPlayerState to a string.
 *
 * Returns: a string with the translated #MeloPlayerState, %NULL otherwise.
*/
const gchar *
melo_player_state_to_string (MeloPlayerState state)
{
  if (state >= MELO_PLAYER_STATE_COUNT)
    return NULL;
  return melo_player_state_str[state];
}

/**
 * melo_player_state_from_string:
 * @sstate: a string with player state
 *
 * Convert a string to a #MeloPlayerState.
 *
 * Returns: a #MeloPlayerState or MELO_PLAYER_STATE_NONE if no correspondence
 * has been found.
*/
MeloPlayerState
melo_player_state_from_string (const gchar *sstate)
{
  int i;

  if (!sstate)
    return MELO_PLAYER_STATE_NONE;

  for (i = 0; i < MELO_PLAYER_STATE_COUNT; i++)
    if (!g_strcmp0 (sstate, melo_player_state_str[i]))
      return i;

  return MELO_PLAYER_STATE_NONE;
}

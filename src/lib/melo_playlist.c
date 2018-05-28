/*
 * melo_playlist.c: Playlist base class
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

#include "melo_playlist.h"

/**
 * SECTION:melo_playlist
 * @title: MeloPlaylist
 * @short_description: Base class for Melo Playlist
 *
 * #MeloPlaylist is the main class to handle media playlist with a full control
 * interface.
 *
 * A #MeloPlaylist handles the media list played and/or to be played by a
 * #MeloPlayer. Any instance of a playlist must be attached to a player. It
 * handles a list of media with a state to store the current media playing.
 *
 * A list of the medias can be retrieved through melo_playlist_get_list() and
 * modified (sort, move, remove, ...) with many functions as
 * melo_playlist_sort(), melo_playlist_move(), melo_playlist_remove(), ...
 */

/* Internal playlist list */
G_LOCK_DEFINE_STATIC (melo_playlist_mutex);
static GHashTable *melo_playlist_hash = NULL;
static GList *melo_playlist_list = NULL;

struct _MeloPlaylistPrivate {
  gchar *id;
};

enum {
  PROP_0,
  PROP_ID,
  PROP_LAST
};

static void melo_playlist_set_property (GObject *object, guint property_id,
                                        const GValue *value, GParamSpec *pspec);
static void melo_playlist_get_property (GObject *object, guint property_id,
                                        GValue *value, GParamSpec *pspec);

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (MeloPlaylist, melo_playlist, G_TYPE_OBJECT)

static void
melo_playlist_finalize (GObject *gobject)
{
  MeloPlaylist *playlist = MELO_PLAYLIST (gobject);
  MeloPlaylistPrivate *priv = melo_playlist_get_instance_private (playlist);

  /* Lock playlist list */
  G_LOCK (melo_playlist_mutex);

  /* Remove object from playlist list */
  melo_playlist_list = g_list_remove (melo_playlist_list, playlist);
  g_hash_table_remove (melo_playlist_hash, priv->id);

  /* Unlock playlist list */
  G_UNLOCK (melo_playlist_mutex);

  /* Free private data */
  if (priv->id)
    g_free (priv->id);

  /* Chain up to the parent class */
  G_OBJECT_CLASS (melo_playlist_parent_class)->finalize (gobject);
}

static void
melo_playlist_class_init (MeloPlaylistClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  /* Add custom finalize() function */
  object_class->finalize = melo_playlist_finalize;
  object_class->set_property = melo_playlist_set_property;
  object_class->get_property = melo_playlist_get_property;

  /**
   * MeloPlaylist:id:
   *
   * The ID of the playlist. This must be set during the construct and it can
   * be only read after instantiation.
   */
  g_object_class_install_property (object_class, PROP_ID,
      g_param_spec_string ("id", "ID", "Playlist ID", NULL,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                           G_PARAM_STATIC_STRINGS));
}

static void
melo_playlist_init (MeloPlaylist *self)
{
  MeloPlaylistPrivate *priv = melo_playlist_get_instance_private (self);

  self->priv = priv;
  priv->id = NULL;
}

/**
 * melo_playlist_get_id:
 * @playlist: the playlist
 *
 * Get the #MeloPlaylist ID.
 *
 * Returns: the ID of the #MeloPlaylist.
 */
const gchar *
melo_playlist_get_id (MeloPlaylist *playlist)
{
  return playlist->priv->id;
}

static void
melo_playlist_set_property (GObject *object, guint property_id,
                            const GValue *value, GParamSpec *pspec)
{
  MeloPlaylist *playlist = MELO_PLAYLIST (object);

  switch (property_id) {
    case PROP_ID:
      g_free (playlist->priv->id);
      playlist->priv->id = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
melo_playlist_get_property (GObject *object, guint property_id, GValue *value,
                          GParamSpec *pspec)
{
  MeloPlaylist *playlist = MELO_PLAYLIST (object);

  switch (property_id) {
    case PROP_ID:
      g_value_set_string (value, melo_playlist_get_id (playlist));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

/**
 * melo_playlist_get_playlist_by_id:
 * @id: the #MeloPlaylist ID to retrieve
 *
 * Get an instance of the #MeloPlaylist with its ID.
 *
 * Returns: (transfer full): the #MeloPlaylist instance or %NULL if not found.
 * Use g_object_unref() after usage.
 */
MeloPlaylist *
melo_playlist_get_playlist_by_id (const gchar *id)
{
  MeloPlaylist *plist;

  /* Lock playlist list */
  G_LOCK (melo_playlist_mutex);

  /* Find playlist by id */
  plist = g_hash_table_lookup (melo_playlist_hash, id);

  /* Increment reference count */
  if (plist)
    g_object_ref (plist);

  /* Unlock playlist list */
  G_UNLOCK (melo_playlist_mutex);

  return plist;
}

/**
 * melo_playlist_new:
 * @type: the type ID of the #MeloPlaylist subtype to instantiate
 * @id: the #MeloPlaylist ID to use for the new instance
 *
 * Instantiate a new #MeloPlaylist subtype with the @id provided.
 *
 * Returns: (transfer full): the new #MeloPlaylist instance or %NULL if failed.
 */
MeloPlaylist *
melo_playlist_new (GType type, const gchar *id)
{
  MeloPlaylist *plist;

  g_return_val_if_fail (id, NULL);
  g_return_val_if_fail (g_type_is_a (type, MELO_TYPE_PLAYLIST), NULL);

  /* Lock playlist list */
  G_LOCK (melo_playlist_mutex);

  /* Create playlist list */
  if (!melo_playlist_hash)
    melo_playlist_hash = g_hash_table_new_full (g_str_hash, g_str_equal,
                                               g_free, NULL);

  /* Check if ID is already used */
  if (g_hash_table_lookup (melo_playlist_hash, id))
    goto failed;

  /* Create a new instance of playlist */
  plist = g_object_new (type, "id", id, NULL);
  if (!plist)
    goto failed;

  /* Add new playlist instance to playlist list */
  g_hash_table_insert (melo_playlist_hash, g_strdup (id), plist);
  melo_playlist_list = g_list_append (melo_playlist_list, plist);

  /* Unlock playlist list */
  G_UNLOCK (melo_playlist_mutex);

  return plist;

failed:
  G_UNLOCK (melo_playlist_mutex);
  return NULL;
}

/**
 * melo_playlist_set_player:
 * @playlist: the playlist
 * @player: the #MeloPlayer to associate
 *
 * Associate a #MeloPlayer to this playlist in order to play media.
 * This function should be called just after instantiation of the #MeloPlaylist.
 */
void
melo_playlist_set_player (MeloPlaylist *playlist, MeloPlayer *player)
{
  playlist->player = player;
}

/**
 * melo_playlist_get_player:
 * @playlist: the playlist
 *
 * Get the associated #MeloPlayer instance.
 *
 * Returns: (transfer full): an instance of the #MeloPlayer associated with
 * the current playlist. After usage, use g_object_unref().
*/
MeloPlayer *
melo_playlist_get_player (MeloPlaylist *playlist)
{
  g_return_val_if_fail (playlist->player, NULL);

  return g_object_ref (playlist->player);
}

/**
 * melo_playlist_get_list:
 * @playlist: the playlist
 * @tags_fields: the tag fields to fill in #MeloTags of the media items
 *
 * Get the list of media in the playlist.
 *
 * Returns: (transfer full): a #MeloPlaylistList with the current media
 * playlist or %NULL if an error has occurred.
 * Use melo_playlist_list_free() after usage.
 */
MeloPlaylistList *
melo_playlist_get_list (MeloPlaylist *playlist, MeloTagsFields tags_fields)
{
  MeloPlaylistClass *pclass = MELO_PLAYLIST_GET_CLASS (playlist);

  g_return_val_if_fail (pclass->get_list, NULL);

  return pclass->get_list (playlist, tags_fields);
}

/**
 * melo_playlist_get_tags:
 * @playlist: the playlist
 * @id: the media ID
 * @fields: the tag fields to get
 *
 * Get the #MeloTags for a specific media ID. Only the fields selected by
 * @fields are filled in the returned #MeloTags.
 *
 * Return: (transfer full): a #MeloTags containing all tags corresponding to the
 * media identified by @id. Use melo_tags_unref() after usage.
 */
MeloTags *
melo_playlist_get_tags (MeloPlaylist *playlist, const gchar *id,
                        MeloTagsFields fields)
{
  MeloPlaylistClass *pclass = MELO_PLAYLIST_GET_CLASS (playlist);

  g_return_val_if_fail (pclass->get_tags, NULL);

  return pclass->get_tags (playlist, id, fields);
}

/**
 * melo_playlist_add:
 * @playlist: the playlist
 * @path: the path of the media to provide to #MeloPlayer
 * @name: the display name of the media, can be %NULL
 * @tags: the #MeloTags attached to the media, can be %NULL
 * @is_current: if %TRUE, set the media as current playing
 *
 * Add a new media to the playllist. The media is appended to the playlist and
 * will be played in last.
 *
 * Returns: %TRUE if the media has been added to the list, %FALSE otherwise.
 */
gboolean
melo_playlist_add (MeloPlaylist *playlist, const gchar *path, const gchar *name,
                   MeloTags *tags, gboolean is_current)
{
  MeloPlaylistClass *pclass = MELO_PLAYLIST_GET_CLASS (playlist);

  g_return_val_if_fail (pclass->add, FALSE);

  return pclass->add (playlist, path, name, tags, is_current);
}

/**
 * melo_playlist_get_prev:
 * @playlist: the playlist
 * @id: a pointer to a media ID which is set with previous media in list to play
 * @tags: a pointer to the #MeloTags attached to the media
 * @set: if %TRUE, set the media has current playing media
 *
 * Get the ID of the previous media to play and its #MeloTags from the playlist.
 * If @set is %TRUE, the media is set has current playing (the play action is
 * not performed by this call: it should be only called from a #MeloPlayer).
 *
 * Returns: (transfer full): the display name of the previous media in list, in
 * addition to the set of @id and @tags, or %NULL if no media found. After use,
 * call g_free().
 */
gchar *
melo_playlist_get_prev (MeloPlaylist *playlist, gchar **id, MeloTags **tags,
                        gboolean set)
{
  MeloPlaylistClass *pclass = MELO_PLAYLIST_GET_CLASS (playlist);

  g_return_val_if_fail (pclass->get_prev, NULL);

  return pclass->get_prev (playlist, id, tags, set);
}

/**
 * melo_playlist_get_next:
 * @playlist: the playlist
 * @id: a pointer to a media ID which is set with next media in list to play
 * @tags: a pointer to the #MeloTags attached to the media
 * @set: if %TRUE, set the media has current playing media
 *
 * Get the ID of the next media to play and its #MeloTags from the playlist. If
 * @set is %TRUE, the media is set has current playing (the play action is not
 * performed by this call: it should be only called from a #MeloPlayer).
 *
 * Returns: (transfer full): the display name of the next media in list, in
 * addition to the set of @id and @tags, or %NULL if no media found. After use,
 * call g_free().
 */
gchar *
melo_playlist_get_next (MeloPlaylist *playlist, gchar **id, MeloTags **tags,
                        gboolean set)
{
  MeloPlaylistClass *pclass = MELO_PLAYLIST_GET_CLASS (playlist);

  g_return_val_if_fail (pclass->get_next, NULL);

  return pclass->get_next (playlist, id, tags, set);
}

/**
 * melo_playlist_has_prev:
 * @playlist: the playlist
 *
 * Check if there is a media to play previous the current playing media in the
 * playlist.
 *
 * Returns: %TRUE if a media is available for playing in playlist, before the
 * current media playing, %FALSE otherwise.
 */
gboolean
melo_playlist_has_prev (MeloPlaylist *playlist)
{
  MeloPlaylistClass *pclass = MELO_PLAYLIST_GET_CLASS (playlist);

  g_return_val_if_fail (pclass->has_prev, FALSE);

  return pclass->has_prev (playlist);
}

/**
 * melo_playlist_has_next:
 * @playlist: the playlist
 *
 * Check if there is a media to play after the current playing media in the
 * playlist.
 *
 * Returns: %TRUE if a media is available for playing in playlist, after the
 * current media playing, %FALSE otherwise.
 */
gboolean
melo_playlist_has_next (MeloPlaylist *playlist)
{
  MeloPlaylistClass *pclass = MELO_PLAYLIST_GET_CLASS (playlist);

  g_return_val_if_fail (pclass->has_next, FALSE);

  return pclass->has_next (playlist);
}

/**
 * melo_playlist_play:
 * @playlist: the playlist
 * @id: the media ID
 *
 * Play the media identified with @id with the associated #MeloPlayer.
 *
 * Returns: %TRUE if the media has been found and loaded into the associated
 * player, %FALSE otherwise.
 */
gboolean
melo_playlist_play (MeloPlaylist *playlist, const gchar *id)
{
  MeloPlaylistClass *pclass = MELO_PLAYLIST_GET_CLASS (playlist);

  g_return_val_if_fail (pclass->play, FALSE);

  return pclass->play (playlist, id);
}

/**
 * melo_playlist_sort:
 * @playlist: the playlist
 * @id: the first media ID in list to sort
 * @count: the number of media to sort, can be -1 for all medias
 * @sort: the #MeloSort filter to use
 *
 * Sort a media list of @count items starting with @id with @sort filter.
 *
 * Returns: %TRUE if the media list has been sorted, %FALSE otherwise.
 */
gboolean
melo_playlist_sort (MeloPlaylist *playlist, const gchar *id, guint count,
                    MeloSort sort)
{
  MeloPlaylistClass *pclass = MELO_PLAYLIST_GET_CLASS (playlist);

  g_return_val_if_fail (pclass->sort, FALSE);

  return pclass->sort (playlist, id, count, sort);
}

/**
 * melo_playlist_move:
 * @playlist: the playlist
 * @id: the first media ID in list to move
 * @up: the offset for moving the list from @id
 * @count: the number of media to move, can be -1 for all medias
 *
 * Move a media list of @count items starting with @id of @up places in list.
 * The @up value can be negative which means the list will be moved in the
 * other direction.
 *
 * For a list A, B, C, D, E (where A is the first media to be played in list)
 * with @id = B, @up = 2 and @count = 2, the resulting list will be:
 * A, D, E, B, C.
 *
 * Returns: %TRUE if the media list has been moved, %FALSE otherwise.
 */
gboolean
melo_playlist_move (MeloPlaylist *playlist, const gchar *id, gint up,
                    gint count)
{
  MeloPlaylistClass *pclass = MELO_PLAYLIST_GET_CLASS (playlist);

  g_return_val_if_fail (pclass->move, FALSE);

  return pclass->move (playlist, id, up, count);
}

/**
 * melo_playlist_move_to:
 * @playlist: the playlist
 * @id: the first media ID in list to move
 * @before: the media ID before which the list is moved
 * @count: the number of media to move, can be -1 for all medias
 *
 * Move a media list of @count items starting with @id, just before the media
 * identified with @before.
 *
 * For a list A, B, C, D, E (where A is the first media to be played in list)
 * with @id = B, @before = A and @count = 2, the resulting list will be:
 * B, C, A, D, E.
 *
 * Returns: %TRUE if the media list has been moved, %FALSE otherwise.
 */
gboolean
melo_playlist_move_to (MeloPlaylist *playlist, const gchar *id,
                       const gchar *before, gint count)
{
  MeloPlaylistClass *pclass = MELO_PLAYLIST_GET_CLASS (playlist);

  g_return_val_if_fail (pclass->move_to, FALSE);

  return pclass->move_to (playlist, id, before, count);
}

/**
 * melo_playlist_remove:
 * @playlist: the playlist
 * @id: the media ID
 *
 * Remove a media identified by @id from the playlist.
 *
 * Returns: %TRUE if the media has been removed, %FALSE otherwise.
 */
gboolean
melo_playlist_remove (MeloPlaylist *playlist, const gchar *id)
{
  MeloPlaylistClass *pclass = MELO_PLAYLIST_GET_CLASS (playlist);

  g_return_val_if_fail (pclass->remove, FALSE);

  return pclass->remove (playlist, id);
}

/**
 * melo_playlist_empty:
 * @playlist: the playlist
 *
 * Empty the playlist by removing all medias.
 */
void
melo_playlist_empty (MeloPlaylist *playlist)
{
  MeloPlaylistClass *pclass = MELO_PLAYLIST_GET_CLASS (playlist);

  if (pclass->empty)
    pclass->empty (playlist);
}

/**
 * melo_playlist_get_cover:
 * @playlist: the playlist
 * @id: the media ID
 * @cover: a pointer to a #GBytes which is set with image cover data
 * @type: a pointer to a string which is set with image mime type
 *
 * Get the image cover data and type of the media identified by @id.
 *
 * Returns: %TRUE if @cover and @type have been set with image data, %FALSE
 * otherwise.
 */
gboolean
melo_playlist_get_cover (MeloPlaylist *playlist, const gchar *id,
                         GBytes **cover, gchar **type)
{
  MeloPlaylistClass *pclass = MELO_PLAYLIST_GET_CLASS (playlist);

  g_return_val_if_fail (pclass->get_cover, FALSE);

  return pclass->get_cover (playlist, id, cover, type);
}

/**
 * melo_playlist_list_new:
 *
 * Create a new #MeloPlaylistList which will contain the content detail and list
 * for the current playlist.
 *
 * Returns: (transfer full): a new #MeloPlaylistList or %NULL if an error
 * occurred. After usage, it should be freed with melo_playlist_list_free().
 */
MeloPlaylistList *
melo_playlist_list_new ()
{
  return g_slice_new0 (MeloPlaylistList);
}

/**
 * melo_playlist_list_free:
 * @list: the list to free
 *
 * Free the #MeloPlaylistList instance.
 */
void
melo_playlist_list_free (MeloPlaylistList *list)
{
  g_free (list->current);
  g_list_free_full (list->items, (GDestroyNotify) melo_playlist_item_unref);
  g_slice_free (MeloPlaylistList, list);
}

/**
 * melo_playlist_item_new:
 * @id: the ID of the media. Can be set to %NULL and set later with g_strdup()
 * @name: the media display name
 * @path: the media path to use with #MeloPlayer
 * @tags: the #MeloTags to attach to the media
 *
 * Create a new #MeloPlaylistItem which will contain detail for one media of a
 * #MeloPlaylistList.
 *
 * Returns: (transfer full): a new #MeloPlaylistItem or %NULL if an error
 * occurred. After usage, call melo_playlist_item_unref().
 */
MeloPlaylistItem *
melo_playlist_item_new (const gchar *id, const gchar *name, const gchar *path,
                        MeloTags *tags)
{
  MeloPlaylistItem *item;

  /* Allocate new item */
  item = g_slice_new0 (MeloPlaylistItem);
  if (!item)
    return NULL;

  /* Set name and type */
  item->id = g_strdup (id);
  item->name = g_strdup (name);
  item->path = g_strdup (path);
  if (tags)
    item->tags = melo_tags_ref (tags);
  item->ref_count = 1;

  return item;
}

/**
 * melo_playlist_item_ref:
 * @item: the playlist item
 *
 * Increment the reference counter of the #MeloPlaylistItem.
 *
 * Returns: (transfer full): a pointer of the #MeloPlaylistItem.
 */
MeloPlaylistItem *
melo_playlist_item_ref (MeloPlaylistItem *item)
{
  item->ref_count++;
  return item;
}

/**
 * melo_playlist_item_unref:
 * @item: the playlist item
 *
 * Decrement the reference counter of the #MeloPlaylistItem. If it reaches zero,
 * the #MeloPlaylistItem is freed.
 */
void
melo_playlist_item_unref (MeloPlaylistItem *item)
{
  if (--item->ref_count)
    return;

  g_free (item->id);
  g_free (item->name);
  g_free (item->path);
  if (item->tags)
    melo_tags_unref (item->tags);
  g_slice_free (MeloPlaylistItem, item);
}

#define DELCARE_PLAYLIST_ITEM_CMP_FUNC(type,field) \
static gint \
melo_playlist_item_cmp_##type (gconstpointer a, gconstpointer b) \
{ \
  const MeloPlaylistItem *i1 = a, *i2 = b; \
  return melo_sort_cmp_##type (i2->field, i1->field); \
} \
static gint \
melo_playlist_item_cmp_##type##_desc (gconstpointer a, gconstpointer b) \
{ \
  const MeloPlaylistItem *i1 = a, *i2 = b; \
  return melo_sort_cmp_##type (i1->field, i2->field); \
}

DELCARE_PLAYLIST_ITEM_CMP_FUNC (file, id)
DELCARE_PLAYLIST_ITEM_CMP_FUNC (title, tags->title)
DELCARE_PLAYLIST_ITEM_CMP_FUNC (artist, tags->artist)
DELCARE_PLAYLIST_ITEM_CMP_FUNC (album, tags->album)
DELCARE_PLAYLIST_ITEM_CMP_FUNC (genre, tags->genre)
DELCARE_PLAYLIST_ITEM_CMP_FUNC (date, tags->date)
DELCARE_PLAYLIST_ITEM_CMP_FUNC (track, tags->track)
DELCARE_PLAYLIST_ITEM_CMP_FUNC (tracks, tags->tracks)

/**
 * melo_playlist_item_list_sort:
 * @list: a #GList of #MeloPlaylistItem to sort
 * @sort: the #MeloSort filter to use
 *
 * Sort a #GList of #MeloPlaylistItem with a certain #MeloSort filter.
 *
 * Returns: (transfer none): the same #GList sorted.
 */
GList *
melo_playlist_item_list_sort (GList *list, MeloSort sort)
{
  GCompareFunc func;

  switch (melo_sort_set_asc (sort)) {
    case MELO_SORT_SHUFFLE:
      func = melo_sort_cmp_shuffle;
      break;
    case MELO_SORT_FILE:
      func = melo_sort_is_desc (sort) ? melo_playlist_item_cmp_file_desc :
                                        melo_playlist_item_cmp_file;
      break;
    case MELO_SORT_TITLE:
      func = melo_sort_is_desc (sort) ? melo_playlist_item_cmp_title_desc :
                                        melo_playlist_item_cmp_title;
      break;
    case MELO_SORT_ARTIST:
      func = melo_sort_is_desc (sort) ? melo_playlist_item_cmp_artist_desc :
                                        melo_playlist_item_cmp_artist;
      break;
    case MELO_SORT_ALBUM:
      func = melo_sort_is_desc (sort) ? melo_playlist_item_cmp_album_desc :
                                        melo_playlist_item_cmp_album;
      break;
    case MELO_SORT_GENRE:
      func = melo_sort_is_desc (sort) ? melo_playlist_item_cmp_genre_desc :
                                        melo_playlist_item_cmp_genre;
      break;
    case MELO_SORT_DATE:
      func = melo_sort_is_desc (sort) ? melo_playlist_item_cmp_date_desc :
                                        melo_playlist_item_cmp_date;
      break;
    case MELO_SORT_TRACK:
      func = melo_sort_is_desc (sort) ? melo_playlist_item_cmp_track_desc :
                                        melo_playlist_item_cmp_track;
      break;
    case MELO_SORT_TRACKS:
      func = melo_sort_is_desc (sort) ? melo_playlist_item_cmp_tracks_desc :
                                        melo_playlist_item_cmp_tracks;
      break;
    case MELO_SORT_NONE:
    default:
      return list;
  }

  return g_list_sort (list, func);
}

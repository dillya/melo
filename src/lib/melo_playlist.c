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

  /* Install ID property */
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

void
melo_playlist_set_player (MeloPlaylist *playlist, MeloPlayer *player)
{
  playlist->player = player;
}

MeloPlayer *
melo_playlist_get_player (MeloPlaylist *playlist)
{
  g_return_val_if_fail (playlist->player, NULL);

  return g_object_ref (playlist->player);
}

MeloPlaylistList *
melo_playlist_get_list (MeloPlaylist *playlist, MeloTagsFields tags_fields)
{
  MeloPlaylistClass *pclass = MELO_PLAYLIST_GET_CLASS (playlist);

  g_return_val_if_fail (pclass->get_list, NULL);

  return pclass->get_list (playlist, tags_fields);
}

MeloTags *
melo_playlist_get_tags (MeloPlaylist *playlist, const gchar *name,
                        MeloTagsFields fields)
{
  MeloPlaylistClass *pclass = MELO_PLAYLIST_GET_CLASS (playlist);

  g_return_val_if_fail (pclass->get_tags, NULL);

  return pclass->get_tags (playlist, name, fields);
}

gboolean
melo_playlist_add (MeloPlaylist *playlist, const gchar *path, const gchar *name,
                   MeloTags *tags, gboolean is_current)
{
  MeloPlaylistClass *pclass = MELO_PLAYLIST_GET_CLASS (playlist);

  g_return_val_if_fail (pclass->add, FALSE);

  return pclass->add (playlist, path, name, tags, is_current);
}

gchar *
melo_playlist_get_prev (MeloPlaylist *playlist, gchar **name, MeloTags **tags,
                        gboolean set)
{
  MeloPlaylistClass *pclass = MELO_PLAYLIST_GET_CLASS (playlist);

  g_return_val_if_fail (pclass->get_prev, NULL);

  return pclass->get_prev (playlist, name, tags, set);
}

gchar *
melo_playlist_get_next (MeloPlaylist *playlist, gchar **name, MeloTags **tags,
                        gboolean set)
{
  MeloPlaylistClass *pclass = MELO_PLAYLIST_GET_CLASS (playlist);

  g_return_val_if_fail (pclass->get_next, NULL);

  return pclass->get_next (playlist, name, tags, set);
}

gboolean
melo_playlist_has_prev (MeloPlaylist *playlist)
{
  MeloPlaylistClass *pclass = MELO_PLAYLIST_GET_CLASS (playlist);

  g_return_val_if_fail (pclass->has_prev, FALSE);

  return pclass->has_prev (playlist);
}

gboolean
melo_playlist_has_next (MeloPlaylist *playlist)
{
  MeloPlaylistClass *pclass = MELO_PLAYLIST_GET_CLASS (playlist);

  g_return_val_if_fail (pclass->has_next, FALSE);

  return pclass->has_next (playlist);
}

gboolean
melo_playlist_play (MeloPlaylist *playlist, const gchar *path)
{
  MeloPlaylistClass *pclass = MELO_PLAYLIST_GET_CLASS (playlist);

  g_return_val_if_fail (pclass->play, FALSE);

  return pclass->play (playlist, path);
}

gboolean
melo_playlist_move (MeloPlaylist *playlist, const gchar *name, gint up,
                    gint count)
{
  MeloPlaylistClass *pclass = MELO_PLAYLIST_GET_CLASS (playlist);

  g_return_val_if_fail (pclass->move, FALSE);

  return pclass->move (playlist, name, up, count);
}

gboolean
melo_playlist_move_to (MeloPlaylist *playlist, const gchar *name,
                       const gchar *before, gint count)
{
  MeloPlaylistClass *pclass = MELO_PLAYLIST_GET_CLASS (playlist);

  g_return_val_if_fail (pclass->move_to, FALSE);

  return pclass->move_to (playlist, name, before, count);
}

gboolean
melo_playlist_remove (MeloPlaylist *playlist, const gchar *path)
{
  MeloPlaylistClass *pclass = MELO_PLAYLIST_GET_CLASS (playlist);

  g_return_val_if_fail (pclass->remove, FALSE);

  return pclass->remove (playlist, path);
}

void
melo_playlist_empty (MeloPlaylist *playlist)
{
  MeloPlaylistClass *pclass = MELO_PLAYLIST_GET_CLASS (playlist);

  if (pclass->empty)
    pclass->empty (playlist);
}

gboolean
melo_playlist_get_cover (MeloPlaylist *playlist, const gchar *path,
                         GBytes **cover, gchar **type)
{
  MeloPlaylistClass *pclass = MELO_PLAYLIST_GET_CLASS (playlist);

  g_return_val_if_fail (pclass->get_cover, FALSE);

  return pclass->get_cover (playlist, path, cover, type);
}

MeloPlaylistList *
melo_playlist_list_new ()
{
  return g_slice_new0 (MeloPlaylistList);
}

void
melo_playlist_list_free (MeloPlaylistList *list)
{
  g_free (list->current);
  g_list_free_full (list->items, (GDestroyNotify) melo_playlist_item_unref);
  g_slice_free (MeloPlaylistList, list);
}

MeloPlaylistItem *
melo_playlist_item_new (const gchar *name, const gchar *full_name,
                        const gchar *path, MeloTags *tags)
{
  MeloPlaylistItem *item;

  /* Allocate new item */
  item = g_slice_new0 (MeloPlaylistItem);
  if (!item)
    return NULL;

  /* Set name and type */
  item->name = g_strdup (name);
  item->full_name = g_strdup (full_name);
  item->path = g_strdup (path);
  if (tags)
    item->tags = melo_tags_ref (tags);
  item->ref_count = 1;

  return item;
}

MeloPlaylistItem *
melo_playlist_item_ref (MeloPlaylistItem *item)
{
  item->ref_count++;
  return item;
}

void
melo_playlist_item_unref (MeloPlaylistItem *item)
{
  if (--item->ref_count)
    return;

  g_free (item->name);
  g_free (item->full_name);
  g_free (item->path);
  if (item->tags)
    melo_tags_unref (item->tags);
  g_slice_free (MeloPlaylistItem, item);
}

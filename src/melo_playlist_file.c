/*
 * melo_playlist_file.c: Simple File Playlist
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

#include <string.h>

#include "melo_player.h"
#include "melo_playlist_file.h"

#define MELO_PLAYLIST_FILE_NAME_EXT_SIZE 10

static GList *melo_playlist_file_get_list (MeloPlaylist *playlist,
                                           gchar **current);
static gboolean melo_playlist_file_add (MeloPlaylist *playlist,
                                        const gchar *name,
                                        const gchar *full_name,
                                        const gchar *path,
                                        gboolean is_current);
static gboolean melo_playlist_file_play (MeloPlaylist *playlist,
                                         const gchar *name);
static gboolean melo_playlist_file_remove (MeloPlaylist *playlist,
                                           const gchar *name);

struct _MeloPlaylistFilePrivate {
  GMutex mutex;
  GList *playlist;
  GHashTable *names;
  MeloPlaylistItem *current;
};

G_DEFINE_TYPE_WITH_PRIVATE (MeloPlaylistFile, melo_playlist_file, MELO_TYPE_PLAYLIST)

static void
melo_playlist_file_finalize (GObject *gobject)
{
  MeloPlaylistFile *playlist_file = MELO_PLAYLIST_FILE (gobject);
  MeloPlaylistFilePrivate *priv =
                          melo_playlist_file_get_instance_private (playlist_file);

  /* Clear mutex */
  g_mutex_clear (&priv->mutex);

  /* Free hash table */
  g_hash_table_remove_all (priv->names);
  g_hash_table_unref (priv->names);

  /* Free playlist */
  g_list_free_full (priv->playlist, (GDestroyNotify) melo_playlist_item_unref);

  /* Chain up to the parent class */
  G_OBJECT_CLASS (melo_playlist_file_parent_class)->finalize (gobject);
}

static void
melo_playlist_file_class_init (MeloPlaylistFileClass *klass)
{
  MeloPlaylistClass *bclass = MELO_PLAYLIST_CLASS (klass);
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  bclass->get_list = melo_playlist_file_get_list;
  bclass->add = melo_playlist_file_add;
  bclass->play = melo_playlist_file_play;
  bclass->remove = melo_playlist_file_remove;

  /* Add custom finalize() function */
  oclass->finalize = melo_playlist_file_finalize;
}

static void
melo_playlist_file_init (MeloPlaylistFile *self)
{
  MeloPlaylistFilePrivate *priv = melo_playlist_file_get_instance_private (self);

  self->priv = priv;
  priv->playlist = NULL;
  priv->current = NULL;

  /* Init mutex */
  g_mutex_init (&priv->mutex);

  /* Init Hash table for names */
  priv->names = g_hash_table_new (g_str_hash, g_str_equal);
}

static GList *
melo_playlist_file_get_list (MeloPlaylist *playlist, gchar **current)
{
  MeloPlaylistFile *plfile = MELO_PLAYLIST_FILE (playlist);
  MeloPlaylistFilePrivate *priv = plfile->priv;
  GList *list;

  /* Lock playlist */
  g_mutex_lock (&priv->mutex);

  /* Copy playlist */
  list = g_list_copy_deep (priv->playlist, (GCopyFunc) melo_playlist_item_ref,
                           NULL);
  if (priv->current)
    *current = g_strdup (priv->current->name);

  /* Unlock playlist */
  g_mutex_unlock (&priv->mutex);

  return list;
}

static gboolean
melo_playlist_file_add (MeloPlaylist *playlist, const gchar *name,
                        const gchar *full_name, const gchar *path,
                        gboolean is_current)
{
  MeloPlaylistFile *plfile = MELO_PLAYLIST_FILE (playlist);
  MeloPlaylistFilePrivate *priv = plfile->priv;
  MeloPlaylistItem *item;
  gint len, i;
  gchar *final_name;

  /* Lock playlist */
  g_mutex_lock (&priv->mutex);

  /* Generate a new name if current doesn't exists */
  len = strlen (name);
  final_name = g_strndup (name, len + MELO_PLAYLIST_FILE_NAME_EXT_SIZE);
  for (i = 1; i > 0 && g_hash_table_lookup (priv->names, final_name); i++)
    g_snprintf (final_name + len, MELO_PLAYLIST_FILE_NAME_EXT_SIZE, "_%d", i);
  if (i < 0) {
    g_mutex_unlock (&priv->mutex);
    return FALSE;
  }

  /* Add a new file to playlist */
  item = melo_playlist_item_new (NULL, full_name, path);
  item->name = final_name;
  priv->playlist = g_list_append (priv->playlist, item);
  g_hash_table_insert (priv->names, final_name, item);

  /* Set as current */
  if (is_current)
    priv->current = item;

  /* Unlock playlist */
  g_mutex_unlock (&priv->mutex);

  return TRUE;
}

static gboolean
melo_playlist_file_play (MeloPlaylist *playlist, const gchar *name)
{
  MeloPlaylistFile *plfile = MELO_PLAYLIST_FILE (playlist);
  MeloPlaylistFilePrivate *priv = plfile->priv;
  MeloPlaylistItem *item;

  /* Lock playlist */
  g_mutex_lock (&priv->mutex);

  /* Find media in hash table */
  item = g_hash_table_lookup (priv->names, name);
  if (item) {
    melo_playlist_item_ref (item);
    priv->current = item;
  }

  /* Unlock playlist */
  g_mutex_unlock (&priv->mutex);

  /* No item found */
  if (!item)
    return FALSE;

  /* Play media if player is available */
  if (playlist->player)
    melo_player_play (playlist->player, item->path);
  melo_playlist_item_unref (item);

  return TRUE;
}

static gboolean
melo_playlist_file_remove (MeloPlaylist *playlist, const gchar *name)
{
  MeloPlaylistFile *plfile = MELO_PLAYLIST_FILE (playlist);
  MeloPlaylistFilePrivate *priv = plfile->priv;
  MeloPlaylistItem *item;

  /* Lock playlist */
  g_mutex_lock (&priv->mutex);

  /* Find media in hash table */
  item = g_hash_table_lookup (priv->names, name);
  if (!item) {
    g_mutex_unlock (&priv->mutex);
    return FALSE;
  }

  /* Remove from list and hash table */
  priv->playlist = g_list_remove (priv->playlist, item);
  g_hash_table_remove (priv->names, name);
  melo_playlist_item_unref (item);

  /* Unlock playlist */
  g_mutex_unlock (&priv->mutex);

  return TRUE;
}

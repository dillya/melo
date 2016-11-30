/*
 * melo_playlist_radio.c: Simple Radio Playlist
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
#include "melo_playlist_radio.h"

static MeloPlaylistList *melo_playlist_radio_get_list (MeloPlaylist *playlist,
                                                    MeloTagsFields tags_fields);
static gboolean melo_playlist_radio_add (MeloPlaylist *playlist,
                                         const gchar *path, const gchar *name,
                                         MeloTags *tags, gboolean is_current);
static void melo_playlist_radio_empty (MeloPlaylist *playlist);

struct _MeloPlaylistRadioPrivate {
  GMutex mutex;
  GList *playlist;
};

G_DEFINE_TYPE_WITH_PRIVATE (MeloPlaylistRadio, melo_playlist_radio, MELO_TYPE_PLAYLIST)

static void
melo_playlist_radio_finalize (GObject *gobject)
{
  MeloPlaylistRadio *playlist_radio = MELO_PLAYLIST_RADIO (gobject);
  MeloPlaylistRadioPrivate *priv =
                      melo_playlist_radio_get_instance_private (playlist_radio);

  /* Clear mutex */
  g_mutex_clear (&priv->mutex);

  /* Free playlist */
  g_list_free_full (priv->playlist, (GDestroyNotify) melo_playlist_item_unref);

  /* Chain up to the parent class */
  G_OBJECT_CLASS (melo_playlist_radio_parent_class)->finalize (gobject);
}

static void
melo_playlist_radio_class_init (MeloPlaylistRadioClass *klass)
{
  MeloPlaylistClass *plclass = MELO_PLAYLIST_CLASS (klass);
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  plclass->get_list = melo_playlist_radio_get_list;
  plclass->add = melo_playlist_radio_add;
  plclass->empty = melo_playlist_radio_empty;

  /* Add custom finalize() function */
  oclass->finalize = melo_playlist_radio_finalize;
}

static void
melo_playlist_radio_init (MeloPlaylistRadio *self)
{
  MeloPlaylistRadioPrivate *priv =
                                melo_playlist_radio_get_instance_private (self);

  self->priv = priv;
  priv->playlist = NULL;

  /* Init mutex */
  g_mutex_init (&priv->mutex);
}

static MeloPlaylistList *
melo_playlist_radio_get_list (MeloPlaylist *playlist,
                              MeloTagsFields tags_fields)
{
  MeloPlaylistRadio *plradio = MELO_PLAYLIST_RADIO (playlist);
  MeloPlaylistRadioPrivate *priv = plradio->priv;
  MeloPlaylistList *list;

  /* Create new list */
  list = melo_playlist_list_new ();
  if (!list)
    return NULL;

  /* Lock playlist */
  g_mutex_lock (&priv->mutex);

  /* Copy playlist */
  list->items = g_list_copy_deep (priv->playlist,
                                  (GCopyFunc) melo_playlist_item_ref, NULL);

  /* Unlock playlist */
  g_mutex_unlock (&priv->mutex);

  /* Current is last entry in playlist */
  if (list->items)
    list->current = g_strdup (((MeloPlaylistItem *) list->items->data)->name);

  return list;
}

static gboolean
melo_playlist_radio_add (MeloPlaylist *playlist, const gchar *path,
                         const gchar *name, MeloTags *tags, gboolean is_current)
{
  MeloPlaylistRadio *plradio = MELO_PLAYLIST_RADIO (playlist);
  MeloPlaylistRadioPrivate *priv = plradio->priv;
  MeloPlaylistItem *item;

  /* Do not accept empty title */
  if (strlen (name) < 4 && strchr (name, '-'))
    return FALSE;

  /* Lock playlist */
  g_mutex_lock (&priv->mutex);

  /* This item is already present */
  if (priv->playlist) {
    item = (MeloPlaylistItem *) priv->playlist->data;
    if (!g_strcmp0 (name, item->name)) {
      g_mutex_unlock (&priv->mutex);
      return FALSE;
    }
  }

  /* Add a new song to playlist */
  item = melo_playlist_item_new (name, name, path, tags);
  item->can_play = FALSE;
  item->can_remove = FALSE;
  priv->playlist = g_list_prepend (priv->playlist, item);

  /* Unlock playlist */
  g_mutex_unlock (&priv->mutex);

  return TRUE;
}

static void
melo_playlist_radio_empty (MeloPlaylist *playlist)
{
  MeloPlaylistRadio *plradio = MELO_PLAYLIST_RADIO (playlist);
  MeloPlaylistRadioPrivate *priv = plradio->priv;

  /* Lock playlist */
  g_mutex_lock (&priv->mutex);

  /* Remove and free all items */
  g_list_free_full (priv->playlist, (GDestroyNotify) melo_playlist_item_unref);
  priv->playlist = NULL;

  /* Unlock playlist */
  g_mutex_unlock (&priv->mutex);
}

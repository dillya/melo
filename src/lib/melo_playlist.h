/*
 * melo_playlist.h: Playlist base class
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

#ifndef __MELO_PLAYLIST_H__
#define __MELO_PLAYLIST_H__

#include <glib-object.h>

#include "melo_tags.h"
#include "melo_sort.h"

G_BEGIN_DECLS

#define MELO_TYPE_PLAYLIST             (melo_playlist_get_type ())
#define MELO_PLAYLIST(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), MELO_TYPE_PLAYLIST, MeloPlaylist))
#define MELO_IS_PLAYLIST(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MELO_TYPE_PLAYLIST))
#define MELO_PLAYLIST_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), MELO_TYPE_PLAYLIST, MeloPlaylistClass))
#define MELO_IS_PLAYLIST_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), MELO_TYPE_PLAYLIST))
#define MELO_PLAYLIST_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), MELO_TYPE_PLAYLIST, MeloPlaylistClass))

#ifndef __GTK_DOC_IGNORE__
typedef struct _MeloPlayer MeloPlayer;
#endif
typedef struct _MeloPlaylist MeloPlaylist;
typedef struct _MeloPlaylistClass MeloPlaylistClass;
typedef struct _MeloPlaylistPrivate MeloPlaylistPrivate;

typedef struct _MeloPlaylistList MeloPlaylistList;
typedef struct _MeloPlaylistItem MeloPlaylistItem;

/**
 * MeloPlaylist:
 *
 * The opaque #MeloPlaylist data structure.
 */
struct _MeloPlaylist {
  GObject parent_instance;

  /*< protected */
  MeloPlayer *player;

  /*< private >*/
  MeloPlaylistPrivate *priv;
};

/**
 * MeloPlaylistClass:
 * @parent_class: Object parent class
 * @get_list: Provide the list of media in the playlist
 * @get_tags: Provide the #MeloTags of one media in the playlist
 * @add: Add a new media to the playlist
 * @get_prev: Get the media in the playlist before the current playing
 * @get_next: Get the media in the playlist to play after the current playing
 * @has_prev: Check if a media can be played in playlist before the current
 *    playing
 * @has_next: Check if a media can be played in playlist after the current
 *    playing
 * @play: Play a media in playlist with the associated #MeloPlayer
 * @sort: Sort one or more media(s) in the playlist
 * @move: Move one or more media(s) in the playlist
 * @move_to: Move one or more media(s) in the playlist before one item
 * @remove: Remove one media from the playlist
 * @empty: Empty the whole media
 * @get_cover: Provide the image cover data of a media in the playlist
 *
 * Subclasses must override at least the @ virtual method to implement a
 * read-only playlist (associated #MeloPlayer only add media, like webradio
 * player).
 */
struct _MeloPlaylistClass {
  GObjectClass parent_class;

  MeloPlaylistList *(*get_list) (MeloPlaylist *playlist,
                                 MeloTagsFields tags_fields);
  MeloTags *(*get_tags) (MeloPlaylist *playlist, const gchar *id,
                         MeloTagsFields fields);
  gboolean (*add) (MeloPlaylist *playlist, const gchar *path, const gchar *name,
                   MeloTags *tags, gboolean is_current);
  gchar *(*get_prev) (MeloPlaylist *playlist, gchar **id, MeloTags **tags,
                      gboolean set);
  gchar *(*get_next) (MeloPlaylist *playlist, gchar **id, MeloTags **tags,
                      gboolean set);
  gboolean (*has_prev) (MeloPlaylist *playlist);
  gboolean (*has_next) (MeloPlaylist *playlist);
  gboolean (*play) (MeloPlaylist *playlist, const gchar *id);
  gboolean (*sort) (MeloPlaylist *playlist, const gchar *id, guint count,
                    MeloSort sort);
  gboolean (*move) (MeloPlaylist *playlist, const gchar *id, gint up,
                    gint count);
  gboolean (*move_to) (MeloPlaylist *playlist, const gchar *id,
                       const gchar *before, gint count);
  gboolean (*remove) (MeloPlaylist *playlist, const gchar *id);
  void (*empty) (MeloPlaylist *playlist);

  gboolean (*get_cover) (MeloPlaylist *playlist, const gchar *id,
                         GBytes **cover, gchar **type);
};

/**
 * MeloPlaylistList:
 * @current: the media ID of the current playing media
 * @items: a #GList of #MeloPlaylistItem
 *
 * A #MeloPlaylistList contains the current media list of a #MeloPlaylist
 * presented with a #GList of #MeloPlaylistItem and the current media playing
 * in associated #MeloPlayer, identified with @current.
 */
struct _MeloPlaylistList {
  gchar *current;
  GList *items;
};

/**
 * MeloPlaylistItem:
 * @id: the ID of the media, used to identify this specific media
 * @name: the display name for the media, can be %NULL
 * @path: the path to use with the associated #MeloPlayer in order to play the
 *    media, can be %NULL
 * @tags: the #MeloTags of the media, can be %NULL
 * @can_play: set to %TRUE if the media can be played
 * @can_remove: set to %TRUE if the media can be removed from the playlist
 *
 * A #MeloPlaylistItem handles all details about one media in the playlist.
 */
struct _MeloPlaylistItem {
  gchar *id;
  gchar *name;
  gchar *path;
  MeloTags *tags;
  gboolean can_play;
  gboolean can_remove;

  /*< private >*/
  gint ref_count;
};

GType melo_playlist_get_type (void);

MeloPlaylist *melo_playlist_new (GType type, const gchar *id);
const gchar *melo_playlist_get_id (MeloPlaylist *playlist);
MeloPlaylist *melo_playlist_get_playlist_by_id (const gchar *id);

void melo_playlist_set_player (MeloPlaylist *playlist, MeloPlayer *player);
MeloPlayer *melo_playlist_get_player (MeloPlaylist *playlist);

MeloPlaylistList *melo_playlist_get_list (MeloPlaylist *playlist,
                                          MeloTagsFields tags_fields);
MeloTags *melo_playlist_get_tags (MeloPlaylist *playlist, const gchar *id,
                                  MeloTagsFields fields);
gboolean melo_playlist_add (MeloPlaylist *playlist, const gchar *path,
                            const gchar *name, MeloTags *tags,
                            gboolean is_current);
gchar *melo_playlist_get_prev (MeloPlaylist *playlist, gchar **id,
                               MeloTags **tags, gboolean set);
gchar *melo_playlist_get_next (MeloPlaylist *playlist, gchar **id,
                               MeloTags **tags, gboolean set);
gboolean melo_playlist_has_prev (MeloPlaylist *playlist);
gboolean melo_playlist_has_next (MeloPlaylist *playlist);
gboolean melo_playlist_play (MeloPlaylist *playlist, const gchar *id);
gboolean melo_playlist_sort (MeloPlaylist *playlist, const gchar *id,
                             guint count, MeloSort sort);
gboolean melo_playlist_move (MeloPlaylist *playlist, const gchar *id, gint up,
                             gint count);
gboolean melo_playlist_move_to (MeloPlaylist *playlist, const gchar *id,
                                const gchar *before, gint count);
gboolean melo_playlist_remove (MeloPlaylist *playlist, const gchar *id);
void melo_playlist_empty (MeloPlaylist *playlist);

gboolean melo_playlist_get_cover (MeloPlaylist *playlist, const gchar *id,
                                  GBytes **cover, gchar **type);

MeloPlaylistList *melo_playlist_list_new (void);
void melo_playlist_list_free (MeloPlaylistList *list);

MeloPlaylistItem *melo_playlist_item_new (const gchar *id,
                                          const gchar *name,
                                          const gchar *path, MeloTags *tags);
MeloPlaylistItem *melo_playlist_item_ref (MeloPlaylistItem *item);
void melo_playlist_item_unref (MeloPlaylistItem *item);

GList *melo_playlist_item_list_sort (GList *list, MeloSort sort);

G_END_DECLS

#endif /* __MELO_PLAYLIST_H__ */

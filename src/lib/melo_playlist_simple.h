/*
 * melo_playlist_simple.h: Simple Playlist
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

#ifndef __MELO_PLAYLIST_SIMPLE_H__
#define __MELO_PLAYLIST_SIMPLE_H__

#include "melo_playlist.h"

G_BEGIN_DECLS

#define MELO_TYPE_PLAYLIST_SIMPLE             (melo_playlist_simple_get_type ())
#define MELO_PLAYLIST_SIMPLE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), MELO_TYPE_PLAYLIST_SIMPLE, MeloPlaylistSimple))
#define MELO_IS_PLAYLIST_SIMPLE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MELO_TYPE_PLAYLIST_SIMPLE))
#define MELO_PLAYLIST_SIMPLE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), MELO_TYPE_PLAYLIST_SIMPLE, MeloPlaylistSimpleClass))
#define MELO_IS_PLAYLIST_SIMPLE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), MELO_TYPE_PLAYLIST_SIMPLE))
#define MELO_PLAYLIST_SIMPLE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), MELO_TYPE_PLAYLIST_SIMPLE, MeloPlaylistSimpleClass))

typedef struct _MeloPlaylistSimple MeloPlaylistSimple;
typedef struct _MeloPlaylistSimpleClass MeloPlaylistSimpleClass;
typedef struct _MeloPlaylistSimplePrivate MeloPlaylistSimplePrivate;

/**
 * MeloPlaylistSimple:
 *
 * The opaque #MeloPlaylistSimple data structure.
 */
struct _MeloPlaylistSimple {
  MeloPlaylist parent_instance;

  /*< private >*/
  MeloPlaylistSimplePrivate *priv;
};

/**
 * MeloPlaylistSimpleClass:
 * @parent_class: MeloPlaylist parent class
 *
 * The opaque #MeloPlaylistSimpleClass data structure.
 */
struct _MeloPlaylistSimpleClass {
  MeloPlaylistClass parent_class;
};

GType melo_playlist_simple_get_type (void);

G_END_DECLS

#endif /* __MELO_PLAYLIST_SIMPLE_H__ */

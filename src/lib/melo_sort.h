/*
 * melo_sort.h: Media sort enums and helpers
 *
 * Copyright (C) 2017 Alexandre Dilly <dillya@sparod.com>
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

#ifndef __MELO_SORT_H__
#define __MELO_SORT_H__

#include <glib.h>

typedef enum {
  MELO_SORT_NONE = 0,

  /* Shuffle medias */
  MELO_SORT_SHUFFLE,

  /* Sort by file name */
  MELO_SORT_FILE,

  /* Sort by tags */
  MELO_SORT_TITLE,
  MELO_SORT_ARTIST,
  MELO_SORT_ALBUM,
  MELO_SORT_GENRE,
  MELO_SORT_DATE,
  MELO_SORT_TRACK,
  MELO_SORT_TRACKS,

  /* Sort by usage */
  MELO_SORT_RELEVANT,
  MELO_SORT_RATING,
  MELO_SORT_PLAY_COUNT,

  /* Sort count */
  MELO_SORT_COUNT,
} MeloSort;

#define MELO_SORT_DESC 0x1000
#define MELO_SORT_MASK (MELO_SORT_DESC - 1)

static inline gboolean
melo_sort_is_valid (MeloSort sort)
{
  return (sort & MELO_SORT_MASK) < MELO_SORT_COUNT;
}

static inline MeloSort
melo_sort_set_asc (MeloSort sort)
{
  return (sort & ~MELO_SORT_DESC);
}

static inline MeloSort
melo_sort_set_desc (MeloSort sort)
{
  return (sort | MELO_SORT_DESC);
}

static inline MeloSort
melo_sort_invert (MeloSort sort)
{
  return (sort ^ MELO_SORT_DESC);
}

static inline gboolean
melo_sort_is_asc (MeloSort sort)
{
  return !(sort & MELO_SORT_DESC);
}

static inline gboolean
melo_sort_is_desc (MeloSort sort)
{
  return (sort & MELO_SORT_DESC);
}

static inline MeloSort
melo_sort_replace (MeloSort sort, MeloSort new_sort)
{
  return (sort & ~MELO_SORT_MASK) | new_sort;
}

const gchar *melo_sort_to_string (MeloSort sort);
MeloSort melo_sort_from_string (const gchar *name);

gint melo_sort_cmp_none (gconstpointer a, gconstpointer b);
gint melo_sort_cmp_shuffle (gconstpointer a, gconstpointer b);

#define melo_sort_cmp_file(a,b) (g_strcmp0 (a, b))
#define melo_sort_cmp_file_desc(a,b) (-g_strcmp0 (a, b))

#define melo_sort_cmp_title(a,b) (g_strcmp0 (a, b))
#define melo_sort_cmp_title_desc(a,b) (-g_strcmp0 (a, b))

#define melo_sort_cmp_artist(a,b) (g_strcmp0 (a, b))
#define melo_sort_cmp_artist_desc(a,b) (-g_strcmp0 (a, b))

#define melo_sort_cmp_album(a,b) (g_strcmp0 (a, b))
#define melo_sort_cmp_album_desc(a,b) (-g_strcmp0 (a, b))

#define melo_sort_cmp_genre(a,b) (g_strcmp0 (a, b))
#define melo_sort_cmp_genre_desc(a,b) (-g_strcmp0 (a, b))

#define melo_sort_cmp_date(a,b) (a > b)
#define melo_sort_cmp_date_desc(a,b) (a < b)

#define melo_sort_cmp_track(a,b) (a > b)
#define melo_sort_cmp_track_desc(a,b) (a < b)

#define melo_sort_cmp_tracks(a,b) (a > b)
#define melo_sort_cmp_tracks_desc(a,b) (a < b)

#endif /* __MELO_SORT_H__ */

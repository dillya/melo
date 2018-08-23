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

/**
 * MeloSort:
 * @MELO_SORT_NONE: do not sort medias
 * @MELO_SORT_SHUFFLE: sort medias randomly
 * @MELO_SORT_FILE: sort medias by file name
 * @MELO_SORT_TITLE: sort medias by title / name
 * @MELO_SORT_ARTIST: sort medias by artist name
 * @MELO_SORT_ALBUM: sort medias by album name
 * @MELO_SORT_GENRE: sort medias by genre
 * @MELO_SORT_DATE: sort medias by date
 * @MELO_SORT_TRACK: sort medias by track number
 * @MELO_SORT_TRACKS: sort medias by number of tracks (in album)
 * @MELO_SORT_RELEVANT: sort medias by relevant
 * @MELO_SORT_RATING: sort medias by rating
 * @MELO_SORT_PLAY_COUNT: sort medias by number of play
 *
 * #MeloSort indicates how a media list should be sorted. By default, the sort
 * is in ascendant but it can be reversed (by descendant) with
 * melo_sort_set_desc() helper.
 */
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

  /*< private >*/
  MELO_SORT_COUNT,
} MeloSort;

#define MELO_SORT_DESC 0x1000
#define MELO_SORT_MASK (MELO_SORT_DESC - 1)

/**
 * melo_sort_is_valid:
 * @sort: a #MeloSort
 *
 * Check if a #MeloSort is valid.
 *
 * Returns: %TRUE if the #MeloSort is valid, %FALSE otherwise.
 */
static inline gboolean
melo_sort_is_valid (MeloSort sort)
{
  return (sort & MELO_SORT_MASK) < MELO_SORT_COUNT;
}

/**
 * melo_sort_set_asc:
 * @sort: a #MeloSort
 *
 * Set a #MeloSort to ascendant direction.
 *
 * Returns: a new #MeloSort with ascendant flag set.
 */
static inline MeloSort
melo_sort_set_asc (MeloSort sort)
{
  return (sort & ~MELO_SORT_DESC);
}

/**
 * melo_sort_set_desc:
 * @sort: a #MeloSort
 *
 * Set a #MeloSort to descendant direction.
 *
 * Returns: a new #MeloSort with descendant flag set.
 */
static inline MeloSort
melo_sort_set_desc (MeloSort sort)
{
  return (sort | MELO_SORT_DESC);
}

/**
 * melo_sort_invert:
 * @sort: a #MeloSort
 *
 * Invert sort direction of a #MeloSort.
 *
 * Returns: a new #MeloSort with direction reversed.
 */
static inline MeloSort
melo_sort_invert (MeloSort sort)
{
  return (sort ^ MELO_SORT_DESC);
}

/**
 * melo_sort_is_asc:
 * @sort: a #MeloSort
 *
 * Check if a #MeloSort is sorting in ascendant direction.
 *
 * Returns: %TRUE if the #MeloSort is set in ascendant direction.
 */
static inline gboolean
melo_sort_is_asc (MeloSort sort)
{
  return !(sort & MELO_SORT_DESC);
}

/**
 * melo_sort_is_desc:
 * @sort: a #MeloSort
 *
 * Check if a #MeloSort is sorting in descendant direction.
 *
 * Returns: %TRUE if the #MeloSort is set in descendant direction.
 */
static inline gboolean
melo_sort_is_desc (MeloSort sort)
{
  return (sort & MELO_SORT_DESC);
}

/**
 * melo_sort_replace:
 * @sort: a #MeloSort
 * @new_sort: a #MeloSort with new method to use
 *
 * Replace the sorting method of a #MeloSort with a new method in keeping the
 * flags (ascendant / descendant).
 *
 * Returns: a new #MeloSort with same direction but a new sorting method.
 */
static inline MeloSort
melo_sort_replace (MeloSort sort, MeloSort new_sort)
{
  return (sort & ~MELO_SORT_MASK) | new_sort;
}

const gchar *melo_sort_to_string (MeloSort sort);
MeloSort melo_sort_from_string (const gchar *name);

gint melo_sort_cmp_none (gconstpointer a, gconstpointer b);
gint melo_sort_cmp_shuffle (gconstpointer a, gconstpointer b);

/**
 * melo_sort_cmp_file:
 * @a: a string containing file name
 * @b: a string containing file name to compare with @a
 *
 * Compare two media file names to sort in ascendant direction.
 *
 * Returns: an integer less than, equal to, or greater than zero, if @a is <, ==
 * or > than @b.
 */
#define melo_sort_cmp_file(a,b) (g_strcmp0 (a, b))
/**
 * melo_sort_cmp_file_desc:
 * @a: a string containing file name
 * @b: a string containing file name to compare with @a
 *
 * Compare two media file names to sort in descendant direction.
 *
 * Returns: an integer less than, equal to, or greater than zero, if @a is <, ==
 * or > than @b.
 */
#define melo_sort_cmp_file_desc(a,b) (-g_strcmp0 (a, b))

/**
 * melo_sort_cmp_title:
 * @a: a string containing title name
 * @b: a string containing title name to compare with @a
 *
 * Compare two media title names to sort in ascendant direction.
 *
 * Returns: an integer less than, equal to, or greater than zero, if @a is <, ==
 * or > than @b.
 */
#define melo_sort_cmp_title(a,b) (g_strcmp0 (a, b))
/**
 * melo_sort_cmp_title_desc:
 * @a: a string containing title name
 * @b: a string containing title name to compare with @a
 *
 * Compare two media title names to sort in descendant direction.
 *
 * Returns: an integer less than, equal to, or greater than zero, if @a is <, ==
 * or > than @b.
 */
#define melo_sort_cmp_title_desc(a,b) (-g_strcmp0 (a, b))

/**
 * melo_sort_cmp_artist:
 * @a: a string containing artist name
 * @b: a string containing artist name to compare with @a
 *
 * Compare two media artist names to sort in ascendant direction.
 *
 * Returns: an integer less than, equal to, or greater than zero, if @a is <, ==
 * or > than @b.
 */
#define melo_sort_cmp_artist(a,b) (g_strcmp0 (a, b))
/**
 * melo_sort_cmp_artist_desc:
 * @a: a string containing artist name
 * @b: a string containing artist name to compare with @a
 *
 * Compare two media artist names to sort in descendant direction.
 *
 * Returns: an integer less than, equal to, or greater than zero, if @a is <, ==
 * or > than @b.
 */
#define melo_sort_cmp_artist_desc(a,b) (-g_strcmp0 (a, b))

/**
 * melo_sort_cmp_album:
 * @a: a string containing album name
 * @b: a string containing album name to compare with @a
 *
 * Compare two media album names to sort in ascendant direction.
 *
 * Returns: an integer less than, equal to, or greater than zero, if @a is <, ==
 * or > than @b.
 */
#define melo_sort_cmp_album(a,b) (g_strcmp0 (a, b))
/**
 * melo_sort_cmp_album_desc:
 * @a: a string containing album name
 * @b: a string containing album name to compare with @a
 *
 * Compare two media album names to sort in descendant direction.
 *
 * Returns: an integer less than, equal to, or greater than zero, if @a is <, ==
 * or > than @b.
 */
#define melo_sort_cmp_album_desc(a,b) (-g_strcmp0 (a, b))

/**
 * melo_sort_cmp_genre:
 * @a: a string containing genre name
 * @b: a string containing genre name to compare with @a
 *
 * Compare two media genre names to sort in ascendant direction.
 *
 * Returns: an integer less than, equal to, or greater than zero, if @a is <, ==
 * or > than @b.
 */
#define melo_sort_cmp_genre(a,b) (g_strcmp0 (a, b))
/**
 * melo_sort_cmp_genre_desc:
 * @a: a string containing genre name
 * @b: a string containing genre name to compare with @a
 *
 * Compare two media genre names to sort in descendant direction.
 *
 * Returns: an integer less than, equal to, or greater than zero, if @a is <, ==
 * or > than @b.
 */
#define melo_sort_cmp_genre_desc(a,b) (-g_strcmp0 (a, b))

/**
 * melo_sort_cmp_date:
 * @a: a media date
 * @b: a media date to compare with @a
 *
 * Compare two media dates to sort in ascendant direction.
 *
 * Returns: an integer less than, equal to, or greater than zero, if @a is <, ==
 * or > than @b.
 */
#define melo_sort_cmp_date(a,b) (a > b)
/**
 * melo_sort_cmp_date_desc:
 * @a: a media date
 * @b: a media date to compare with @a
 *
 * Compare two media dates to sort in descendant direction.
 *
 * Returns: an integer less than, equal to, or greater than zero, if @a is <, ==
 * or > than @b.
 */
#define melo_sort_cmp_date_desc(a,b) (a < b)

/**
 * melo_sort_cmp_track:
 * @a: a media track
 * @b: a media track to compare with @a
 *
 * Compare two media tracks to sort in ascendant direction.
 *
 * Returns: an integer less than, equal to, or greater than zero, if @a is <, ==
 * or > than @b.
 */
#define melo_sort_cmp_track(a,b) (a > b)
/**
 * melo_sort_cmp_track_desc:
 * @a: a media track
 * @b: a media track to compare with @a
 *
 * Compare two media tracks to sort in descendant direction.
 *
 * Returns: an integer less than, equal to, or greater than zero, if @a is <, ==
 * or > than @b.
 */
#define melo_sort_cmp_track_desc(a,b) (a < b)

/**
 * melo_sort_cmp_tracks:
 * @a: a media tracks
 * @b: a media tracks to compare with @a
 *
 * Compare two media number of tracks to sort in ascendant direction.
 *
 * Returns: an integer less than, equal to, or greater than zero, if @a is <, ==
 * or > than @b.
 */
#define melo_sort_cmp_tracks(a,b) (a > b)
/**
 * melo_sort_cmp_tracks_desc:
 * @a: a media tracks
 * @b: a media tracks to compare with @a
 *
 * Compare two media number of tracks to sort in descendant direction.
 *
 * Returns: an integer less than, equal to, or greater than zero, if @a is <, ==
 * or > than @b.
 */
#define melo_sort_cmp_tracks_desc(a,b) (a < b)

#endif /* __MELO_SORT_H__ */

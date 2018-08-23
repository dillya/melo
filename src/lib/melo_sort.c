/*
 * melo_sort.c: Media sort enums and helpers
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

#include "melo_sort.h"

/**
 * SECTION:melo_sort
 * @title: MeloSort
 * @short_description: Enumerator to handle media sorting
 *
 * #MeloSort provides the basic media sorting methods to easily generates media
 * list with media sorted by file name, tags (title, artist, album, ...), or
 * usage (relevant, rating).
 * The medias can be sorted in ascendant or descendant direction, except for the
 * shuffle method.
 *
 * Some helpers are provided also to implement complete sorting easily in it is
 * not already the case in the basic object implementation like #MeloBrowser.
 */

static struct {
  MeloSort sort;
  const gchar *name;
  const gchar *name_desc;
} const melo_sort_map[] = {
  { MELO_SORT_NONE,         "none",         "none"               },
  { MELO_SORT_SHUFFLE,      "shuffle",      "shuffle"            },
  { MELO_SORT_FILE,         "file",         "file_desc"          },
  { MELO_SORT_TITLE,        "title",        "title_desc"         },
  { MELO_SORT_ARTIST,       "artist",       "artist_desc"        },
  { MELO_SORT_ALBUM,        "album",        "album_desc"         },
  { MELO_SORT_GENRE,        "genre",        "genre_desc"         },
  { MELO_SORT_DATE,         "date",         "date_desc"          },
  { MELO_SORT_TRACK,        "track",        "track_desc"         },
  { MELO_SORT_TRACKS,       "tracks",       "tracks_desc"        },
  { MELO_SORT_RELEVANT,     "relevant",     "relevant_desc"      },
  { MELO_SORT_RATING,       "rating",       "rating_desc"        },
  { MELO_SORT_PLAY_COUNT,   "play_count",   "play_count_desc"    },
};

/**
 * melo_sort_to_string:
 * @sort: a #MeloSort
 *
 * Convert a #MeloSort to a string.
 *
 * Returns: a string with the translated #MeloSort, %NULL otherwise.
 */
const gchar *
melo_sort_to_string (MeloSort sort)
{
  MeloSort s =  melo_sort_set_asc (sort);
  guint i;

  if (!melo_sort_is_valid (sort))
    return NULL;

  for (i = 0; i < G_N_ELEMENTS (melo_sort_map); i++)
    if (s == melo_sort_map[i].sort)
      return melo_sort_is_desc (sort) ? melo_sort_map[i].name_desc :
                                        melo_sort_map[i].name;

  return NULL;
}

/**
 * melo_sort_from_string:
 * @name: a string with sort method
 *
 * Convert a string to a #MeloSort.
 *
 * Returns: a #MeloSort or an invalid #MeloSort of the method has not been
 * found.
 */
MeloSort
melo_sort_from_string (const gchar *name)
{
  guint i;

  if (!name)
    return MELO_SORT_NONE;

  for (i = 0; i < G_N_ELEMENTS (melo_sort_map); i++) {
    if (!g_strcmp0 (melo_sort_map[i].name, name))
      return melo_sort_map[i].sort;
    if (!g_strcmp0 (melo_sort_map[i].name_desc, name))
      return melo_sort_set_desc (melo_sort_map[i].sort);
  }

  return MELO_SORT_COUNT;
}

/**
 * melo_sort_cmp_none:
 * @a: a void pointer
 * @b: a void pointer
 *
 * Do not compare any medias: no-operation compare function.
 *
 * Returns: 0.
 */
gint
melo_sort_cmp_none (gconstpointer a, gconstpointer b)
{
  return 0;
}

/**
 * melo_sort_cmp_shuffle:
 * @a: a void pointer
 * @b: a void pointer
 *
 * Generate a random number used to shuffle the media list.
 *
 * Returns: a random integer between -10 and 10.
 */
gint
melo_sort_cmp_shuffle (gconstpointer a, gconstpointer b)
{
  return g_random_int_range (-10, 10);
}

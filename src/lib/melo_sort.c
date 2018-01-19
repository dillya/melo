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

struct {
  MeloSort sort;
  const gchar *name;
  const gchar *name_desc;
} static const melo_sort_map[] = {
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

const gchar *
melo_sort_to_string (MeloSort sort)
{
  MeloSort s =  melo_sort_set_asc (sort);
  gint i;

  if (!melo_sort_is_valid (sort))
    return NULL;

  for (i = 0; i < G_N_ELEMENTS (melo_sort_map); i++)
    if (s == melo_sort_map[i].sort)
      return melo_sort_is_desc (sort) ? melo_sort_map[i].name_desc :
                                        melo_sort_map[i].name;

  return NULL;
}

MeloSort
melo_sort_from_string (const gchar *name)
{
  gint i;

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

gint
melo_sort_cmp_none (gconstpointer a, gconstpointer b)
{
  return 0;
}

gint
melo_sort_cmp_shuffle (gconstpointer a, gconstpointer b)
{
  return g_random_int_range (-10, 10);
}

/*
 * Copyright (C) 2020 Alexandre Dilly <dillya@sparod.com>
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA.
 */

#include <stdatomic.h>

#define MELO_LOG_TAG "tags"
#include "melo/melo_log.h"

#include "melo/melo_browser.h"
#include "melo/melo_cover.h"
#include "melo/melo_player.h"
#include "melo/melo_tags.h"

struct _MeloTags {
  atomic_int ref_count;
  char *title;
  char *artist;
  char *album;
  char *genre;
  unsigned int track;
  char *cover;
};

/**
 * melo_tags_new:
 *
 * Creates a new #MeloTags.
 *
 * Returns: (transfer full): a new #MeloTags.
 */
MeloTags *
melo_tags_new (void)
{
  MeloTags *tags;

  /* Allocate new tags */
  tags = calloc (1, sizeof (*tags));
  if (!tags)
    return NULL;

  /* Initialize reference counter */
  atomic_init (&tags->ref_count, 1);

  return tags;
}

/**
 * melo_tags_ref:
 * @tags: a #MeloTags
 *
 * Increase the reference count on @tags.
 *
 * Returns: the #MeloTags.
 */
MeloTags *
melo_tags_ref (MeloTags *tags)
{
  if (!tags)
    return NULL;

  atomic_fetch_add (&tags->ref_count, 1);

  return tags;
}

/**
 * melo_tags_unref:
 * @tags: (nullable): a #MeloTags
 *
 * Releases a reference on @tags. This may result in the tags being freed.
 */
void
melo_tags_unref (MeloTags *tags)
{
  int ref_count;

  if (!tags)
    return;

  ref_count = atomic_fetch_sub (&tags->ref_count, 1);
  if (ref_count == 1) {
    g_free (tags->title);
    g_free (tags->artist);
    g_free (tags->album);
    g_free (tags->genre);
    g_free (tags->cover);
    free (tags);
  } else if (ref_count < 1)
    MELO_LOGC ("negative reference counter %d", ref_count - 1);
}

/**
 * melo_tags_new_from_taglist:
 * @obj: (nullable): the origin object
 * @tag_list: a #GstTagList to use
 *
 * Creates a new #MeloTags and fill it with tags found in @tag_list. For the
 * @obj parameter, please refer to melo_tags_set_cover() documentation.
 *
 * Returns: (transfer full): a new #MeloTags.
 */
MeloTags *
melo_tags_new_from_taglist (GObject *obj, const GstTagList *tag_list)
{
  MeloTags *tags;

  /* Create new tags */
  tags = melo_tags_new ();
  if (!tags)
    return NULL;

  /* Parse tag list */
  if (tag_list) {
    GstSample *sample;
    char *cover;

    /* Set strings */
    gst_tag_list_get_string (tag_list, GST_TAG_TITLE, &tags->title);
    gst_tag_list_get_string (tag_list, GST_TAG_ARTIST, &tags->artist);
    gst_tag_list_get_string (tag_list, GST_TAG_ALBUM, &tags->album);
    gst_tag_list_get_string (tag_list, GST_TAG_GENRE, &tags->genre);
    gst_tag_list_get_uint (tag_list, GST_TAG_TRACK_NUMBER, &tags->track);

    /* Find a cover */
    sample = melo_cover_extract_from_gst_tags_list (tag_list);
    cover = melo_cover_cache_save_gst_sample (sample);
    melo_tags_set_cover (tags, obj, cover);
    g_free (cover);
  }

  return tags;
}

/**
 * melo_tags_merge:
 * @new: a #MeloTags to merge to
 * @old: a #MeloTags to merge from
 * @flags: a combination of #MeloTagsMergeFlag
 *
 * This function can be used to merge the fields from @old into @new. If a field
 * of @new is already set, the field of @old will be discarded. The @flags
 * parameter can be used to specify merge operations, like skipping some tags
 * data copy.
 *
 * Returns: (transfer full): the newly #MeloTags with merged fields.
 */
MeloTags *
melo_tags_merge (MeloTags *new, MeloTags *old, unsigned int flags)
{
  if (!new || !old)
    return new;

  if (!(flags & MELO_TAGS_MERGE_FLAG_SKIP_TITLE) && !new->title)
    new->title = g_strdup (old->title);
  if (!(flags & MELO_TAGS_MERGE_FLAG_SKIP_ARTIST) && !new->artist)
    new->artist = g_strdup (old->artist);
  if (!(flags & MELO_TAGS_MERGE_FLAG_SKIP_ALBUM) && !new->album)
    new->album = g_strdup (old->album);
  if (!(flags & MELO_TAGS_MERGE_FLAG_SKIP_GENRE) && !new->genre)
    new->genre = g_strdup (old->genre);
  if (!(flags & MELO_TAGS_MERGE_FLAG_SKIP_TRACK) && !new->track)
    new->track = old->track;
  if (!(flags & MELO_TAGS_MERGE_FLAG_SKIP_COVER) && !new->cover)
    new->cover = g_strdup (old->cover);
  melo_tags_unref (old);

  return new;
}

/**
 * melo_tags_set_title:
 * @tags: a #MeloTags
 * @title: the media title to set
 *
 * This function can be used to set the media title in @tags. If a media title
 * is already set, the functions will returns %false. To update a #MeloTags,
 * please consider melo_tags_merge().
 *
 * Returns: %true if the media title has been set, %false otherwise.
 */
bool
melo_tags_set_title (MeloTags *tags, const char *title)
{
  if (!tags || tags->title)
    return false;

  /* Set title */
  tags->title = g_strdup (title);

  return true;
}

/**
 * melo_tags_set_artist:
 * @tags: a #MeloTags
 * @artist: the media artist to set
 *
 * This function can be used to set the media artist in @tags. If a media artist
 * is already set, the functions will returns %false. To update a #MeloTags,
 * please consider melo_tags_merge().
 *
 * Returns: %true if the media artist has been set, %false otherwise.
 */
bool
melo_tags_set_artist (MeloTags *tags, const char *artist)
{
  if (!tags || tags->artist)
    return false;

  /* Set artist */
  tags->artist = g_strdup (artist);

  return true;
}

/**
 * melo_tags_set_album:
 * @tags: a #MeloTags
 * @album: the media album to set
 *
 * This function can be used to set the media album in @tags. If a media album
 * is already set, the functions will returns %false. To update a #MeloTags,
 * please consider melo_tags_merge().
 *
 * Returns: %true if the media album has been set, %false otherwise.
 */
bool
melo_tags_set_album (MeloTags *tags, const char *album)
{
  if (!tags || tags->album)
    return false;

  /* Set album */
  tags->album = g_strdup (album);

  return true;
}

/**
 * melo_tags_set_genre:
 * @tags: a #MeloTags
 * @genre: the media genre to set
 *
 * This function can be used to set the media genre in @tags. If a media genre
 * is already set, the functions will returns %false. To update a #MeloTags,
 * please consider melo_tags_merge().
 *
 * Returns: %true if the media genre has been set, %false otherwise.
 */
bool
melo_tags_set_genre (MeloTags *tags, const char *genre)
{
  if (!tags || tags->genre)
    return false;

  /* Set genre */
  tags->genre = g_strdup (genre);

  return true;
}

/**
 * melo_tags_set_track:
 * @tags: a #MeloTags
 * @track: the media track to set
 *
 * This function can be used to set the media track in @tags. If a media track
 * is already set, the functions will returns %false. To update a #MeloTags,
 * please consider melo_tags_merge().
 *
 * Returns: %true if the media track has been set, %false otherwise.
 */
bool
melo_tags_set_track (MeloTags *tags, unsigned int track)
{
  if (!tags || tags->track)
    return false;

  /* Set track */
  tags->track = track;

  return true;
}

/**
 * melo_tags_set_cover:
 * @tags: a #MeloTags
 * @obj: (nullable): the origin object
 * @cover: the media cover to set
 *
 * This function can be used to set the media cover in @tags. If a media cover
 * is already set, the functions will returns %false. To update a #MeloTags,
 * please consider melo_tags_merge().
 *
 * The parameter @obj can be set to specify the origin of the cover: when cover
 * is set from a #MeloBrowser or a #MeloPlayer, the final following prefix will
 * be added to the cover string, respectively, "browser/ID/" and "player/ID/",
 * where ID is the object ID. This new cover string can then be used to retrieve
 * the cover data with melo_browser_get_asset() and melo_player_get_asset().
 *
 * Returns: %true if the media cover has been set, %false otherwise.
 */
bool
melo_tags_set_cover (MeloTags *tags, GObject *obj, const char *cover)
{
  if (!tags || tags->cover)
    return false;

  /* Set cover */
  tags->cover = melo_tags_gen_cover (obj, cover);

  return true;
}

/**
 * melo_tags_get_title:
 * @tags: a #MeloTags
 *
 * Returns: the media title or %NULL.
 */
const char *
melo_tags_get_title (MeloTags *tags)
{
  return tags ? tags->title : NULL;
}

/**
 * melo_tags_get_artist:
 * @tags: a #MeloTags
 *
 * Returns: the media artist or %NULL.
 */
const char *
melo_tags_get_artist (MeloTags *tags)
{
  return tags ? tags->artist : NULL;
}

/**
 * melo_tags_get_album:
 * @tags: a #MeloTags
 *
 * Returns: the media album or %NULL.
 */
const char *
melo_tags_get_album (MeloTags *tags)
{
  return tags ? tags->album : NULL;
}

/**
 * melo_tags_get_genre:
 * @tags: a #MeloTags
 *
 * Returns: the media genre or %NULL.
 */
const char *
melo_tags_get_genre (MeloTags *tags)
{
  return tags ? tags->genre : NULL;
}

/**
 * melo_tags_get_track:
 * @tags: a #MeloTags
 *
 * Returns: the media track or 0.
 */
unsigned int
melo_tags_get_track (MeloTags *tags)
{
  return tags ? tags->track : 0;
}

/**
 * melo_tags_get_cover:
 * @tags: a #MeloTags
 *
 * Returns: the media cover or %NULL.
 */
const char *
melo_tags_get_cover (MeloTags *tags)
{
  return tags ? tags->cover : NULL;
}

/**
 * melo_tags_gen_cover:
 * @obj: (nullable): the origin object
 * @cover: the media cover to set
 *
 * This function can be used to generate the media cover string which is used
 * in #MeloTags.
 *
 * The parameter @obj can be set to specify the origin of the cover: when cover
 * is set from a #MeloBrowser or a #MeloPlayer, the final following prefix will
 * be added to the cover string, respectively, "browser/ID/" and "player/ID/",
 * where ID is the object ID. This new cover string can then be used to retrieve
 * the cover data with melo_browser_get_asset() and melo_player_get_asset().
 *
 * Returns: the cover string generated.
 */
char *
melo_tags_gen_cover (GObject *obj, const char *cover)
{
  char *cov;

  /* Set cover */
  if (obj && cover) {
    gchar *id = NULL;

    /* Check object */
    if (MELO_IS_BROWSER (obj)) {
      g_object_get (obj, "id", &id, NULL);
      cov = g_strconcat ("browser/", id, "/", cover, NULL);
    } else if (MELO_IS_PLAYER (obj)) {
      g_object_get (obj, "id", &id, NULL);
      cov = g_strconcat ("player/", id, "/", cover, NULL);
    } else
      cov = g_strdup (cover);
    g_free (id);
  } else
    cov = g_strdup (cover);

  return cov;
}

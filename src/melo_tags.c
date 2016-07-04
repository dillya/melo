/*
 * melo_tags.c: Media tags handler
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

#include "melo_tags.h"

MeloTags *
melo_tags_new (void)
{
  return g_slice_new0 (MeloTags);
}

MeloTags *
melo_tags_copy (MeloTags *src)
{
  MeloTags *dest;

  /* Create a new tags */
  dest = melo_tags_new ();
  if (!dest)
    return NULL;

  /* Copy tags */
  dest->title = g_strdup (src->title);
  dest->artist = g_strdup (src->artist);
  dest->album = g_strdup (src->album);
  dest->genre = g_strdup (src->genre);
  dest->date = src->date;
  dest->track = src->track;
  dest->tracks = src->tracks;

  return dest;
}

void
melo_tags_update_from_gst_tag_list (MeloTags *tags, GstTagList *tlist,
                                    MeloTagsFields fields)
{
  /* Clear before update */
  melo_tags_clear (tags);

  /* No fields to read */
  if (fields == MELO_TAGS_FIELDS_NONE)
    return;

  /* Fill MeloTags from GstTagList */
  if (fields & MELO_TAGS_FIELDS_TITLE)
    gst_tag_list_get_string (tlist, GST_TAG_TITLE, &tags->title);
  if (fields & MELO_TAGS_FIELDS_ARTIST)
    gst_tag_list_get_string (tlist, GST_TAG_ARTIST, &tags->artist);
  if (fields & MELO_TAGS_FIELDS_ALBUM)
    gst_tag_list_get_string (tlist, GST_TAG_ALBUM, &tags->album);
  if (fields & MELO_TAGS_FIELDS_GENRE)
    gst_tag_list_get_string (tlist, GST_TAG_GENRE, &tags->genre);
  if (fields & MELO_TAGS_FIELDS_DATE)
    gst_tag_list_get_int (tlist, GST_TAG_DATE, &tags->date);
  if (fields & MELO_TAGS_FIELDS_TRACK)
    gst_tag_list_get_uint (tlist, GST_TAG_TRACK_NUMBER, &tags->track);
  if (fields & MELO_TAGS_FIELDS_TRACKS)
    gst_tag_list_get_uint (tlist, GST_TAG_TRACK_COUNT, &tags->tracks);
}

MeloTagsFields
melo_tags_get_fields_from_json_array (JsonArray *array)
{
  MeloTagsFields fields = MELO_TAGS_FIELDS_NONE;
  const gchar *field;
  guint count, i;

  /* Parse array */
  count = json_array_get_length (array);
  for (i = 0; i < count; i++) {
    field = json_array_get_string_element (array, i);
    if (!field)
      break;
    if (!g_strcmp0 (field, "none")) {
      fields = MELO_TAGS_FIELDS_NONE;
      break;
    } else if (!g_strcmp0 (field, "full")) {
      fields = MELO_TAGS_FIELDS_FULL;
      break;
    } else if (!g_strcmp0 (field, "title"))
      fields |= MELO_TAGS_FIELDS_TITLE;
    else if (!g_strcmp0 (field, "artist"))
      fields |= MELO_TAGS_FIELDS_ARTIST;
    else if (!g_strcmp0 (field, "album"))
      fields |= MELO_TAGS_FIELDS_ALBUM;
    else if (!g_strcmp0 (field, "genre"))
      fields |= MELO_TAGS_FIELDS_GENRE;
    else if (!g_strcmp0 (field, "date"))
      fields |= MELO_TAGS_FIELDS_DATE;
    else if (!g_strcmp0 (field, "track"))
      fields |= MELO_TAGS_FIELDS_TRACK;
    else if (!g_strcmp0 (field, "tracks"))
      fields |= MELO_TAGS_FIELDS_TRACKS;
  }

  return fields;
}

void
melo_tags_add_to_json_object (MeloTags *tags, JsonObject *obj,
                              MeloTagsFields fields)
{
  /* Nothing to do */
  if (fields == MELO_TAGS_FIELDS_NONE)
    return;

  /* Fill object */
  if (fields & MELO_TAGS_FIELDS_TITLE)
    json_object_set_string_member (obj, "title", tags->title);
  if (fields & MELO_TAGS_FIELDS_ARTIST)
    json_object_set_string_member (obj, "artist", tags->artist);
  if (fields & MELO_TAGS_FIELDS_ALBUM)
    json_object_set_string_member (obj, "album", tags->album);
  if (fields & MELO_TAGS_FIELDS_GENRE)
    json_object_set_string_member (obj, "genre", tags->genre);
  if (fields & MELO_TAGS_FIELDS_DATE)
    json_object_set_int_member (obj, "date", tags->date);
  if (fields & MELO_TAGS_FIELDS_TRACK)
    json_object_set_int_member (obj, "track", tags->track);
  if (fields & MELO_TAGS_FIELDS_TRACKS)
    json_object_set_int_member (obj, "tracks", tags->tracks);
}

JsonObject *
melo_tags_to_json_object (MeloTags *tags, MeloTagsFields fields)
{
  JsonObject *obj;

  /* Create a new JSON object */
  obj = json_object_new ();
  if (!obj)
    return NULL;

  /* Fill object */
  melo_tags_add_to_json_object (tags, obj, fields);

  return obj;
}

void
melo_tags_clear (MeloTags *tags)
{
  g_clear_pointer (&tags->title, g_free);
  g_clear_pointer (&tags->artist, g_free);
  g_clear_pointer (&tags->album, g_free);
  g_clear_pointer (&tags->genre, g_free);
  memset (tags, 0, sizeof (*tags));
}

void
melo_tags_free (MeloTags *tags)
{
  g_free (tags->title);
  g_free (tags->artist);
  g_free (tags->album);
  g_free (tags->genre);
  g_slice_free (MeloTags, tags);
}

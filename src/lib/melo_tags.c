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

#include <gst/tag/tag.h>

#include "melo_tags.h"
#include "melo_browser.h"

/* Cover URL base */
G_LOCK_DEFINE_STATIC (melo_tags_mutex);
static gchar *melo_tags_cover_url_base = NULL;

struct _MeloTagsPrivate {
  GMutex mutex;
  gint64 timestamp;
  gint ref_count;

  /* Cover data */
  GBytes *cover;
  gchar *cover_url;
  gchar *cover_type;
};

MeloTags *
melo_tags_new (void)
{
  MeloTags *tags;

  /* Allocate new tags */
  tags = g_slice_new0 (MeloTags);
  if (!tags)
    return NULL;

  /* Allocate private */
  tags->priv = g_slice_new0 (MeloTagsPrivate);
  if (!tags->priv) {
    g_slice_free (MeloTags, tags);
    return NULL;
  }

  /* Init private mutex */
  g_mutex_init (&tags->priv->mutex);

  /* Set reference counter to 1 */
  tags->priv->ref_count = 1;

  /* Set initial timestamp */
  melo_tags_update (tags);

  return tags;
}

void
melo_tags_update (MeloTags *tags)
{
  tags->priv->timestamp = g_get_monotonic_time ();
}

gboolean
melo_tags_updated (MeloTags *tags, gint64 timestamp)
{
  return tags->priv->timestamp > timestamp;
}

MeloTags *
melo_tags_copy (MeloTags *tags)
{
  MeloTagsPrivate *priv = tags->priv;
  MeloTagsPrivate *npriv;
  MeloTags *ntags;

  /* Create a new MeloTags */
  ntags = melo_tags_new ();
  if (!ntags)
    return NULL;

  /* Copy values */
  ntags->title = g_strdup (tags->title);
  ntags->artist = g_strdup (tags->artist);
  ntags->album = g_strdup (tags->album);
  ntags->genre = g_strdup (tags->genre);
  ntags->date = tags->date;
  ntags->track = tags->track;
  ntags->tracks = tags->tracks;
  npriv = ntags->priv;

  /* Lock cover access */
  g_mutex_lock (&priv->mutex);

  /* Copy cover values */
  if (priv->cover)
    npriv->cover = g_bytes_ref (priv->cover);
  npriv->cover_url = g_strdup (priv->cover_url);
  npriv->cover_type = g_strdup (priv->cover_type);

  /* Unlock cover access */
  g_mutex_unlock (&priv->mutex);

  return ntags;
}

void
melo_tags_merge (MeloTags *tags, MeloTags *old_tags)
{
  MeloTagsPrivate *priv = tags->priv;
  MeloTagsPrivate *opriv = old_tags->priv;

  /* Copy values */
  if (!tags->title)
    tags->title = g_strdup (old_tags->title);
  if (!tags->artist)
    tags->artist = g_strdup (old_tags->artist);
  if (!tags->album)
    tags->album = g_strdup (old_tags->album);
  if (!tags->genre)
    tags->genre = g_strdup (old_tags->genre);
  if (!tags->date)
    tags->date = old_tags->date;
  if (!tags->track)
    tags->track = old_tags->track;
  if (!tags->tracks)
    tags->tracks = old_tags->tracks;

  /* Lock cover access */
  g_mutex_lock (&priv->mutex);
  g_mutex_lock (&opriv->mutex);

  /* Free previous cover */
  if (!priv->cover && !priv->cover_url) {
    g_free (priv->cover_type);
    g_free (priv->cover_url);
    if (opriv->cover)
      priv->cover = g_bytes_ref (opriv->cover);
    priv->cover_url = g_strdup (opriv->cover_url);
    priv->cover_type = g_strdup (opriv->cover_type);
  }

  /* Update timestamp */
  melo_tags_update (tags);

  /* Unlock cover access */
  g_mutex_unlock (&opriv->mutex);
  g_mutex_unlock (&priv->mutex);
}

MeloTags *
melo_tags_ref (MeloTags *tags)
{
  if (tags)
    tags->priv->ref_count++;
  return tags;
}

void
melo_tags_set_cover (MeloTags *tags, GBytes *cover, const gchar *type)
{
  if (cover)
    g_bytes_ref (cover);
  melo_tags_take_cover (tags, cover, type);
}

void
melo_tags_take_cover (MeloTags *tags, GBytes *cover, const gchar *type)
{
  MeloTagsPrivate *priv = tags->priv;

  /* Lock cover access */
  g_mutex_lock (&priv->mutex);

  /* Free previous cover */
  if (priv->cover)
    g_bytes_unref (priv->cover);

  /* Set cover */
  priv->cover = cover;
  if (type) {
    g_free (priv->cover_type);
    priv->cover_type = g_strdup (type);
  }

  /* Update timestamp */
  melo_tags_update (tags);

  /* Unlock cover access */
  g_mutex_unlock (&priv->mutex);
}

gboolean
melo_tags_has_cover (MeloTags *tags)
{
  return tags->priv->cover ? TRUE : FALSE;
}

GBytes *
melo_tags_get_cover (MeloTags *tags, gchar **type)
{
  MeloTagsPrivate *priv = tags->priv;
  GBytes *cover = NULL;

  /* Lock cover access */
  g_mutex_lock (&priv->mutex);

  /* Get cover */
  if (priv->cover) {
    /* Take a reference of cover */
    cover = g_bytes_ref (priv->cover);

    /* Set cover type */
    if (type)
      *type = g_strdup (priv->cover_type);
  }

  /* Unlock cover access */
  g_mutex_unlock (&priv->mutex);

  return cover;
}

gboolean
melo_tags_has_cover_type (MeloTags *tags)
{
  return tags->priv->cover_type ? TRUE : FALSE;
}

gchar *
melo_tags_get_cover_type (MeloTags *tags)
{
  MeloTagsPrivate *priv = tags->priv;
  gchar *type = NULL;

  /* Lock cover access */
  g_mutex_lock (&priv->mutex);

  /* Get cover */
  if (priv->cover)
      type = g_strdup (priv->cover_type);

  /* Unlock cover access */
  g_mutex_unlock (&priv->mutex);

  return type;
}

gboolean
melo_tags_set_cover_url (MeloTags *tags, GObject *obj, const gchar *path,
                         const gchar *type)
{
  MeloTagsPrivate *priv = tags->priv;
  const gchar *otype = NULL;
  const gchar *id = NULL;
  gchar *url = NULL;
  gchar del ='/';

  /* Lock object */
  g_object_ref (obj);

  /* Get ID from object */
  if (MELO_IS_BROWSER (obj)) {
    id = melo_browser_get_id (MELO_BROWSER (obj));
    otype = "browser";
  } else if (MELO_IS_PLAYER (obj)) {
    id = melo_player_get_id (MELO_PLAYER (obj));
    otype = "player";
    path = "";
    del = '\0';
  } else if (MELO_IS_PLAYLIST (obj)) {
    id = melo_playlist_get_id (MELO_PLAYLIST (obj));
    otype = "playlist";
  }

  /* No ID found */
  if (!id)
    goto failed;

  G_LOCK (melo_tags_mutex);

  /* Generate cover URL from ID and path */
  if (!path)
    url = NULL;
  else if (melo_tags_cover_url_base)
    url = g_strdup_printf ("%s/%s/%s%c%s", melo_tags_cover_url_base, otype, id,
                           del, path);
  else
    url = g_strdup_printf ("%s/%s%c%s", otype, id, path, del, path);

  G_UNLOCK (melo_tags_mutex);

  /* Free object */
  g_object_unref (obj);

  /* Lock cover access */
  g_mutex_lock (&priv->mutex);

  /* Set cover URL */
  g_free (priv->cover_url);
  priv->cover_url = url;

  /* Update cover type */
  if (type) {
    g_free (priv->cover_type);
    priv->cover_type = g_strdup (type);
  }

  /* Update timestamp */
  melo_tags_update (tags);

  /* Unlock cover access */
  g_mutex_unlock (&priv->mutex);

  return TRUE;

failed:
  g_object_unref (obj);
  return FALSE;
}

void
melo_tags_copy_cover_url (MeloTags *tags, const gchar *url, const gchar *type)
{
  MeloTagsPrivate *priv = tags->priv;

  /* Lock cover access */
  g_mutex_lock (&priv->mutex);

  /* Set cover URL */
  g_free (priv->cover_url);
  priv->cover_url = g_strdup (url);

  /* Update cover type */
  if (type) {
    g_free (priv->cover_type);
    priv->cover_type = g_strdup (type);
  }

  /* Update timestamp */
  melo_tags_update (tags);

  /* Unlock cover access */
  g_mutex_unlock (&priv->mutex);
}

gboolean
melo_tags_has_cover_url (MeloTags *tags)
{
  return tags->priv->cover_url ? TRUE : FALSE;
}

gchar *
melo_tags_get_cover_url (MeloTags *tags)
{
  MeloTagsPrivate *priv = tags->priv;
  gchar *url = NULL;

  /* Lock cover access */
  g_mutex_lock (&priv->mutex);

  /* Get cover URL */
  if (priv->cover_url)
      url = g_strdup (priv->cover_url);

  /* Unlock cover access */
  g_mutex_unlock (&priv->mutex);

  return url;
}

void
melo_tags_set_cover_url_base (const gchar *base)
{
  G_LOCK (melo_tags_mutex);
  g_free (melo_tags_cover_url_base);
  melo_tags_cover_url_base = g_strdup (base);
  G_UNLOCK (melo_tags_mutex);
}

gboolean
melo_tags_get_cover_from_url (const gchar *url, GBytes **data, gchar **type)
{
  gchar *otype, *id, *path;
  gchar **values = NULL;
  gboolean ret = FALSE;
  gint base_len = 0;
  gint len;

  G_LOCK (melo_tags_mutex);

  /* Check URL base */
  if (melo_tags_cover_url_base)
    base_len = strlen (melo_tags_cover_url_base) + 1;

  G_UNLOCK (melo_tags_mutex);

  /* Check string length */
  len = strlen (url);
  if (!len || len < base_len)
    return FALSE;

  /* Split URL (by removing base URL */
  values = g_strsplit (url + base_len, "/", 3);
  if (!values || !values[0] || !values[1])
    goto end;

  /* Get element type, ID and path */
  otype = values[0];
  id = values[1];
  path = values[2];

  /* Get cover from element */
  if (!g_strcmp0 (otype, "browser") && path) {
    MeloBrowser *browser;

    /* Get browser from its ID */
    browser = melo_browser_get_browser_by_id (id);
    if (browser) {
      ret = melo_browser_get_cover (browser, path, data, type);
      g_object_unref (browser);
    }
  } else if (!g_strcmp0 (otype, "player")) {
    MeloPlayer *player;

    /* Get player from its ID */
    player = melo_player_get_player_by_id (id);
    if (player) {
      ret = melo_player_get_cover (player, data, type);
      g_object_unref (player);
    }
  } else if (!g_strcmp0 (otype, "playlist") && path) {
    MeloPlaylist *playlist;

    /* Get playlist from its ID */
    playlist = melo_playlist_get_playlist_by_id (id);
    if (playlist) {
      ret = melo_playlist_get_cover (playlist, path, data, type);
      g_object_unref (playlist);
    }
  } else
    goto end;

end:
  /* Free URL values */
  g_strfreev (values);

  return ret;
}

MeloTags *
melo_tags_new_from_gst_tag_list (const GstTagList *tlist, MeloTagsFields fields)
{
  MeloTags *tags;

  /* Create new tags */
  tags = melo_tags_new ();
  if (!tags)
    return NULL;

  /* No fields to read */
  if (fields == MELO_TAGS_FIELDS_NONE)
    return tags;

  /* Fill MeloTags from GstTagList */
  if (fields & MELO_TAGS_FIELDS_TITLE)
    gst_tag_list_get_string (tlist, GST_TAG_TITLE, &tags->title);
  if (fields & MELO_TAGS_FIELDS_ARTIST)
    gst_tag_list_get_string (tlist, GST_TAG_ARTIST, &tags->artist);
  if (fields & MELO_TAGS_FIELDS_ALBUM)
    gst_tag_list_get_string (tlist, GST_TAG_ALBUM, &tags->album);
  if (fields & MELO_TAGS_FIELDS_GENRE)
    gst_tag_list_get_string (tlist, GST_TAG_GENRE, &tags->genre);
  if (fields & MELO_TAGS_FIELDS_TRACK)
    gst_tag_list_get_uint (tlist, GST_TAG_TRACK_NUMBER, &tags->track);
  if (fields & MELO_TAGS_FIELDS_TRACKS)
    gst_tag_list_get_uint (tlist, GST_TAG_TRACK_COUNT, &tags->tracks);

  /* Get date */
  if (fields & MELO_TAGS_FIELDS_DATE) {
    GDate *date;

    /* Get only year from GDate */
    if (gst_tag_list_get_date (tlist, GST_TAG_DATE, &date)) {
      tags->date = g_date_get_year (date);
      g_date_free (date);
    }
  }

  /* Get album / single cover */
  if (fields & MELO_TAGS_FIELDS_COVER) {
    GstSample *final_sample = NULL;
    gint count, i;

    /* Find the best image (front cover if possible) */
    count = gst_tag_list_get_tag_size(tlist, GST_TAG_IMAGE);
    for (i = 0; i < count; i++) {
      GstTagImageType type = GST_TAG_IMAGE_TYPE_NONE;
      const GstStructure *info;
      GstSample *sample;

      /* Get next image */
      if (!gst_tag_list_get_sample_index (tlist, GST_TAG_IMAGE, i, &sample))
        continue;

      /* Get infos about image */
      info = gst_sample_get_info (sample);
      if (!info) {
        gst_sample_unref (sample);
        continue;
      }

      /* Get image type */
      gst_structure_get_enum (info, "image-type", GST_TYPE_TAG_IMAGE_TYPE,
                              &type);
      /* Select only front cover or first undefined image */
      if (type == GST_TAG_IMAGE_TYPE_FRONT_COVER ||
          (type == GST_TAG_IMAGE_TYPE_UNDEFINED && final_sample == NULL)) {
        if (final_sample)
          gst_sample_unref (final_sample);
        final_sample = sample;
        continue;
      }
      gst_sample_unref (sample);
    }

    /* Get preview image if no image found */
    if (!final_sample) {
      GstSample *sample;

      /* Get preview */
      if (gst_tag_list_get_sample (tlist, GST_TAG_PREVIEW_IMAGE, &sample))
        final_sample = sample;
    }

    /* Copy found image */
    if (final_sample) {
        GstBuffer *buffer;
        GstCaps *caps;
        gpointer data;
        gsize size, dsize;
        gchar *c;

        /* Get buffer and caps */
        buffer = gst_sample_get_buffer (final_sample);
        caps = gst_sample_get_caps (final_sample);

        /* Extract data from buffer */
        size = gst_buffer_get_size (buffer);
        gst_buffer_extract_dup (buffer, 0, size, &data, &dsize);

        /* Create a new GBytes with data */
        tags->priv->cover = g_bytes_new_take (data, dsize);

        /* Copy type string */
        tags->priv->cover_type = gst_caps_to_string (caps);
        c = g_strstr_len (tags->priv->cover_type, -1, ",");
        if (c)
          *c = '\0';
        gst_sample_unref (final_sample);
    }
  }

  return tags;
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
    else if (!g_strcmp0 (field, "cover"))
      fields |= MELO_TAGS_FIELDS_COVER;
    else if (!g_strcmp0 (field, "cover_url"))
      fields |= MELO_TAGS_FIELDS_COVER_URL;
  }

  return fields;
}

void
melo_tags_add_to_json_object (MeloTags *tags, JsonObject *obj,
                              MeloTagsFields fields)
{
  /* Nothing to do */
  if (!tags || fields == MELO_TAGS_FIELDS_NONE)
    return;

  /* Set timestamp in any case */
  json_object_set_int_member (obj, "timestamp", tags->priv->timestamp);

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

  /* Convert image to base64 */
  if (fields & MELO_TAGS_FIELDS_COVER) {
    MeloTagsPrivate *priv = tags->priv;

    /* Lock cover */
    g_mutex_lock (&priv->mutex);

    if (priv->cover) {
      const guchar *data;
      gsize size;
      gchar *cover;

      /* Get data and encode */
      data = g_bytes_get_data (priv->cover, &size);
      cover = g_base64_encode (data, size);

      /* Add to object */
      json_object_set_string_member (obj, "cover", cover);
      json_object_set_string_member (obj, "cover_type", priv->cover_type);
      g_free (cover);
    }

    /* Unlock cover */
    g_mutex_unlock (&priv->mutex);
  }

  /* Add cover URL */
  if (fields & MELO_TAGS_FIELDS_COVER_URL) {
    MeloTagsPrivate *priv = tags->priv;

    /* Lock cover */
    g_mutex_lock (&priv->mutex);

    /* Add to object */
    if (priv->cover_url) {
      json_object_set_string_member (obj, "cover_url", priv->cover_url);
      json_object_set_string_member (obj, "cover_type", priv->cover_type);
    }

    /* Unlock cover */
    g_mutex_unlock (&priv->mutex);
  }
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
melo_tags_unref (MeloTags *tags)
{
  if (!tags)
    return;

  /* Decrement reference count */
  tags->priv->ref_count--;
  if (tags->priv->ref_count)
    return;

  /* Free tags */
  g_free (tags->title);
  g_free (tags->artist);
  g_free (tags->album);
  g_free (tags->genre);
  g_bytes_unref (tags->priv->cover);
  g_free (tags->priv->cover_url);
  g_free (tags->priv->cover_type);
  g_mutex_clear (&tags->priv->mutex);
  g_slice_free (MeloTagsPrivate, tags->priv);
  g_slice_free (MeloTags, tags);
}

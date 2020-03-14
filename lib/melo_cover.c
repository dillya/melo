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

#include <gio/gio.h>
#include <gst/tag/tag.h>

#define MELO_LOG_TAG "cover"
#include "melo/melo_log.h"

#include "melo/melo_cover.h"

#define MELO_COVER_CACHE_PATH "/melo/cover_cache/"

static char *melo_cover_cache_path;

/**
 * melo_cover_cache_init:
 *
 * Initializes the Melo library internal cover cache.
 */
void
melo_cover_cache_init ()
{
  /* Build cover cache path */
  melo_cover_cache_path =
      g_strconcat (g_get_user_data_dir (), MELO_COVER_CACHE_PATH, NULL);

  /* Create cache directory */
  g_mkdir_with_parents (melo_cover_cache_path, 0700);
}

/**
 * melo_cover_cache_deinit:
 *
 * Clean up any resources created by Melo library in melo_cover_cache_init().
 */
void
melo_cover_cache_deinit ()
{
  g_free (melo_cover_cache_path);
}

/**
 * melo_cover_type_from_mime_type:
 * @type: the mime type to convert
 *
 * Convert a mime-type to a #MeloCoverType.
 *
 * Returns: a #MeloCoverType corresponding to the mime-type.
 */
MeloCoverType
melo_cover_type_from_mime_type (const char *type)
{
  if (!type)
    return MELO_COVER_TYPE_UNKNOWN;
  else if (!strcmp (type, "image/jpeg"))
    return MELO_COVER_TYPE_JPEG;
  else if (!strcmp (type, "image/png"))
    return MELO_COVER_TYPE_PNG;

  return MELO_COVER_TYPE_UNKNOWN;
}

static void
close_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GOutputStream *stream = G_OUTPUT_STREAM (source_object);

  /* Close finished */
  if (!g_output_stream_close_finish (stream, res, NULL))
    MELO_LOGW ("failed to close cover");

  /* Release stream */
  g_object_unref (stream);
}

static void
write_all_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GOutputStream *stream = G_OUTPUT_STREAM (source_object);
  GBytes *bytes = user_data;

  /* Write finished */
  if (!g_output_stream_write_all_finish (stream, res, NULL, NULL))
    MELO_LOGW ("failed to write cover");

  /* Release data */
  g_bytes_unref (bytes);

  /* Close stream */
  g_output_stream_close_async (
      stream, G_PRIORITY_DEFAULT, NULL, close_cb, NULL);
}

static void
create_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GFile *file = G_FILE (source_object);
  GBytes *bytes = user_data;
  GFileOutputStream *stream;

  /* Get IO stream */
  stream = g_file_create_finish (file, res, NULL);
  if (!stream) {
    /* File already exists */
    g_object_unref (file);
    g_bytes_unref (bytes);
    return;
  }

  /* Free file */
  g_object_unref (file);

  /* Write data to file */
  g_output_stream_write_all_async (G_OUTPUT_STREAM (stream),
      g_bytes_get_data (bytes, NULL), g_bytes_get_size (bytes),
      G_PRIORITY_DEFAULT, NULL, write_all_cb, bytes);
}

/**
 * melo_cover_cache_save:
 * @data: (transfer none) (array length=size): the cover data to save
 * @size: the size of @data
 * @type: the #MeloCoverType
 * @free_func: the function to call to release the data
 * @user_data: data to pass to @free_func
 *
 * This function calculates the hash of the cover and save the cover into the
 * internal cover cache. The cover can then be retrieved by passing the returned
 * hash to melo_cover_cache_get_path().
 *
 * Returns: the hash of the cover, should be freed with g_free().
 */
char *
melo_cover_cache_save (unsigned char *data, size_t size, MeloCoverType type,
    GDestroyNotify free_func, void *user_data)
{
  const char *ext = NULL;
  char *digest, *hash;
  GBytes *bytes;
  GFile *file;

  /* Select file extension */
  switch (type) {
  case MELO_COVER_TYPE_JPEG:
    ext = ".jpg";
    break;
  case MELO_COVER_TYPE_PNG:
    ext = ".png";
    break;
  case MELO_COVER_TYPE_UNKNOWN:
    ext = ".bin";
    break;
  default:
    MELO_LOGE ("invalid cover type %u", type);
    free_func (user_data);
    return NULL;
  }

  /* Calculate cover hash */
  digest = g_compute_checksum_for_data (G_CHECKSUM_MD5, data, size);

  /* Generate hash */
  hash = g_strconcat (digest, ext, NULL);
  g_free (digest);

  /* Create bytes */
  bytes = g_bytes_new_with_free_func (data, size, free_func, user_data);

  /* Cover not present in cache */
  file = g_file_new_build_filename (melo_cover_cache_path, hash, NULL);
  g_file_create_async (
      file, G_FILE_CREATE_NONE, G_PRIORITY_DEFAULT, NULL, create_cb, bytes);

  return hash;
}

/**
 * melo_cover_cache_save_gst_sample:
 * @sample: (transfer none): a #GstSample containing a cover
 *
 * This function save the cover contained in @sample. It takes the reference.
 *
 * Returns: the hash of the cover, should be freed with g_free().
 */
char *
melo_cover_cache_save_gst_sample (GstSample *sample)
{
  MeloCoverType type = MELO_COVER_TYPE_UNKNOWN;
  GstBuffer *buffer;
  GstMapInfo info;
  GstCaps *caps;

  /* No sample */
  if (!sample)
    return NULL;

  /* Get buffer */
  buffer = gst_sample_get_buffer (sample);
  if (!buffer) {
    gst_sample_unref (sample);
    return NULL;
  }

  /* Get image type */
  caps = gst_sample_get_caps (sample);
  if (caps) {
    GstStructure *structure;

    /* Get structure from caps */
    structure = gst_caps_get_structure (caps, 0);
    type = melo_cover_type_from_mime_type (gst_structure_get_name (structure));
  }

  /* Release sample */
  gst_buffer_ref (buffer);
  gst_sample_unref (sample);

  /* Map buffer */
  if (!gst_buffer_map (buffer, &info, GST_MAP_READ)) {
    gst_buffer_unref (buffer);
    return NULL;
  }

  return melo_cover_cache_save (
      info.data, info.size, type, (GDestroyNotify) gst_buffer_unref, buffer);
}

/**
 * melo_cover_extract_from_gst_tags_list:
 * @list: (transfer full): a #GstTagsList containing a cover
 *
 * This function find a valid cover in @list and returns a reference to it. The
 * gst_sample_unref() should be called after usage.
 *
 * Returns: a #GstSample containing a cover or %NULL.
 */
GstSample *
melo_cover_extract_from_gst_tags_list (const GstTagList *list)
{
  GstSample *sample = NULL;
  unsigned int count;

  /* No list */
  if (!list)
    return NULL;

  /* Select best image */
  count = gst_tag_list_get_tag_size (list, GST_TAG_IMAGE);
  while (count--) {
    const GstStructure *info;
    GstSample *s;

    /* Get next sample */
    if (!gst_tag_list_get_sample_index (list, GST_TAG_IMAGE, count, &s))
      continue;

    /* Get image info */
    info = gst_sample_get_info (s);
    if (info) {
      GstTagImageType type = GST_TAG_IMAGE_TYPE_NONE;

      /* Get image type */
      gst_structure_get_enum (
          info, "image-type", GST_TYPE_TAG_IMAGE_TYPE, &type);

      /* Select cover type or undefined if no cover */
      if (type == GST_TAG_IMAGE_TYPE_FRONT_COVER ||
          (type == GST_TAG_IMAGE_TYPE_UNDEFINED && sample == NULL)) {
        if (sample)
          gst_sample_unref (sample);
        sample = s;
        continue;
      }
    }

    /* Release sample */
    gst_sample_unref (s);
  }

  /* Sample found */
  if (sample)
    return sample;

  /* Get preview image if no image found */
  gst_tag_list_get_sample (list, GST_TAG_PREVIEW_IMAGE, &sample);

  return sample;
}

/**
 * melo_cover_cache_get_path:
 * @hash: a string returned by melo_cover_cache_save()
 *
 * This function is used to get the file path of a cover saved with
 * melo_cover_cache_save().
 *
 * Returns: the file path of the cover in cache, should be freed with g_free().
 */
char *
melo_cover_cache_get_path (const char *hash)
{
  return g_strconcat (melo_cover_cache_path, hash, NULL);
}

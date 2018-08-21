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

#include <libsoup/soup.h>

#include <gst/tag/tag.h>

#include "melo_tags.h"

/**
 * SECTION:melo_tags
 * @title: MeloTags
 * @short_description: Main structure to handle media tags
 *
 * #MeloTags provides a full set of function to store all tags related to a
 * media into a dedicated structure of type #MeloTags.
 *
 * #MeloTags comes with a powerful internal cache to handle cover image of
 * one or more medias. Each cover data associated to a #MeloTags with
 * melo_tags_set_cover_by_data() or cover URL associated to a #MeloTags with
 * melo_tags_set_cover_by_url() is automatically stored in an internal cache
 * which keeps image cover data until tags is unreferenced or end of program.
 * A unique ID is generated from the data or URL provided in order to prevent
 * storing duplicate covers.
 *
 * A cover can be set as:
 *  - 'non-persistent' with MELO_TAGS_COVER_PERSIST_NONE which means it will be
 *    freed during free of the last dependant MeloTags,
 *  - 'semi-persistent' with MELO_TAGS_COVER_PERSIST_EXIT which means it will be
 *    kept internally until end of the program,
 *  - 'persistent' with MELO_TAGS_COVER_PERSIST_DISK which means it will be
 *    stored on disk and will be available at next startup of the application.
 *
 * A cover can then be retrieved from a #MeloTags with melo_tags_get_cover() or
 * with its unique ID with melo_tags_get_cover_by_id().
 *
 * Many convert functions are also provided to fill a #MeloTags from a
 * #GstTagList with melo_tags_new_from_gst_tag_list() or to fill a #JsonObject
 * from a #MeloTags with melo_tags_add_to_json_object().
 */

/* Internal cover cache */
static GHashTable *melo_tags_cover_hash = NULL;
static GHashTable *melo_tags_cover_url_hash = NULL;
static SoupSession *melo_tags_cover_session = NULL;
static gchar *melo_tags_cover_path = NULL;

typedef struct _MeloTagsCover {
  GBytes *data;
  gint ref_count;
} MeloTagsCover;

typedef struct _MeloTagsCoverURL {
  gchar *url;
  gchar *id;
  guint64 timestamp;
  MeloTagsCoverPersist persist;
  gint ref_count;
} MeloTagsCoverURL;

static gchar *melo_tags_cover_ref (const gchar *id);

/**
 * melo_tags_new:
 *
 * Create a new #MeloTags instance.
 *
 * Returns: (transfer full): a pointer to a new #MeloTags reference. It must be
 * freed after usage with melo_tags_unref().
 */
MeloTags *
melo_tags_new (void)
{
  MeloTags *tags;

  /* Allocate new tags */
  tags = g_slice_new0 (MeloTags);
  if (!tags)
    return NULL;

  /* Set reference counter to 1 */
  tags->ref_count = 1;

  /* Set initial timestamp */
  melo_tags_update (tags);

  return tags;
}

/**
 * melo_tags_update:
 * @tags: the tags
 *
 * Update the internal timestamp to now, which allows to follow updates of the
 * #MeloTags data.
 */
void
melo_tags_update (MeloTags *tags)
{
  tags->timestamp = g_get_monotonic_time ();
}

/**
 * melo_tags_updated:
 * @tags: the tags
 * @timestamp: a timestamp (in us)
 *
 * Compare the provided timestamp in @timestamp with the internal timestamp. If
 * the internal value is older than @timestamp, the #MeloTags has not changed
 * since @timestamp.
 *
 * Returns: %TRUE if the #MeloTags has been updated since @timestamp, %FALSE
 * otherwise.
 */
gboolean
melo_tags_updated (MeloTags *tags, gint64 timestamp)
{
  return tags->timestamp > timestamp;
}

/**
 * melo_tags_copy:
 * @tags: the tags
 *
 * Do a deep copy of the #MeloTags provided by @tags.It will be completely
 * independent from the original.
 *
 * Returns: (transfer full): a new #MeloTags with the same data than @tags.
 * After use, call melo_tags_unref().
 */
MeloTags *
melo_tags_copy (MeloTags *tags)
{
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
  ntags->cover = melo_tags_cover_ref (tags->cover);

  return ntags;
}

/**
 * melo_tags_merge:
 * @tags: the tags
 * @ref_tags: the reference tags
 *
 * Copy @ref_tags data for all unset data in @tags.
 */
void
melo_tags_merge (MeloTags *tags, MeloTags *ref_tags)
{
  /* Copy values */
  if (!tags->title)
    tags->title = g_strdup (ref_tags->title);
  if (!tags->artist)
    tags->artist = g_strdup (ref_tags->artist);
  if (!tags->album)
    tags->album = g_strdup (ref_tags->album);
  if (!tags->genre)
    tags->genre = g_strdup (ref_tags->genre);
  if (!tags->date)
    tags->date = ref_tags->date;
  if (!tags->track)
    tags->track = ref_tags->track;
  if (!tags->tracks)
    tags->tracks = ref_tags->tracks;
  if (!tags->cover)
    tags->cover = melo_tags_cover_ref (ref_tags->cover);

  /* Update timestamp */
  melo_tags_update (tags);
}

/**
 * melo_tags_ref:
 * @tags: the tags
 *
 * Increment the reference counter of the #MeloTags.
 *
 * Returns: (transfer full): a pointer of the #MeloTags.
 */
MeloTags *
melo_tags_ref (MeloTags *tags)
{
  if (tags)
    g_atomic_int_inc (&tags->ref_count);
  return tags;
}

static MeloTagsCover *
melo_tags_cover_new (GBytes *data)
{
  MeloTagsCover *cover;

  /* Create new cover */
  cover = g_slice_new0 (MeloTagsCover);
  if (cover) {
    cover->data = g_bytes_ref (data);
    cover->ref_count = 1;
  }

   return cover;
}

static void
melo_tags_cover_free (MeloTagsCover *cover)
{
  g_bytes_unref (cover->data);
  g_slice_free (MeloTagsCover, cover);
}

static MeloTagsCoverURL *
melo_tags_cover_url_new (const gchar *url, MeloTagsCoverPersist persist)
{
  MeloTagsCoverURL *cover_url;

  /* Create new cover URL */
  cover_url = g_slice_new0 (MeloTagsCoverURL);
  if (cover_url) {
    cover_url->url = g_strdup (url);
    cover_url->timestamp = g_get_monotonic_time ();
    cover_url->persist = persist;
    cover_url->ref_count = 1;
  }

   return cover_url;
}

static void
melo_tags_cover_url_free (MeloTagsCoverURL *cover_url)
{
  g_free (cover_url->id);
  g_free (cover_url->url);
  g_slice_free (MeloTagsCoverURL, cover_url);
}

static gchar *
melo_tags_cover_gen_file_path (const gchar *id)
{
  if (!melo_tags_cover_path) {
    melo_tags_cover_path = g_strdup_printf ("%s/melo/cover",
                                            g_get_user_data_dir ());
    g_mkdir_with_parents (melo_tags_cover_path, 0700);
  }

  return g_strdup_printf ("%s/%s", melo_tags_cover_path, id);
}

static gchar *
melo_tags_cover_add_data (GBytes *data, MeloTagsCoverPersist persist)
{
  MeloTagsCover *cover;
  gchar *file, *id;

  /* Generate cover ID from md5 sum */
  id = g_compute_checksum_for_bytes (G_CHECKSUM_MD5, data);

  /* Generate file name on disk */
  file = melo_tags_cover_gen_file_path (id);

  /* Generate hash table if doesn't exist */
  if (!melo_tags_cover_hash)
    melo_tags_cover_hash = g_hash_table_new_full (g_str_hash, g_str_equal,
                                         g_free,
                                         (GDestroyNotify) melo_tags_cover_free);

  /* Check disk persistence */
  if (g_file_test (file, G_FILE_TEST_EXISTS)) {
    /* Already on disk */
    g_free (file);
    return id;
  } else if (persist == MELO_TAGS_COVER_PERSIST_DISK) {
    /* Save image data to disk */
    g_file_set_contents (file, g_bytes_get_data (data, NULL),
                         g_bytes_get_size (data), NULL);
    g_free (file);

    /* Remove any copy from hash table */
    g_hash_table_remove (melo_tags_cover_hash, id);

    return id;
  }

  /* Find in cover hash table */
  cover = g_hash_table_lookup (melo_tags_cover_hash, id);
  if (cover) {
    /* Cover is already handled internally */
    g_atomic_int_inc (&cover->ref_count);
    goto end;
  }

  /* Create cover */
  cover = melo_tags_cover_new (data);
  if (!cover)
    goto failed;

  /* Add to hash table (it takes ownership of the ID) */
  if (!g_hash_table_insert (melo_tags_cover_hash, g_strdup (id), cover))
    goto failed;

end:
  /* Force persistence until end of execution */
  if (persist == MELO_TAGS_COVER_PERSIST_EXIT)
    g_atomic_int_inc (&cover->ref_count);

  return id;

failed:
  g_free (id);
  return NULL;
}

static gchar *
melo_tags_cover_add_url (const gchar *url, MeloTagsCoverPersist persist)
{
  MeloTagsCoverURL *cover_url;
  gchar *id, *url_id;

  /* Generate cover ID from md5 sum */
  id = g_compute_checksum_for_string (G_CHECKSUM_MD5, url, -1);
  url_id = g_strdup_printf ("@%s", id);

  /* Generate URL hash table if doesn't exist */
  if (!melo_tags_cover_url_hash)
    melo_tags_cover_url_hash = g_hash_table_new_full (g_str_hash, g_str_equal,
                                     g_free,
                                     (GDestroyNotify) melo_tags_cover_url_free);

  /* Find in cover URL hash table */
  cover_url = g_hash_table_lookup (melo_tags_cover_url_hash, id);
  if (cover_url) {
    /* Persistence is conservative */
    if (persist > cover_url->persist)
      cover_url->persist = persist;

    /* Cover URL is already handled internally */
    g_atomic_int_inc (&cover_url->ref_count);
    g_free (id);
    goto end;
  }

  /* Create cover URL */
  cover_url = melo_tags_cover_url_new (url, persist);
  if (!cover_url)
    goto failed;

  /* Add to hash table (it takes ownership of the ID) */
  if (!g_hash_table_insert (melo_tags_cover_url_hash, id, cover_url))
    goto failed;

end:
  /* Force persistence until end of execution */
  if (persist == MELO_TAGS_COVER_PERSIST_EXIT)
    g_atomic_int_inc (&cover_url->ref_count);

  return url_id;

failed:
  g_free (id);
  g_free (url_id);
  return NULL;
}

/**
 * melo_tags_set_cover_by_data:
 * @tags: the tags
 * @cover: the image data
 * @persist: cover data persistence option
 *
 * Add a cover to a #MeloTags with a #GBytes containing the image data.
 * A unique ID is generated from the image data provided and stored in an
 * internal cache for fast access. The cover data can then be retrieved with
 * melo_tags_get_cover() or melo_tags_get_cover_by_id() until end of the
 * program.
 * If @persist is set to MELO_TAGS_COVER_PERSIST_DISK, the cover data is saved
 * on the disk instead of memory.
 *
 * Returns: a string containing the ID of the cover provided or %NULL if an
 * error occurred. The string stays valid while the #MeloTags @tags exists.
 */
const gchar *
melo_tags_set_cover_by_data (MeloTags *tags, GBytes *cover,
                             MeloTagsCoverPersist persist)
{
  gchar *id;

  /* Add cover to internal cache */
  id = melo_tags_cover_add_data (cover, persist);
  if (id) {
    g_free (tags->cover);
    tags->cover = id;
  }

  return id;
}

/**
 * melo_tags_set_cover_by_url:
 * @tags: the tags
 * @url: the image URL
 * @persist: cover data persistence option
 *
 * Add a cover to a #MeloTags with an URL.
 * A unique ID is generated from the URL provided and stored in an internal
 * cache for fast access. The cover data can then be retrieved with
 * melo_tags_get_cover() or melo_tags_get_cover_by_id() until end of the
 * program.
 * For different URL which point to the same image, only one copy of the data
 * is available into the internal cache, which means you don't care to have
 * multiple URL with the same image data at end.
 * If @persist is set to MELO_TAGS_COVER_PERSIST_DISK, the cover data is saved
 * on the disk instead of memory.
 *
 * Returns: a string containing the ID of the cover provided or %NULL if an
 * error occurred. The string stays valid while the #MeloTags @tags exists.
 */
const gchar *
melo_tags_set_cover_by_url (MeloTags *tags, const gchar *url,
                            MeloTagsCoverPersist persist)
{
  gchar *id;

  /* Add cover URL to internal cache */
  id = melo_tags_cover_add_url (url, persist);
  if (id) {
    g_free (tags->cover);
    tags->cover = id;
  }

  return id;
}

static gchar *
melo_tags_cover_ref (const gchar *id)
{
  MeloTagsCover *cover;

  /* No cover attached */
  if (!id)
    return NULL;

  /* Cover coming from an URL */
  if (*id == '@') {
    MeloTagsCoverURL *cover_url;

    /* No cover URL hash table */
    if (!melo_tags_cover_url_hash)
      return NULL;

    /* Find in cover URL hash table */
    cover_url = g_hash_table_lookup (melo_tags_cover_url_hash, ++id);
    if (!cover_url)
      return NULL;

    /* Add a reference the cover URL data */
    g_atomic_int_inc (&cover_url->ref_count);

    return g_strdup_printf ("@%s", id);
  }

  /* No cover hash table */
  if (!melo_tags_cover_hash)
    return NULL;

  /* Find cover in hash table */
  cover = g_hash_table_lookup (melo_tags_cover_hash, id);
  if (!cover) {
    gchar *file;

    /* Generate file name on disk */
    file = melo_tags_cover_gen_file_path (id);

    /* Check file existence */
    if (!g_file_test (file, G_FILE_TEST_EXISTS))
      id = NULL;
    g_free (file);

    return g_strdup (id);
  }

  /* Add a reference the cover data */
  g_atomic_int_inc (&cover->ref_count);

  return g_strdup (id);
}

static void
melo_tags_cover_unref (const gchar *id)
{
  MeloTagsCover *cover;

  /* No cover attached */
  if (!id)
    return;

  /* Cover coming from an URL */
  if (*id == '@') {
    MeloTagsCoverURL *cover_url;

    /* No cover URL hash table */
    if (!melo_tags_cover_url_hash)
      return;

    /* Find in cover URL hash table */
    cover_url = g_hash_table_lookup (melo_tags_cover_url_hash, ++id);
    if (!cover_url)
      return;

    /* Unref the cover URL data */
    if (g_atomic_int_dec_and_test (&cover_url->ref_count)) {
      /* Unref the cover data */
      melo_tags_cover_unref (cover_url->id);

      /* Remove cover from hash table */
      g_hash_table_remove (melo_tags_cover_url_hash, id);
    }

    return;
  }

  /* No cover hash table */
  if (!melo_tags_cover_hash)
    return;

  /* Find cover in hash table */
  cover = g_hash_table_lookup (melo_tags_cover_hash, id);
  if (!cover)
    return;

  /* Unref the cover data */
  if (g_atomic_int_dec_and_test (&cover->ref_count)) {
    /* Remove cover from hash table */
    g_hash_table_remove (melo_tags_cover_hash, id);
  }
}

/**
 * melo_tags_get_cover:
 * @tags: the tags
 *
 * Provide a #GBytes containing the image data of the cover.
 *
 * Returns: (transfer full): a #GBytes containing the image data of the cover or
 * %NULL if no cover has been set. After usage, call g_bytes_unref().
 */
GBytes *
melo_tags_get_cover (MeloTags *tags)
{
  if (!tags)
    return NULL;

  return melo_tags_get_cover_by_id (tags->cover);
}

/**
 * melo_tags_get_cover_by_id:
 * @id: the cover ID
 *
 * Provide a #GBytes containing the image data of the cover identified by @id.
 *
 * Returns: (transfer full): a #GBytes containing the image data of the cover or
 * %NULL if no cover has been set. After usage, call g_bytes_unref().
 */
GBytes *
melo_tags_get_cover_by_id (const gchar *id)
{
  MeloTagsCover *cover = NULL;

  /* No ID provided or no cover hash table */
  if (!id)
    return NULL;

  /* Cover coming from an URL */
  if (*id == '@') {
    MeloTagsCoverURL *cover_url;

    /* No cover URL hash table */
    if (!melo_tags_cover_url_hash)
      return NULL;

    /* Find in cover URL hash table */
    cover_url = g_hash_table_lookup (melo_tags_cover_url_hash, ++id);
    if (!cover_url)
      return NULL;

    /* Update access time */
    cover_url->timestamp = g_get_monotonic_time ();

    /* Cover has not been downloaded yet */
    if (!cover_url->id) {
      SoupMessage *msg;
      GBytes *data = NULL;

      /* Create a new Soup session */
      if (!melo_tags_cover_session)
        melo_tags_cover_session = soup_session_new_with_options (
                                                SOUP_SESSION_USER_AGENT, "Melo",
                                                NULL);

      /* Prepare HTTP request */
      msg = soup_message_new ("GET", cover_url->url);
      if (msg) {
        /* Download cover data */
        if (soup_session_send_message (melo_tags_cover_session, msg) == 200)
          g_object_get (msg, "response-body-data", &data, NULL);

        /* Free message */
        g_object_unref (msg);
      }

      /* Add data to internal cache */
      if (data)
        cover_url->id = melo_tags_cover_add_data (data, cover_url->persist);

      return data;
    }

    return melo_tags_get_cover_by_id (cover_url->id);
  }

  /* Find in cover hash table */
  if (melo_tags_cover_hash)
    cover = g_hash_table_lookup (melo_tags_cover_hash, id);
  if (!cover) {
    GBytes *data = NULL;
    gchar *path;

    /* Generate file name on disk */
    path = melo_tags_cover_gen_file_path (id);

    /* Load image data from disk */
    if (g_file_test (path, G_FILE_TEST_EXISTS)) {
      GMappedFile *file;

      /* Map file */
      file = g_mapped_file_new (path, FALSE, NULL);
      if (file) {
        /* Generate GBytes */
        data = g_mapped_file_get_bytes (file);
        g_mapped_file_unref (file);
      }
    }
    g_free (path);


    return data;
  }

  return g_bytes_ref (cover->data);
}

/**
 * melo_tags_flush_cover_cache:
 *
 * Flush the internal image cover cache.
 * It must be called only by the main context of the application, at end, after
 * everything has been freed.
 */
void
melo_tags_flush_cover_cache (void)
{
  /* Destroy cover URL hash table */
  if (melo_tags_cover_url_hash) {
    g_hash_table_destroy (melo_tags_cover_url_hash);
    melo_tags_cover_url_hash = NULL;
  }

  /* Destroy cover hash table */
  if (melo_tags_cover_hash) {
    g_hash_table_destroy (melo_tags_cover_hash);
    melo_tags_cover_hash = NULL;
  }

  /* Free cover path on disk */
  g_free (melo_tags_cover_path);
  melo_tags_cover_path = NULL;

  /* Free Soup session */
  if (melo_tags_cover_session) {
    g_object_unref (melo_tags_cover_session);
    melo_tags_cover_session = NULL;
  }
}

/**
 * melo_tags_new_from_gst_tag_list:
 * @tlist: a #GstTagList containing media tags
 * @fields: the tags to extract from #GstTagList
 * @persist: cover data persistence option
 *
 * Generate a #MeloTags from a #GstTagList, with specified @fields tags fields.
 *
 * Returns: (transfer full): a new #MeloTags containing all extracted media tags
 * from @tlist or %NULL if an error occurred. After use, call melo_tags_unref().
 */
MeloTags *
melo_tags_new_from_gst_tag_list (const GstTagList *tlist, MeloTagsFields fields,
                                 MeloTagsCoverPersist persist)
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
        GBytes *cover;
        gpointer data;
        gsize size, dsize;

        /* Get buffer and extract data */
        buffer = gst_sample_get_buffer (final_sample);
        size = gst_buffer_get_size (buffer);
        gst_buffer_extract_dup (buffer, 0, size, &data, &dsize);

        /* Set cover to tags */
        cover = g_bytes_new_take (data, dsize);
        if (cover) {
          melo_tags_set_cover_by_data (tags, cover, persist);
          g_bytes_unref (cover);
        }

        /* Free image */
        gst_sample_unref (final_sample);
    }
  }

  return tags;
}

/**
 * melo_tags_get_fields_from_json_array:
 * @array: a #JsonArray containing a JSON array
 *
 * Convert a #JsonArray containing many strings into a #MeloTagsFields.
 *
 * Returns: a #MeloTagsFields with all tags fields specified in @array.
 */
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
  }

  return fields;
}

/**
 * melo_tags_add_to_json_object:
 * @tags: the tags
 * @object: a #JsonObject to fill with #MeloTags data
 * @fields: the tags to add to the #JsonObject
 *
 * Fill the #JsonObject provided with many members containing all data requested
 * in @fields.
 */
void
melo_tags_add_to_json_object (MeloTags *tags, JsonObject *object,
                              MeloTagsFields fields)
{
  /* Nothing to do */
  if (!tags || fields == MELO_TAGS_FIELDS_NONE)
    return;

  /* Set timestamp in any case */
  json_object_set_int_member (object, "timestamp", tags->timestamp);

  /* Fill object */
  if (fields & MELO_TAGS_FIELDS_TITLE)
    json_object_set_string_member (object, "title", tags->title);
  if (fields & MELO_TAGS_FIELDS_ARTIST)
    json_object_set_string_member (object, "artist", tags->artist);
  if (fields & MELO_TAGS_FIELDS_ALBUM)
    json_object_set_string_member (object, "album", tags->album);
  if (fields & MELO_TAGS_FIELDS_GENRE)
    json_object_set_string_member (object, "genre", tags->genre);
  if (fields & MELO_TAGS_FIELDS_DATE)
    json_object_set_int_member (object, "date", tags->date);
  if (fields & MELO_TAGS_FIELDS_TRACK)
    json_object_set_int_member (object, "track", tags->track);
  if (fields & MELO_TAGS_FIELDS_TRACKS)
    json_object_set_int_member (object, "tracks", tags->tracks);
  if (fields & MELO_TAGS_FIELDS_COVER)
    json_object_set_string_member (object, "cover", tags->cover);
}

/**
 * melo_tags_to_json_object:
 * @tags: the tags
 * @fields: the tags to add to the #JsonObject
 *
 * Create a new #JsonObject with many members containing all data requested
 * in @fields.
 *
 * Returns: (transfer full): a new #JsonObject filled with #MeloTags data or
 * %NULL if an error occurred. After use, call json_object_unref().
 */
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

/**
 * melo_tags_unref:
 * @tags: the tags
 *
 * Decrement the reference counter of the #MeloTags. If it reaches zero, the
 * #MeloTags is freed.
 */
void
melo_tags_unref (MeloTags *tags)
{
  if (!tags)
    return;

  /* Decrement reference count */
  if (!g_atomic_int_dec_and_test (&tags->ref_count))
    return;

  /* Remove cover reference */
  melo_tags_cover_unref (tags->cover);

  /* Free tags */
  g_free (tags->title);
  g_free (tags->artist);
  g_free (tags->album);
  g_free (tags->genre);
  g_free (tags->cover);
  g_slice_free (MeloTags, tags);
}

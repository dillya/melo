/*
 * melo_tags.h: Media tags handler
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

#ifndef __MELO_TAGS_H__
#define __MELO_TAGS_H__

#include <glib.h>
#include <gst/gst.h>
#include <json-glib/json-glib.h>

typedef struct _MeloTags MeloTags;
typedef enum _MeloTagsFields MeloTagsFields;
typedef enum _MeloTagsCoverPersist MeloTagsCoverPersist;

/**
 * MeloTags:
 * @title: the media title
 * @artist: the artist of the media
 * @album: the album of the media
 * @genre: the media genre
 * @date: the date of creation
 * @track: the track number of the media (in the album)
 * @tracks: the number of tracks in the album
 * @cover: the cover ID of the media
 *
 * #MeloTags contains all details on a media such as its title, the related
 * artist and album.
 * To retrieve the image cover data from its ID, the melo_tags_get_cover() can
 * be used with a #MeloTags or melo_tags_get_cover_by_id() can be used with the
 * cover ID only, when the #MeloTags is not available.
 */
struct _MeloTags {
  gchar *title;
  gchar *artist;
  gchar *album;
  gchar *genre;
  gint date;
  guint track;
  guint tracks;
  gchar *cover;

  /*< private >*/
  gint64 timestamp;
  gint ref_count;
};

/**
 * MeloTagsFields:
 * @MELO_TAGS_FIELDS_NONE: get none of tags
 * @MELO_TAGS_FIELDS_TITLE: get the media title
 * @MELO_TAGS_FIELDS_ARTIST: get the artist of the media
 * @MELO_TAGS_FIELDS_ALBUM: get the album of the media
 * @MELO_TAGS_FIELDS_GENRE: get the media genre
 * @MELO_TAGS_FIELDS_DATE: get the date of creation
 * @MELO_TAGS_FIELDS_TRACK: get the track number of the media (in the album)
 * @MELO_TAGS_FIELDS_TRACKS: get the number of tracks in the album
 * @MELO_TAGS_FIELDS_COVER: get the cover ID
 * @MELO_TAGS_FIELDS_FULL: get all tags available
 *
 * #MeloTagsFields is a bit field used to provide which details on a media to
 * get from a #MeloTags.
 */
enum _MeloTagsFields {
  MELO_TAGS_FIELDS_NONE = 0,
  MELO_TAGS_FIELDS_TITLE = (1 << 0),
  MELO_TAGS_FIELDS_ARTIST = (1 << 1),
  MELO_TAGS_FIELDS_ALBUM = (1 << 2),
  MELO_TAGS_FIELDS_GENRE = (1 << 3),
  MELO_TAGS_FIELDS_DATE = (1 << 4),
  MELO_TAGS_FIELDS_TRACK = (1 << 5),
  MELO_TAGS_FIELDS_TRACKS = (1 << 6),
  MELO_TAGS_FIELDS_COVER = (1 << 7),

  MELO_TAGS_FIELDS_FULL = ~0,
};

/**
 * MeloTagsCoverPersist:
 * @MELO_TAGS_COVER_PERSIST_NONE: kept until last linked #MeloTags is
 *    unreferenced (good for a cover handled by a #MeloPlayer)
 * @MELO_TAGS_COVER_PERSIST_EXIT: kept until end of program (good for a cover
 *    handled by a #MeloBrowser or #MeloPlaylist)
 * @MELO_TAGS_COVER_PERSIST_DISK: copied on disk and kept forever (good for a
 *    library implementation)
 *
 * Many persistence of the cover data are available through
 * #MeloTagsCoverPersist to optimize the memory usage in any use case. Prefer
 * MELO_TAGS_COVER_PERSIST_NONE for a #MeloPlayer which needs to keep the
 * cover only during media playing, MELO_TAGS_COVER_PERSIST_EXIT for a
 * #MeloBrowser which set covers by URL (since a garbage collector optimize
 * memory used by image data, by discarding them is there are not used and
 * comes from an URL) and MELO_TAGS_COVER_PERSIST_DISK for a
 * #MeloBrowser which set covers by data.
 */
enum _MeloTagsCoverPersist {
  MELO_TAGS_COVER_PERSIST_NONE = 0,
  MELO_TAGS_COVER_PERSIST_EXIT,
  MELO_TAGS_COVER_PERSIST_DISK,

  /*< private >*/
  MELO_TAGS_COVER_PERSIST_COUNT
};

MeloTags *melo_tags_new (void);
void melo_tags_update (MeloTags *tags);
gboolean melo_tags_updated (MeloTags *tags, gint64 timestamp);
MeloTags *melo_tags_copy (MeloTags *tags);
void melo_tags_merge (MeloTags *tags, MeloTags *ref_tags);
MeloTags *melo_tags_ref (MeloTags *tags);
void melo_tags_unref (MeloTags *tags);

/* Set an image cover to MeloTags */
const gchar *melo_tags_set_cover_by_data (MeloTags *tags, GBytes *cover,
                                          MeloTagsCoverPersist persist);
const gchar *melo_tags_set_cover_by_url (MeloTags *tags, const gchar *url,
                                         MeloTagsCoverPersist persist);

/* Get an image cover */
GBytes *melo_tags_get_cover (MeloTags *tags);
GBytes *melo_tags_get_cover_by_id (const gchar *id);

/* Flush image cover cache (to call at end of program) */
void melo_tags_flush_cover_cache (void);

/* Gstreamer helper */
MeloTags *melo_tags_new_from_gst_tag_list (const GstTagList *tlist,
                                           MeloTagsFields fields,
                                           MeloTagsCoverPersist persist);

/* JSON-RPC helper */
MeloTagsFields melo_tags_get_fields_from_json_array (JsonArray *array);
void melo_tags_add_to_json_object (MeloTags *tags, JsonObject *object,
                                   MeloTagsFields fields);
JsonObject *melo_tags_to_json_object (MeloTags *tags, MeloTagsFields fields);

#endif /* __MELO_TAGS_H__ */

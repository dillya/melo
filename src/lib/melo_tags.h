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
typedef struct _MeloTagsPrivate MeloTagsPrivate;
typedef enum _MeloTagsFields MeloTagsFields;

struct _MeloTags {
  gchar *title;
  gchar *artist;
  gchar *album;
  gchar *genre;
  gint date;
  guint track;
  guint tracks;

  /*< private >*/
  MeloTagsPrivate *priv;
};

enum _MeloTagsFields {
  MELO_TAGS_FIELDS_NONE = 0,
  MELO_TAGS_FIELDS_TITLE = 1,
  MELO_TAGS_FIELDS_ARTIST = 2,
  MELO_TAGS_FIELDS_ALBUM = 4,
  MELO_TAGS_FIELDS_GENRE = 8,
  MELO_TAGS_FIELDS_DATE = 16,
  MELO_TAGS_FIELDS_TRACK = 32,
  MELO_TAGS_FIELDS_TRACKS = 64,
  MELO_TAGS_FIELDS_COVER = 128,
  MELO_TAGS_FIELDS_COVER_URL = 256,

  MELO_TAGS_FIELDS_FULL = ~0,
};

MeloTags *melo_tags_new (void);
void melo_tags_update (MeloTags *tags);
gboolean melo_tags_updated (MeloTags *tags, gint64 timestamp);
MeloTags *melo_tags_copy (MeloTags *tags);
void melo_tags_merge (MeloTags *tags, MeloTags *old_tags);
MeloTags *melo_tags_ref (MeloTags *tags);
void melo_tags_unref (MeloTags *tags);

/* Cover access */
void melo_tags_set_cover (MeloTags *tags, GBytes *cover, const gchar *type);
void melo_tags_take_cover (MeloTags *tags, GBytes *cover, const gchar *type);
gboolean melo_tags_has_cover (MeloTags *tags);
GBytes *melo_tags_get_cover (MeloTags *tags, gchar **type);
gboolean melo_tags_has_cover_type (MeloTags *tags);
gchar *melo_tags_get_cover_type (MeloTags *tags);

/* Cover URL for HTTP access */
gboolean melo_tags_set_cover_url (MeloTags *tags, GObject *obj,
                                  const gchar *path, const gchar *type);
void melo_tags_copy_cover_url (MeloTags *tags, const gchar *url,
                               const gchar *type);
gboolean melo_tags_has_cover_url (MeloTags *tags);
gchar *melo_tags_get_cover_url (MeloTags *tags);
void melo_tags_set_cover_url_base (const gchar *base);
gboolean melo_tags_get_cover_from_url (const gchar *url, GBytes **data,
                                       gchar **type);

/* Gstreamer helper */
MeloTags *melo_tags_new_from_gst_tag_list (const GstTagList *tlist,
                                           MeloTagsFields fields);

/* JSON-RPC helper */
MeloTagsFields melo_tags_get_fields_from_json_array (JsonArray *array);
void melo_tags_add_to_json_object (MeloTags *tags, JsonObject *object,
                                   MeloTagsFields fields);
JsonObject *melo_tags_to_json_object (MeloTags *tags, MeloTagsFields fields);

#endif /* __MELO_TAGS_H__ */

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

#ifndef _MELO_TAGS_H_
#define _MELO_TAGS_H_

#include <stdbool.h>

#include <gst/gst.h>

typedef struct _MeloTags MeloTags;
typedef enum _MeloTagsMergeFlag MeloTagsMergeFlag;

/**
 * MeloTagsMergeFlag:
 * @MELO_TAGS_MERGE_FLAG_NONE: no flags, all tags data will be merged
 * @MELO_TAGS_MERGE_FLAG_SKIP_TITLE: skip title, others are merged
 * @MELO_TAGS_MERGE_FLAG_SKIP_ARTIST: skip artist, others are merged
 * @MELO_TAGS_MERGE_FLAG_SKIP_ALBUM: skip album, others are merged
 * @MELO_TAGS_MERGE_FLAG_SKIP_GENRE: skip genre, others are merged
 * @MELO_TAGS_MERGE_FLAG_SKIP_TRACK: skip track, others are merged
 * @MELO_TAGS_MERGE_FLAG_SKIP_COVER: skip cover, others are merged
 * @MELO_TAGS_MERGE_FLAG_SKIP_BROWSER: skip browser, others are merged
 * @MELO_TAGS_MERGE_FLAG_SKIP_MEDIA_ID: skip media ID, others are merged
 * @MELO_TAGS_MERGE_FLAG_SKIP_ALL: skip all tags, none are merged
 *
 * This flags can be used to specify which tags data will be merged from old
 * tag to new tag during melo_tags_merge() call. If the flags is set to
 * MELO_TAGS_MERGE_FLAG_NONE, all tags data will be merge to new tags. To limit
 * the merge to only some tags data, a bit-field of the flags should be used. To
 * skip cover and title during merge, you should use the following combination:
 * (MELO_TAGS_MERGE_FLAG_SKIP_COVER | MELO_TAGS_MERGE_FLAG_SKIP_TITLE).
 */
enum _MeloTagsMergeFlag {
  MELO_TAGS_MERGE_FLAG_NONE = 0,
  MELO_TAGS_MERGE_FLAG_SKIP_TITLE = (1 << 0),
  MELO_TAGS_MERGE_FLAG_SKIP_ARTIST = (1 << 1),
  MELO_TAGS_MERGE_FLAG_SKIP_ALBUM = (1 << 2),
  MELO_TAGS_MERGE_FLAG_SKIP_GENRE = (1 << 3),
  MELO_TAGS_MERGE_FLAG_SKIP_TRACK = (1 << 4),
  MELO_TAGS_MERGE_FLAG_SKIP_COVER = (1 << 5),
  MELO_TAGS_MERGE_FLAG_SKIP_BROWSER = (1 << 6),
  MELO_TAGS_MERGE_FLAG_SKIP_MEDIA_ID = (1 << 7),
  MELO_TAGS_MERGE_FLAG_SKIP_ALL = ((1 << 6) - 1),
};

MeloTags *melo_tags_new (void);
MeloTags *melo_tags_new_from_taglist (GObject *obj, const GstTagList *tag_list);

MeloTags *melo_tags_ref (MeloTags *tags);
void melo_tags_unref (MeloTags *tags);

MeloTags *melo_tags_merge (MeloTags *new, MeloTags *old, unsigned int flags);

bool melo_tags_set_title (MeloTags *tags, const char *title);
bool melo_tags_set_artist (MeloTags *tags, const char *artist);
bool melo_tags_set_album (MeloTags *tags, const char *album);
bool melo_tags_set_genre (MeloTags *tags, const char *genre);
bool melo_tags_set_track (MeloTags *tags, unsigned int track);
bool melo_tags_set_cover (MeloTags *tags, GObject *obj, const char *cover);
bool melo_tags_set_browser (MeloTags *tags, const char *browser);
bool melo_tags_set_media_id (MeloTags *tags, const char *id);

const char *melo_tags_get_title (MeloTags *tags);
const char *melo_tags_get_artist (MeloTags *tags);
const char *melo_tags_get_album (MeloTags *tags);
const char *melo_tags_get_genre (MeloTags *tags);
unsigned int melo_tags_get_track (MeloTags *tags);
const char *melo_tags_get_cover (MeloTags *tags);
const char *melo_tags_get_browser (MeloTags *tags);
const char *melo_tags_get_media_id (MeloTags *tags);

char *melo_tags_gen_cover (GObject *obj, const char *cover);

#endif /* !_MELO_TAGS_H_ */

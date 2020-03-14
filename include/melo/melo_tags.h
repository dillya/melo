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

MeloTags *melo_tags_new (void);
MeloTags *melo_tags_new_from_taglist (GObject *obj, const GstTagList *tag_list);

MeloTags *melo_tags_ref (MeloTags *tags);
void melo_tags_unref (MeloTags *tags);

MeloTags *melo_tags_merge (MeloTags *new, MeloTags *old);

bool melo_tags_set_title (MeloTags *tags, const char *title);
bool melo_tags_set_artist (MeloTags *tags, const char *artist);
bool melo_tags_set_album (MeloTags *tags, const char *album);
bool melo_tags_set_genre (MeloTags *tags, const char *genre);
bool melo_tags_set_track (MeloTags *tags, unsigned int track);
bool melo_tags_set_cover (MeloTags *tags, GObject *obj, const char *cover);

const char *melo_tags_get_title (MeloTags *tags);
const char *melo_tags_get_artist (MeloTags *tags);
const char *melo_tags_get_album (MeloTags *tags);
const char *melo_tags_get_genre (MeloTags *tags);
unsigned int melo_tags_get_track (MeloTags *tags);
const char *melo_tags_get_cover (MeloTags *tags);

char *melo_tags_gen_cover (GObject *obj, const char *cover);

#endif /* !_MELO_TAGS_H_ */

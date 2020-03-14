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

#ifndef _MELO_COVER_H_
#define _MELO_COVER_H_

#include <gst/gst.h>

typedef enum _MeloCoverType MeloCoverType;

/**
 * MeloCoverType:
 * @MELO_COVER_TYPE_UNKNOWN: unknown image type
 * @MELO_COVER_TYPE_JPEG: JPEG image type
 * @MELO_COVER_TYPE_PNG: PNG image type
 *
 * The #MeloCoverType describes the type of the image.
 */
enum _MeloCoverType {
  MELO_COVER_TYPE_UNKNOWN = 0,
  MELO_COVER_TYPE_JPEG,
  MELO_COVER_TYPE_PNG,
};

void melo_cover_cache_init ();
void melo_cover_cache_deinit ();

MeloCoverType melo_cover_type_from_mime_type (const char *type);

char *melo_cover_cache_save (unsigned char *data, size_t size,
    MeloCoverType type, GDestroyNotify free_func, void *user_data);
char *melo_cover_cache_save_gst_sample (GstSample *sample);

GstSample *melo_cover_extract_from_gst_tags_list (const GstTagList *list);

char *melo_cover_cache_get_path (const char *hash);

#endif /* !_MELO_COVER_H_ */

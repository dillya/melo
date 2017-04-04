/*
 * melo_file_db.h: Database managment for File module
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

#ifndef __MELO_FILE_DB_H__
#define __MELO_FILE_DB_H__

#include <glib-object.h>

#include "melo_tags.h"

G_BEGIN_DECLS

#define MELO_TYPE_FILE_DB             (melo_file_db_get_type ())
#define MELO_FILE_DB(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), MELO_TYPE_FILE_DB, MeloFileDB))
#define MELO_IS_FILE_DB(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MELO_TYPE_FILE_DB))
#define MELO_FILE_DB_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), MELO_TYPE_FILE_DB, MeloFileDBClass))
#define MELO_IS_FILE_DB_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), MELO_TYPE_FILE_DB))
#define MELO_FILE_DB_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), MELO_TYPE_FILE_DB, MeloFileDBClass))

typedef struct _MeloFileDB MeloFileDB;
typedef struct _MeloFileDBClass MeloFileDBClass;
typedef struct _MeloFileDBPrivate MeloFileDBPrivate;

struct _MeloFileDB {
  GObject parent_instance;

  /*< private >*/
  MeloFileDBPrivate *priv;
};

struct _MeloFileDBClass {
  GObjectClass parent_class;
};

typedef enum {
  MELO_FILE_DB_FIELDS_END = 0,
  MELO_FILE_DB_FIELDS_PATH,
  MELO_FILE_DB_FIELDS_PATH_ID,
  MELO_FILE_DB_FIELDS_FILE,
  MELO_FILE_DB_FIELDS_FILE_ID,
  MELO_FILE_DB_FIELDS_TITLE,
  MELO_FILE_DB_FIELDS_ARTIST,
  MELO_FILE_DB_FIELDS_ARTIST_ID,
  MELO_FILE_DB_FIELDS_ALBUM,
  MELO_FILE_DB_FIELDS_ALBUM_ID,
  MELO_FILE_DB_FIELDS_GENRE,
  MELO_FILE_DB_FIELDS_GENRE_ID,
  MELO_FILE_DB_FIELDS_DATE,
  MELO_FILE_DB_FIELDS_TRACK,
  MELO_FILE_DB_FIELDS_TRACKS,

  /* Fields count */
  MELO_FILE_DB_FIELDS_COUNT
} MeloFileDBFields;

typedef enum {
  MELO_FILE_DB_SORT_NONE = 0,
  MELO_FILE_DB_SORT_FILE,
  MELO_FILE_DB_SORT_TITLE,
  MELO_FILE_DB_SORT_ARTIST,
  MELO_FILE_DB_SORT_ALBUM,
  MELO_FILE_DB_SORT_GENRE,
  MELO_FILE_DB_SORT_DATE,
  MELO_FILE_DB_SORT_TRACK,
  MELO_FILE_DB_SORT_TRACKS,

  /* Sort count */
  MELO_FILE_DB_SORT_COUNT
} MeloFileDBSort;
#define MELO_FILE_DB_SORT_AS_DESC(sort) \
  (sort + MELO_FILE_DB_SORT_COUNT)

GType melo_file_db_get_type (void);

MeloFileDB *melo_file_db_new (const gchar *file, const gchar *cover_path);
const gchar *melo_file_db_get_cover_path (MeloFileDB *db);

gboolean melo_file_db_get_path_id (MeloFileDB *db, const gchar *path,
                                   gboolean add, gint *path_id);

gboolean melo_file_db_add_tags (MeloFileDB *db, const gchar *path,
                                const gchar *filename, gint timestamp,
                                MeloTags *tags, gchar **cover_out_file);
gboolean melo_file_db_add_tags2 (MeloFileDB *db, gint path_id,
                                 const gchar *filename, gint timestamp,
                                 MeloTags *tags, gchar **cover_out_file);

/* Get specific entry */
MeloTags *melo_file_db_get_song (MeloFileDB *db, GObject *obj,
                                 MeloTagsFields tags_fields,
                                 MeloFileDBFields field_0, ...);
MeloTags *melo_file_db_get_artist (MeloFileDB *db, GObject *obj,
                                   MeloTagsFields tags_fields,
                                   MeloFileDBFields field_0, ...);
MeloTags *melo_file_db_get_album (MeloFileDB *db, GObject *obj,
                                  MeloTagsFields tags_fields,
                                  MeloFileDBFields field_0, ...);
MeloTags *melo_file_db_get_genre (MeloFileDB *db, GObject *obj,
                                  MeloTagsFields tags_fields,
                                  MeloFileDBFields field_0, ...);

typedef gboolean (*MeloFileDBGetList) (const gchar *path, const gchar *file,
                                       gint id, MeloTags *tags,
                                       gpointer user_data);

/* Get browser item list */
gboolean melo_file_db_get_file_list (MeloFileDB *db, GObject *obj,
                                     MeloFileDBGetList cb, gpointer user_data,
                                     gint offset, gint count,
                                     MeloFileDBSort sort,
                                     MeloTagsFields tags_fields,
                                     MeloFileDBFields field_0, ...);
gboolean melo_file_db_get_song_list (MeloFileDB *db, GObject *obj,
                                     MeloFileDBGetList cb, gpointer user_data,
                                     gint offset, gint count,
                                     MeloFileDBSort sort,
                                     MeloTagsFields tags_fields,
                                     MeloFileDBFields field_0, ...);
gboolean melo_file_db_get_artist_list (MeloFileDB *db, GObject *obj,
                                     MeloFileDBGetList cb, gpointer user_data,
                                     gint offset, gint count,
                                     MeloFileDBSort sort,
                                     MeloTagsFields tags_fields,
                                     MeloFileDBFields field_0, ...);
gboolean melo_file_db_get_album_list (MeloFileDB *db, GObject *obj,
                                     MeloFileDBGetList cb, gpointer user_data,
                                     gint offset, gint count,
                                     MeloFileDBSort sort,
                                     MeloTagsFields tags_fields,
                                     MeloFileDBFields field_0, ...);
gboolean melo_file_db_get_genre_list (MeloFileDB *db, GObject *obj,
                                     MeloFileDBGetList cb, gpointer user_data,
                                     gint offset, gint count,
                                     MeloFileDBSort sort,
                                     MeloTagsFields tags_fields,
                                     MeloFileDBFields field_0, ...);

G_END_DECLS

#endif /* __MELO_FILE_DB_H__ */

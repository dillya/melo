/*
 * melo_file_db.c: Database managment for File module
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

#include <sqlite3.h>

#include "melo_file_db.h"

#define MELO_FILE_DB_VERSION 1
#define MELO_FILE_DB_VERSION_STR "1"

/* Table creation */
#define MELO_FILE_DB_CREATE \
  "CREATE TABLE song (" \
  "        'title'         TEXT," \
  "        'artist_id'     INTEGER," \
  "        'album_id'      INTEGER," \
  "        'genre_id'      INTEGER," \
  "        'date'          INTEGER," \
  "        'track'         INTEGER," \
  "        'tracks'        INTEGER," \
  "        'file'          TEXT," \
  "        'path_id'       INTEGER," \
  "        'timestamp'     INTEGER" \
  ");" \
  "CREATE TABLE artist (" \
  "        'artist'        TEXT NOT NULL UNIQUE" \
  ");" \
  "CREATE TABLE album (" \
  "        'album'         TEXT NOT NULL UNIQUE" \
  ");" \
  "CREATE TABLE genre (" \
  "        'genre'         TEXT NOT NULL UNIQUE" \
  ");" \
  "CREATE TABLE path (" \
  "        'path'          TEXT NOT NULL UNIQUE" \
  ");" \
  "PRAGMA user_version = " MELO_FILE_DB_VERSION_STR ";"

/* Get database version */
#define MELO_FILE_DB_GET_VERSION "PRAGMA user_version;"

/* Clean database */
#define MELO_FILE_DB_CLEAN \
  "DROP TABLE IF EXISTS song;" \
  "DROP TABLE IF EXISTS artist;" \
  "DROP TABLE IF EXISTS album;" \
  "DROP TABLE IF EXISTS genre;" \
  "DROP TABLE IF EXISTS path;"

struct _MeloFileDBPrivate {
  GMutex mutex;
  sqlite3 *db;
};

G_DEFINE_TYPE_WITH_PRIVATE (MeloFileDB, melo_file_db, G_TYPE_OBJECT)

static gboolean melo_file_db_open (MeloFileDB *db, const gchar *file);
static void melo_file_db_close (MeloFileDB *db);

static void
melo_file_db_finalize (GObject *gobject)
{
  MeloFileDB *fdb = MELO_FILE_DB (gobject);
  MeloFileDBPrivate *priv = melo_file_db_get_instance_private (fdb);

  /* Close database file */
  melo_file_db_close (fdb);

  /* Clear mutex */
  g_mutex_clear (&priv->mutex);

  /* Chain up to the parent class */
  G_OBJECT_CLASS (melo_file_db_parent_class)->finalize (gobject);
}

static void
melo_file_db_class_init (MeloFileDBClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  /* Add custom finalize() function */
  object_class->finalize = melo_file_db_finalize;
}

static void
melo_file_db_init (MeloFileDB *self)
{
  MeloFileDBPrivate *priv = melo_file_db_get_instance_private (self);

  self->priv = priv;

  /* Init mutex */
  g_mutex_init (&priv->mutex);
}

MeloFileDB *
melo_file_db_new (const gchar *file)
{
  MeloFileDB *fdb;

  /* Create a new object */
  fdb = g_object_new (MELO_TYPE_FILE_DB, NULL);
  if (!fdb)
    return NULL;

  /* Open database file */
  if (!melo_file_db_open (fdb, file)) {
    g_object_unref (fdb);
    return NULL;
  }

  return fdb;
}

static gboolean
melo_file_db_get_int (MeloFileDBPrivate *priv, const gchar *sql, gint *value)
{
  sqlite3_stmt *req;
  gint count = 0;
  int ret;

  /* Prepare SQL request */
  ret = sqlite3_prepare_v2 (priv->db, sql, -1, &req, NULL);
  if (ret != SQLITE_OK)
    return FALSE;

  /* Get value from results */
  while ((ret = sqlite3_step (req)) == SQLITE_ROW) {
    *value = sqlite3_column_int (req, 0);
    count++;
  }

  /* Finalize request */
  sqlite3_finalize (req);

  return ret != SQLITE_DONE || !count ? FALSE : TRUE;
}

static gboolean
melo_file_db_open (MeloFileDB *db, const gchar *file)
{
  MeloFileDBPrivate *priv = db->priv;
  gint version;
  gchar *path;

  /* Create directory if necessary */
  path = g_path_get_dirname (file);
  if (g_mkdir_with_parents (path, 0700)) {
    g_free (path);
    return FALSE;
  }
  g_free (path);

  /* Lock database access */
  g_mutex_lock (&priv->mutex);

  /* Open database file */
  if (!priv->db) {
    /* Open sqlite database */
    if (sqlite3_open (file, &priv->db)) {
      g_mutex_unlock (&priv->mutex);
      return FALSE;
    }

    /* Get database version */
    melo_file_db_get_int (priv, MELO_FILE_DB_GET_VERSION, &version);

    /* Not initialized or old version */
    if (version < MELO_FILE_DB_VERSION) {
      /* Remove old database */
      sqlite3_exec (priv->db, MELO_FILE_DB_CLEAN, NULL, NULL, NULL);

      /* Initialize database */
      sqlite3_exec (priv->db, MELO_FILE_DB_CREATE, NULL, NULL, NULL);
    }
  }

  /* Unlock database access */
  g_mutex_unlock (&priv->mutex);

  return TRUE;
}

static void
melo_file_db_close (MeloFileDB *db)
{
  MeloFileDBPrivate *priv = db->priv;

  /* Lock database access */
  g_mutex_lock (&priv->mutex);

  /* Close databse */
  if (priv->db) {
    sqlite3_close (priv->db);
    priv->db = NULL;
  }

  /* Unlock database access */
  g_mutex_unlock (&priv->mutex);
}

gboolean
melo_file_db_get_path_id (MeloFileDB *db, const gchar *path, gboolean add,
                          gint *path_id)
{
  MeloFileDBPrivate *priv = db->priv;
  gboolean ret;
  char *sql;

  /* Lock database access */
  g_mutex_lock (&priv->mutex);

  /* Get ID for path */
  sql = sqlite3_mprintf ("SELECT rowid FROM path WHERE path = '%q'", path);
  ret = melo_file_db_get_int (priv, sql, path_id);
  sqlite3_free (sql);

  /* Path not found */
  if (!ret || !*path_id) {
    if (!add) {
      g_mutex_unlock (&priv->mutex);
      return FALSE;
    }

    /* Add new path */
    sql = sqlite3_mprintf ("INSERT INTO path (path) VALUES ('%q')", path);
    sqlite3_exec (priv->db, sql, NULL, NULL, NULL);
    *path_id = sqlite3_last_insert_rowid (priv->db);
    sqlite3_free (sql);
  }

  /* Unlock database access */
  g_mutex_unlock (&priv->mutex);

  return TRUE;
}

gboolean
melo_file_db_add_tags2 (MeloFileDB *db, gint path_id, const gchar *filename,
                        gint timestamp, MeloTags *tags)
{
  MeloFileDBPrivate *priv = db->priv;
  sqlite3_stmt *req;
  gint row_id = 0, ts = 0;
  gint artist_id;
  gint album_id;
  gint genre_id;
  gboolean ret;
  char *sql;

  /* Lock database access */
  g_mutex_lock (&priv->mutex);

  /* Find if file is already registered */
  sql = sqlite3_mprintf ("SELECT rowid,timestamp FROM song "
                         "WHERE path_id = %d AND file = '%q'",
                         path_id, filename);
  sqlite3_prepare_v2 (priv->db, sql, -1, &req, NULL);
  sqlite3_free (sql);
  while (sqlite3_step (req) == SQLITE_ROW) {
    row_id = sqlite3_column_int (req, 0);
    ts = sqlite3_column_int (req, 1);
  }
  sqlite3_finalize (req);

  /* File already registered and up to date */
  if (row_id && timestamp == ts) {
    g_mutex_unlock (&priv->mutex);
    return TRUE;
  }

  /* Find artist ID */
  if (tags && tags->artist) {
    sql = sqlite3_mprintf ("SELECT rowid FROM artist WHERE artist = '%q'",
                           tags->artist);
    ret = melo_file_db_get_int (priv, sql, &artist_id);
    sqlite3_free (sql);
    if (!ret || !artist_id) {
      /* Add new artist */
      sql = sqlite3_mprintf ("INSERT INTO artist (artist) VALUES ('%q')",
                             tags->artist);
      sqlite3_exec (priv->db, sql, NULL, NULL, NULL);
      artist_id = sqlite3_last_insert_rowid (priv->db);
      sqlite3_free (sql);
    }
  }

  /* Find album ID */
  if (tags && tags->album) {
    sql = sqlite3_mprintf ("SELECT rowid FROM album WHERE album = '%q'",
                           tags->album);
    ret = melo_file_db_get_int (priv, sql, &album_id);
    sqlite3_free (sql);
    if (!ret || !album_id) {
      /* Add new album */
      sql = sqlite3_mprintf ("INSERT INTO album (album) VALUES ('%q')",
                             tags->album);
      sqlite3_exec (priv->db, sql, NULL, NULL, NULL);
      album_id = sqlite3_last_insert_rowid (priv->db);
      sqlite3_free (sql);
    }
  }

  /* Find genre ID */
  if (tags && tags->genre) {
    sql = sqlite3_mprintf ("SELECT rowid FROM genre WHERE genre = '%q'",
                           tags->genre);
    ret = melo_file_db_get_int (priv, sql, &genre_id);
    sqlite3_free (sql);
    if (!ret || !genre_id) {
      /* Add new genre */
      sql = sqlite3_mprintf ("INSERT INTO genre (genre) VALUES ('%q')",
                             tags->genre);
      sqlite3_exec (priv->db, sql, NULL, NULL, NULL);
      genre_id = sqlite3_last_insert_rowid (priv->db);
      sqlite3_free (sql);
    }
  }

  /* Add song */
  if (!row_id) {
    sql = sqlite3_mprintf ("INSERT INTO song (title,artist_id,album_id,"
                           "genre_id,date,track,tracks,file,path_id,timestamp) "
                           "VALUES ('%q',%d,%d,%d,%d,%d,%d,'%q',%d,%d)",
                           tags ? tags->title : NULL, artist_id, album_id,
                           genre_id, tags ? tags->date : 0,
                           tags ? tags->track : 0, tags ? tags->tracks : 0,
                           filename, path_id, timestamp);
  } else {
    sql = sqlite3_mprintf ("UPDATE song SET title = '%q', artist_id = %d, "
                           "album_id = %d, genre_id = %d, date = %d, "
                           "track = %d, tracks = %d, timestamp = '%d' "
                           "WHERE rowid = %d",
                           tags ? tags->title : NULL, artist_id, album_id,
                           genre_id, tags ? tags->date : 0,
                           tags ? tags->track : 0, tags ? tags->tracks : 0,
                           timestamp, row_id);
  }
  sqlite3_exec (priv->db, sql, NULL, NULL, NULL);
  sqlite3_free (sql);

  /* Unlock database access */
  g_mutex_unlock (&priv->mutex);

  return TRUE;
}

gboolean
melo_file_db_add_tags (MeloFileDB *db, const gchar *path, const gchar *filename,
                       gint timestamp, MeloTags *tags)
{
  gint path_id;

  /* Get path ID (and add if not available) */
  if (!melo_file_db_get_path_id (db, path, TRUE, &path_id))
    return FALSE;

  /* Add tags to database */
  return melo_file_db_add_tags2 (db, path_id, filename, timestamp, tags);
}

#define MELO_FILE_DB_COLUMN_SIZE 255
#define MELO_FILE_DB_COND_COUNT 10

static gpointer
melo_file_db_find_vsong (MeloFileDB *db, gboolean one,
                         MeloTagsFields tags_fields, MeloFileDBFields field,
                         va_list args)
{
  MeloFileDBPrivate *priv = db->priv;
  sqlite3_stmt *req;
  MeloTags *tags;
  GList *list = NULL;
  gboolean join_artist = FALSE;
  gboolean join_album = FALSE;
  gboolean join_genre = FALSE;
  gboolean join_path = FALSE;
  gchar columns[MELO_FILE_DB_COLUMN_SIZE];
  gchar *conds[MELO_FILE_DB_COND_COUNT];
  gchar *conditions;
  gchar *sql;
  gint pos = 0, len = 0;

  /* Generate columns for request */
  if (tags_fields & MELO_TAGS_FIELDS_TITLE)
    len += g_snprintf (columns+len, MELO_FILE_DB_COLUMN_SIZE-len, "title,");
  if (tags_fields & MELO_TAGS_FIELDS_ARTIST) {
    len += g_snprintf (columns+len, MELO_FILE_DB_COLUMN_SIZE-len, "artist,");
    join_artist = TRUE;
  }
  if (tags_fields & MELO_TAGS_FIELDS_ALBUM) {
    len += g_snprintf (columns+len, MELO_FILE_DB_COLUMN_SIZE-len, "album,");
    join_album = TRUE;
  }
  if (tags_fields & MELO_TAGS_FIELDS_GENRE) {
    len += g_snprintf (columns+len, MELO_FILE_DB_COLUMN_SIZE-len, "genre,");
    join_genre = TRUE;
  }
  if (tags_fields & MELO_TAGS_FIELDS_DATE)
    len += g_snprintf (columns+len, MELO_FILE_DB_COLUMN_SIZE-len, "date,");
  if (tags_fields & MELO_TAGS_FIELDS_TRACK)
    len += g_snprintf (columns+len, MELO_FILE_DB_COLUMN_SIZE-len, "track,");
  if (tags_fields & MELO_TAGS_FIELDS_TRACKS)
    len += g_snprintf (columns+len, MELO_FILE_DB_COLUMN_SIZE-len, "tracks,");
  if (len)
    columns[len-1] = '\0';

  /* Generate SQL request */
  while (pos < MELO_FILE_DB_COND_COUNT && field != MELO_FILE_DB_FIELDS_END) {
    switch (field) {
      case MELO_FILE_DB_FIELDS_PATH:
        conds[pos++] = sqlite3_mprintf ("path = '%q'",
                                        va_arg (args, const gchar *));
        join_path = TRUE;
        break;
      case MELO_FILE_DB_FIELDS_FILE:
        conds[pos++] = sqlite3_mprintf ("file = '%q'",
                                        va_arg (args, const gchar *));
        break;
      case MELO_FILE_DB_FIELDS_TITLE:
        conds[pos++] = sqlite3_mprintf ("title = '%q'",
                                        va_arg (args, const gchar *));
        break;
      case MELO_FILE_DB_FIELDS_ARTIST:
        conds[pos++] = sqlite3_mprintf ("artist = '%q'",
                                        va_arg (args, const gchar *));
        join_artist = TRUE;
        break;
      case MELO_FILE_DB_FIELDS_ALBUM:
        conds[pos++] = sqlite3_mprintf ("album = '%q'",
                                        va_arg (args, const gchar *));
        join_album = TRUE;
        break;
      case MELO_FILE_DB_FIELDS_GENRE:
        conds[pos++] = sqlite3_mprintf ("genre = '%q'",
                                        va_arg (args, const gchar *));
        join_genre = TRUE;
        break;
      case MELO_FILE_DB_FIELDS_DATE:
        conds[pos++] = sqlite3_mprintf ("date = %d", va_arg (args, gint));
        break;
      case MELO_FILE_DB_FIELDS_TRACK:
        conds[pos++] = sqlite3_mprintf ("track = %d", va_arg (args, gint));
        break;
      case MELO_FILE_DB_FIELDS_TRACKS:
        conds[pos++] = sqlite3_mprintf ("tracks = %d", va_arg (args, gint));
        break;
      default:
        for (; pos; pos--)
          sqlite3_free (conds[pos-1]);
        goto error;
    }

    /* Get next field */
    field = va_arg (args, MeloFileDBFields);
  }
  conds[pos] = NULL;

  /* Finalize condition */
  conditions = g_strjoinv (" AND ", conds);
  for (; pos; pos--)
    sqlite3_free (conds[pos-1]);

  /* Generate SQL request */
  sql = sqlite3_mprintf ("SELECT %s FROM song %s %s %s %s WHERE %s",
        columns,
        join_artist ? "LEFT JOIN artist ON song.artist_id = artist.rowid" : "",
        join_album ? "LEFT JOIN album ON song.album_id = album.rowid" : "",
        join_genre ? "LEFT JOIN genre ON song.genre_id = genre.rowid" : "",
        join_path ? "LEFT JOIN path ON song.path_id = path.rowid" : "",
        conditions);
  g_free (conditions);

  /* Do SQL request */
  sqlite3_prepare_v2 (priv->db, sql, -1, &req, NULL);
  sqlite3_free (sql);

  while (sqlite3_step (req) == SQLITE_ROW) {
    MeloTags *tags;
    gint i = 0;

    /* Create a new MeloTags */
    tags = melo_tags_new ();
    if (!tags)
      break;

    /* Fill MeloTags */
    if (tags_fields & MELO_TAGS_FIELDS_TITLE)
      tags->title = g_strdup (sqlite3_column_text (req, i++));
    if (tags_fields & MELO_TAGS_FIELDS_ARTIST)
      tags->artist = g_strdup (sqlite3_column_text (req, i++));
    if (tags_fields & MELO_TAGS_FIELDS_ALBUM)
      tags->album = g_strdup (sqlite3_column_text (req, i++));
    if (tags_fields & MELO_TAGS_FIELDS_GENRE)
      tags->genre = g_strdup (sqlite3_column_text (req, i++));
    if (tags_fields & MELO_TAGS_FIELDS_DATE)
      tags->date = sqlite3_column_int (req, i++);
    if (tags_fields & MELO_TAGS_FIELDS_TRACK)
      tags->track = sqlite3_column_int (req, i++);
    if (tags_fields & MELO_TAGS_FIELDS_TRACKS)
      tags->tracks = sqlite3_column_int (req, i++);

    /* Get only one song from database */
    if (one) {
      sqlite3_finalize (req);
      return tags;
    }

    /* Add new MeloTags to list */
    list = g_list_prepend (list, tags);
  }

  /* Finalize SQL request */
  sqlite3_finalize (req);

  return list;

error:
  return NULL;
}

MeloTags *
melo_file_db_find_one_song (MeloFileDB *db, MeloTagsFields tags_fields,
                            MeloFileDBFields field_0, ...)
{
  MeloTags *tags;
  va_list args;

  /* Find tags */
  va_start (args, field_0);
  tags = (MeloTags *) melo_file_db_find_vsong (db, TRUE, tags_fields, field_0,
                                               args);
  va_end (args);

  return tags;
}

GList *
melo_file_db_find_song (MeloFileDB *db, MeloTagsFields tags_fields,
                        MeloFileDBFields field_0, ...)
{
  GList *list;
  va_list args;

  /* Find tags */
  va_start (args, field_0);
  list = (GList *) melo_file_db_find_vsong (db, FALSE, tags_fields, field_0,
                                            args);
  va_end (args);

  return list;
}

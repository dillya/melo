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

#define MELO_FILE_DB_VERSION 3
#define MELO_FILE_DB_VERSION_STR "3"

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
  "        'cover'         TEXT," \
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

typedef enum {
  MELO_FILE_DB_TYPE_FILE = 0,
  MELO_FILE_DB_TYPE_SONG,
  MELO_FILE_DB_TYPE_ARTIST,
  MELO_FILE_DB_TYPE_ALBUM,
  MELO_FILE_DB_TYPE_GENRE,
  MELO_FILE_DB_TYPE_DATE,
} MeloFileDBType;

static const gchar *melo_file_db_order_string[] = {
  [MELO_FILE_DB_SORT_FILE] = "file",
  [MELO_FILE_DB_SORT_TITLE] = "title",
  [MELO_FILE_DB_SORT_ARTIST] = "artist",
  [MELO_FILE_DB_SORT_ALBUM] = "album",
  [MELO_FILE_DB_SORT_GENRE] = "genre",
  [MELO_FILE_DB_SORT_DATE] = "date",
  [MELO_FILE_DB_SORT_TRACK] = "track",
  [MELO_FILE_DB_SORT_TRACKS] = "tracks",
};

struct _MeloFileDBPrivate {
  GMutex mutex;
  sqlite3 *db;
  gchar *cover_path;
};

G_DEFINE_TYPE_WITH_PRIVATE (MeloFileDB, melo_file_db, G_TYPE_OBJECT)

static gboolean melo_file_db_open (MeloFileDB *db, const gchar *file);
static void melo_file_db_close (MeloFileDB *db);

static void
melo_file_db_finalize (GObject *gobject)
{
  MeloFileDB *fdb = MELO_FILE_DB (gobject);
  MeloFileDBPrivate *priv = melo_file_db_get_instance_private (fdb);

  /* Free cover path */
  g_free (priv->cover_path);

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
melo_file_db_new (const gchar *file, const gchar *cover_path)
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

  /* Copy cover path and create it */
  fdb->priv->cover_path = g_strdup (cover_path);
  g_mkdir_with_parents (fdb->priv->cover_path, 0700);

  return fdb;
}

const gchar *
melo_file_db_get_cover_path (MeloFileDB *db)
{
  return db->priv->cover_path;
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
                        gint timestamp, MeloTags *tags, gchar **cover_out_file)
{
  const gchar *title, *artist, *album, *genre;
  MeloFileDBPrivate *priv = db->priv;
  sqlite3_stmt *req;
  guint track = 0, tracks = 0;
  gint row_id = 0, ts = 0;
  gint artist_id;
  gint album_id;
  gint genre_id;
  gint date = 0;
  gboolean ret;
  gchar *cover_file = NULL;
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

  /* Get strings from tags */
  title = tags && tags->title ? tags->title : "None";
  artist = tags && tags->artist ? tags->artist : "None";
  album = tags && tags->album ? tags->album : "None";
  genre = tags && tags->genre ? tags->genre : "None";

  /* Get values from tags */
  if (tags) {
    GBytes *cover;

    /* Get numbers from tags */
    date = tags->date;
    track = tags->track;
    tracks = tags->tracks;

    /* Get cover art */
    cover = melo_tags_get_cover (tags, NULL);
    if (cover) {
      gchar *cover_path;
      gchar *type;
      gchar *md5;

      /* Calculate md5 of cover art */
      md5 = g_compute_checksum_for_bytes (G_CHECKSUM_MD5, cover);

      /* Get cover type */
      type = melo_tags_get_cover_type (tags);

      /* Generate file name */
      cover_file = g_strdup_printf ("%s.%s", md5,
                                 g_strcmp0 (type, "image/png") ? "png" : "jpg");
      g_free (type);
      g_free (md5);

      /* Create file if not exist */
      cover_path = g_strdup_printf ("%s/%s", priv->cover_path, cover_file);
      if (!g_file_test (cover_path, G_FILE_TEST_EXISTS))
        g_file_set_contents (cover_path, g_bytes_get_data (cover, NULL),
                             g_bytes_get_size (cover), NULL);
      g_free (cover_path);
      g_bytes_unref (cover);
    }
  }

  /* Find artist ID */
  sql = sqlite3_mprintf ("SELECT rowid FROM artist WHERE artist = '%q'",
                         artist);
  ret = melo_file_db_get_int (priv, sql, &artist_id);
  sqlite3_free (sql);
  if (!ret || !artist_id) {
    /* Add new artist */
    sql = sqlite3_mprintf ("INSERT INTO artist (artist) VALUES ('%q')",
                           artist);
    sqlite3_exec (priv->db, sql, NULL, NULL, NULL);
    artist_id = sqlite3_last_insert_rowid (priv->db);
    sqlite3_free (sql);
  }

  /* Find album ID */
  sql = sqlite3_mprintf ("SELECT rowid FROM album WHERE album = '%q'", album);
  ret = melo_file_db_get_int (priv, sql, &album_id);
  sqlite3_free (sql);
  if (!ret || !album_id) {
    /* Add new album */
    sql = sqlite3_mprintf ("INSERT INTO album (album) VALUES ('%q')", album);
    sqlite3_exec (priv->db, sql, NULL, NULL, NULL);
    album_id = sqlite3_last_insert_rowid (priv->db);
    sqlite3_free (sql);
  }

  /* Find genre ID */
  sql = sqlite3_mprintf ("SELECT rowid FROM genre WHERE genre = '%q'", genre);
  ret = melo_file_db_get_int (priv, sql, &genre_id);
  sqlite3_free (sql);
  if (!ret || !genre_id) {
    /* Add new genre */
    sql = sqlite3_mprintf ("INSERT INTO genre (genre) VALUES ('%q')", genre);
    sqlite3_exec (priv->db, sql, NULL, NULL, NULL);
    genre_id = sqlite3_last_insert_rowid (priv->db);
    sqlite3_free (sql);
  }

  /* Add song */
  if (!row_id) {
    sql = sqlite3_mprintf ("INSERT INTO song (title,artist_id,album_id,"
                           "genre_id,date,track,tracks,cover,file,path_id,"
                           "timestamp) "
                           "VALUES (%Q,%d,%d,%d,%d,%d,%d,%Q,'%q',%d,%d)",
                           title, artist_id, album_id, genre_id, date, track,
                           tracks, cover_file, filename, path_id, timestamp);
  } else {
    sql = sqlite3_mprintf ("UPDATE song SET title = %Q, artist_id = %d, "
                           "album_id = %d, genre_id = %d, date = %d, "
                           "track = %d, tracks = %d, cover = %Q, "
                           "timestamp = '%d' "
                           "WHERE rowid = %d",
                           title, artist_id, album_id, genre_id, date, track,
                           tracks, cover_file, timestamp, row_id);
  }
  sqlite3_exec (priv->db, sql, NULL, NULL, NULL);
  sqlite3_free (sql);
  if (cover_out_file)
    *cover_out_file = cover_file;
  else
    g_free (cover_file);

  /* Unlock database access */
  g_mutex_unlock (&priv->mutex);

  return TRUE;
}

gboolean
melo_file_db_add_tags (MeloFileDB *db, const gchar *path, const gchar *filename,
                       gint timestamp, MeloTags *tags, gchar **cover_out_file)
{
  gint path_id;

  /* Get path ID (and add if not available) */
  if (!melo_file_db_get_path_id (db, path, TRUE, &path_id))
    return FALSE;

  /* Add tags to database */
  return melo_file_db_add_tags2 (db, path_id, filename, timestamp, tags,
                                 cover_out_file);
}

#define MELO_FILE_DB_COLUMN_SIZE 256
#define MELO_FILE_DB_COND_COUNT MELO_FILE_DB_FIELDS_COUNT

static gboolean
melo_file_db_vfind (MeloFileDB *db, MeloFileDBType type, GObject *obj,
                    MeloFileDBGetList cb, gpointer user_data, MeloTags **utags,
                    gint offset, gint count, MeloFileDBSort sort,
                    MeloTagsFields tags_fields, MeloFileDBFields field,
                    va_list args)
{
  const gchar *order = "", *order_col = "", *order_sort = "";
  MeloFileDBPrivate *priv = db->priv;
  sqlite3_stmt *req = NULL;
  MeloTags *tags;
  gboolean join_artist = FALSE;
  gboolean join_album = FALSE;
  gboolean join_genre = FALSE;
  gboolean join_path = FALSE;
  gchar columns[MELO_FILE_DB_COLUMN_SIZE];
  gchar *conds[MELO_FILE_DB_COND_COUNT];
  gchar *conditions;
  gchar *sql;
  gint pos = 0, len = 0;

  /* Handle exclusive tags cover */
  if (tags_fields & MELO_TAGS_FIELDS_COVER_EX &&
      tags_fields & MELO_TAGS_FIELDS_COVER_URL)
    tags_fields &= ~MELO_TAGS_FIELDS_COVER;

  /* Generate columns for request */
  len = g_snprintf (columns, MELO_FILE_DB_COLUMN_SIZE,
                    type <= MELO_FILE_DB_TYPE_SONG ? "song.rowid," : "rowid,");
  if (type == MELO_FILE_DB_TYPE_FILE) {
    len += g_snprintf (columns+len, MELO_FILE_DB_COLUMN_SIZE-len, "path,file,");
    join_path = TRUE;
  }
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
  if (tags_fields & MELO_TAGS_FIELDS_COVER_URL)
    len += g_snprintf (columns+len, MELO_FILE_DB_COLUMN_SIZE-len, "cover,");
  if (tags_fields & MELO_TAGS_FIELDS_COVER)
    len += g_snprintf (columns+len, MELO_FILE_DB_COLUMN_SIZE-len, "cover,");
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
      case MELO_FILE_DB_FIELDS_PATH_ID:
        conds[pos++] = sqlite3_mprintf ("path_id = %d",
                                        va_arg (args, gint));
        break;
      case MELO_FILE_DB_FIELDS_FILE:
        conds[pos++] = sqlite3_mprintf ("file = '%q'",
                                        va_arg (args, const gchar *));
        break;
      case MELO_FILE_DB_FIELDS_FILE_ID:
        conds[pos++] = sqlite3_mprintf ("song.rowid = '%d'",
                                        va_arg (args, gint));
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
      case MELO_FILE_DB_FIELDS_ARTIST_ID:
        conds[pos++] = sqlite3_mprintf ("artist_id = '%d'",
                                        va_arg (args, gint));
        break;
      case MELO_FILE_DB_FIELDS_ALBUM:
        conds[pos++] = sqlite3_mprintf ("album = '%q'",
                                        va_arg (args, const gchar *));
        join_album = TRUE;
        break;
      case MELO_FILE_DB_FIELDS_ALBUM_ID:
        conds[pos++] = sqlite3_mprintf ("album_id = '%d'",
                                        va_arg (args, gint));
        break;
      case MELO_FILE_DB_FIELDS_GENRE:
        conds[pos++] = sqlite3_mprintf ("genre = '%q'",
                                        va_arg (args, const gchar *));
        join_genre = TRUE;
        break;
      case MELO_FILE_DB_FIELDS_GENRE_ID:
        conds[pos++] = sqlite3_mprintf ("genre_id = '%d'",
                                        va_arg (args, gint));
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
  if (pos) {
    conditions = g_strjoinv (" AND ", conds);
    for (; pos; pos--)
      sqlite3_free (conds[pos-1]);
  } else
    conditions = g_strdup ("1");

  if (sort != MELO_FILE_DB_SORT_NONE && sort < MELO_FILE_DB_SORT_COUNT * 2) {
    /* Setup order clause */
    order = "ORDER BY ";
    if (sort > MELO_FILE_DB_SORT_COUNT) {
      order_col = melo_file_db_order_string[sort-MELO_FILE_DB_SORT_COUNT];
      order_sort = " COLLATE NOCASE DESC";
    } else {
      order_col = melo_file_db_order_string[sort];
      order_sort = " COLLATE NOCASE ASC";
    }
  }

  /* Generate SQL request */
  switch (type) {
    case MELO_FILE_DB_TYPE_SONG:
    case MELO_FILE_DB_TYPE_FILE:
      sql = sqlite3_mprintf ("SELECT %s FROM song %s %s %s %s WHERE %s %s%s%s "
         "LIMIT %d,%d", columns,
         join_artist ? "LEFT JOIN artist ON song.artist_id = artist.rowid" : "",
         join_album ? "LEFT JOIN album ON song.album_id = album.rowid" : "",
         join_genre ? "LEFT JOIN genre ON song.genre_id = genre.rowid" : "",
         join_path ? "LEFT JOIN path ON song.path_id = path.rowid" : "",
         conditions, order, order_col, order_sort, offset, count);
      break;
    case MELO_FILE_DB_TYPE_ARTIST:
      sql = sqlite3_mprintf ("SELECT %s FROM artist WHERE %s %s%s%s "
                             "LIMIT %d,%d",
                             columns, conditions, order, order_col, order_sort,
                             offset, count);
      break;
    case MELO_FILE_DB_TYPE_ALBUM:
      sql = sqlite3_mprintf ("SELECT %s FROM album WHERE %s %s%s%s LIMIT %d,%d",
                             columns, conditions, order, order_col, order_sort,
                             offset, count);
      break;
    case MELO_FILE_DB_TYPE_GENRE:
      sql = sqlite3_mprintf ("SELECT %s FROM genre WHERE %s %s%s%s LIMIT %d,%d",
                             columns, conditions, order, order_col, order_sort,
                             offset, count);
      break;
    default:
      sql = NULL;
  }
  g_free (conditions);

  /* Do SQL request */
  sqlite3_prepare_v2 (priv->db, sql, -1, &req, NULL);
  sqlite3_free (sql);

  while (sqlite3_step (req) == SQLITE_ROW) {
    const gchar *path, *file;
    MeloTags *tags;
    gint id, i = 0;

    /* Do not generate tags */
    if (!cb && (!utags || *utags))
      continue;

    /* Create a new MeloTags */
    tags = melo_tags_new ();
    if (!tags)
      goto error;;

    /* Fill MeloTags */
    id = sqlite3_column_int (req, i++);
    if (type == MELO_FILE_DB_TYPE_FILE) {
      path = sqlite3_column_text (req, i++);
      file = sqlite3_column_text (req, i++);
    }
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
    if (tags_fields & MELO_TAGS_FIELDS_COVER_URL)
      melo_tags_set_cover_url (tags, obj, sqlite3_column_text (req, i++), NULL);
    if (tags_fields & MELO_TAGS_FIELDS_COVER) {
      const gchar *filename;

      /* Get cover filename */
      filename = sqlite3_column_text (req, i++);
      if (filename) {
        GMappedFile *file;
        GBytes *cover;
        const gchar *type;
        gchar *path;

        /* Open cover file and read data */
        path = g_strdup_printf ("%s/%s", priv->cover_path, filename);
        if (g_file_test (path, G_FILE_TEST_EXISTS)) {
          file = g_mapped_file_new (path, FALSE, NULL);
          cover = g_mapped_file_get_bytes (file);
          g_mapped_file_unref (file);
        }
        g_free (path);

        /* Add cover type */
        type = NULL;

        /* Add cover to MeloTags */
        melo_tags_take_cover (tags, cover, type);
      }
    }

    /* Set utags */
    if (utags && !*utags)
      *utags = tags;

    /* Call callback */
    if (cb && !cb (path, file, id, tags, user_data))
      goto error;
  }

  /* Finalize SQL request */
  sqlite3_finalize (req);

  return TRUE;

error:
  if (req)
    sqlite3_finalize (req);
  return FALSE;
}

#define DEFINE_MELO_FILE_DB_GET(type, utype, filter) \
  MeloTags * \
  melo_file_db_get_##type (MeloFileDB *db, GObject *obj, \
                         MeloTagsFields tags_fields, \
                         MeloFileDBFields field_0, ...) \
  { \
    MeloTags *tags = NULL; \
    va_list args; \
   \
    /* Apply filter on tags */ \
    tags_fields &= filter; \
   \
    /* Get tags */ \
    va_start (args, field_0); \
    melo_file_db_vfind (db, MELO_FILE_DB_TYPE_##utype, obj, NULL, NULL, \
                        &tags, 0, 1, MELO_FILE_DB_SORT_NONE, tags_fields, \
                        field_0, args); \
    va_end (args); \
   \
    return tags; \
  } \
  \
  gboolean \
  melo_file_db_get_##type##_list (MeloFileDB *db, GObject *obj, \
                                  MeloFileDBGetList cb, gpointer user_data, \
                                  gint offset, gint count, \
                                  MeloFileDBSort sort, \
                                  MeloTagsFields tags_fields, \
                                  MeloFileDBFields field_0, ...) \
  { \
    gboolean ret; \
    va_list args; \
   \
    /* Apply filter on tags */ \
    tags_fields &= filter; \
   \
    /* Get list */ \
    va_start (args, field_0); \
    ret = melo_file_db_vfind (db, MELO_FILE_DB_TYPE_##utype, obj, cb, \
                              user_data, NULL, offset, count, sort, \
                              tags_fields, field_0, args); \
    va_end (args); \
   \
    return ret; \
  }

DEFINE_MELO_FILE_DB_GET (file, FILE, MELO_TAGS_FIELDS_FULL)
DEFINE_MELO_FILE_DB_GET (song, SONG, MELO_TAGS_FIELDS_FULL)
DEFINE_MELO_FILE_DB_GET (artist, ARTIST, MELO_TAGS_FIELDS_ARTIST)
DEFINE_MELO_FILE_DB_GET (album, ALBUM, MELO_TAGS_FIELDS_ALBUM)
DEFINE_MELO_FILE_DB_GET (genre, GENRE, MELO_TAGS_FIELDS_GENRE)

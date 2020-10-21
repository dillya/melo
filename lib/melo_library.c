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

#include <stdio.h>

#include <glib.h>
#include <sqlite3.h>

#define MELO_LOG_TAG "library"
#include "melo/melo_log.h"

#include "melo/melo_library.h"

/* Default size for SQL requests */
#define MELO_LIBRARY_SQL_COLS_SIZE 256
#define MELO_LIBRARY_SQL_CONDS_SIZE 1024

static const char *melo_library_sql =
    "PRAGMA user_version = 1;"
    ""
    "CREATE TABLE player ("
    "    player       TEXT NOT NULL UNIQUE"
    ");"
    ""
    "CREATE TABLE path ("
    "    path         TEXT UNIQUE"
    ");"
    ""
    "CREATE TABLE artist ("
    "    name         TEXT UNIQUE"
    ");"
    ""
    "CREATE TABLE album ("
    "    name         TEXT UNIQUE"
    ");"
    ""
    "CREATE TABLE genre ("
    "    name         TEXT UNIQUE"
    ");"
    ""
    "CREATE TABLE cover ("
    "    cover        TEXT UNIQUE"
    ");"
    ""
    "CREATE TABLE media ("
    "    player_id    INTEGER,"
    "    path_id      INTEGER,"
    "    media        TEXT NOT NULL,"
    "    name         TEXT,"
    ""
    "    title        TEXT,"
    "    artist_id    INTEGER,"
    "    album_id     INTEGER,"
    "    genre_id     INTEGER,"
    "    date         INTEGER,"
    "    track        INTEGER,"
    "    cover_id     INTEGER,"
    "    timestamp    INTEGER,"
    ""
    "    FOREIGN KEY (player_id) REFERENCES player (rowid),"
    "    FOREIGN KEY (path_id) REFERENCES path (rowid),"
    "    FOREIGN KEY (album_id) REFERENCES album (rowid),"
    "    FOREIGN KEY (artist_id) REFERENCES artist (rowid),"
    "    FOREIGN KEY (genre_id) REFERENCES genre (rowid),"
    "    FOREIGN KEY (cover_id) REFERENCES cover (rowid)"
    ");"
    ""
    "CREATE VIEW medias AS "
    "SELECT media.rowid        AS media_id,"
    "       media.player_id    AS player_id,"
    "       media.path_id      AS path_id,"
    ""
    "       media.artist_id    AS artist_id,"
    "       media.album_id     AS album_id,"
    "       media.genre_id     AS genre_id,"
    ""
    "       player.player      AS player,"
    "       path.path          AS path,"
    "       media.media        AS media,"
    "       media.name         AS name,"
    "       media.timestamp    AS timestamp,"
    ""
    "       media.title        AS title,"
    "       artist.name        AS artist,"
    "       album.name         AS album,"
    "       genre.name         AS genre,"
    "       media.date         AS date,"
    "       media.track        AS track,"
    "       cover.cover        AS cover"
    "  FROM media"
    "  LEFT OUTER JOIN path       ON media.path_id = path.rowid"
    "  LEFT OUTER JOIN player     ON media.player_id = player.rowid"
    "  LEFT OUTER JOIN artist     ON media.artist_id = artist.rowid"
    "  LEFT OUTER JOIN album      ON media.album_id = album.rowid"
    "  LEFT OUTER JOIN genre      ON media.genre_id = genre.rowid"
    "  LEFT OUTER JOIN cover      ON media.cover_id = cover.rowid;"
    ""
    "CREATE VIEW search AS "
    "SELECT media.rowid        AS media_id,"
    "       media.media        AS media,"
    "       media.name         AS name,"
    "       media.title        AS title,"
    "       artist.name        AS artist,"
    "       album.name         AS album,"
    "       genre.name         AS genre"
    "  FROM media"
    "  LEFT OUTER JOIN artist     ON media.artist_id = artist.rowid"
    "  LEFT OUTER JOIN album      ON media.album_id = album.rowid"
    "  LEFT OUTER JOIN genre      ON media.genre_id = genre.rowid"
    "  LEFT OUTER JOIN cover      ON media.cover_id = cover.rowid;"
    ""
    "CREATE VIRTUAL TABLE fts USING fts3 ("
    "    media,"
    "    name,"
    "    title,"
    "    artist,"
    "    album,"
    "    genre"
    ");"
    ""
    "CREATE TRIGGER media_after_insert AFTER INSERT ON media "
    "BEGIN"
    "    INSERT INTO fts ("
    "        docid,"
    "        media,"
    "        name,"
    "        title,"
    "        artist,"
    "        album,"
    "        genre"
    "    ) SELECT * FROM search WHERE media_id = new.rowid;"
    "END;"
    ""
    "CREATE TRIGGER media_after_update AFTER UPDATE ON media "
    "BEGIN"
    "    INSERT INTO fts ("
    "        docid,"
    "        media,"
    "        name,"
    "        title,"
    "        artist,"
    "        album,"
    "        genre"
    "    ) SELECT * FROM search WHERE media_id = new.rowid;"
    "END;"
    ""
    "CREATE TRIGGER media_before_update BEFORE UPDATE ON media "
    "BEGIN"
    "    DELETE FROM fts WHERE docid = old.rowid;"
    "END;"
    ""
    "CREATE TRIGGER media_before_delete BEFORE DELETE ON media "
    "BEGIN"
    "    DELETE FROM fts WHERE docid = old.rowid;"
    "END;"
    ""
    "INSERT INTO path VALUES (NULL);"
    "INSERT INTO cover VALUES (NULL);"
    "INSERT INTO artist VALUES (NULL);"
    "INSERT INTO album VALUES (NULL);"
    "INSERT INTO genre VALUES (NULL);";

static const char *melo_library_type_str[MELO_LIBRARY_FIELD_COUNT] = {
    [MELO_LIBRARY_FIELD_NONE] = "none",
    [MELO_LIBRARY_FIELD_PLAYER_ID] = "player_id",
    [MELO_LIBRARY_FIELD_PATH_ID] = "path_id",
    [MELO_LIBRARY_FIELD_MEDIA_ID] = "media_id",
    [MELO_LIBRARY_FIELD_ARTIST_ID] = "artist_id",
    [MELO_LIBRARY_FIELD_ALBUM_ID] = "album_id",
    [MELO_LIBRARY_FIELD_GENRE_ID] = "genre_id",
    [MELO_LIBRARY_FIELD_PLAYER] = "player",
    [MELO_LIBRARY_FIELD_PATH] = "path",
    [MELO_LIBRARY_FIELD_MEDIA] = "media",
    [MELO_LIBRARY_FIELD_NAME] = "name",
    [MELO_LIBRARY_FIELD_TIMESTAMP] = "timestamp",
    [MELO_LIBRARY_FIELD_TITLE] = "title",
    [MELO_LIBRARY_FIELD_ARTIST] = "artist",
    [MELO_LIBRARY_FIELD_ALBUM] = "album",
    [MELO_LIBRARY_FIELD_GENRE] = "genre",
    [MELO_LIBRARY_FIELD_DATE] = "date",
    [MELO_LIBRARY_FIELD_TRACK] = "track",
    [MELO_LIBRARY_FIELD_COVER] = "cover",
};

/* Library database handler */
static sqlite3 *melo_library_db;

static bool
melo_library_get_uint64 (const char *sql, uint64_t *value)
{
  sqlite3_stmt *req;
  int count = 0;
  int ret;

  /* Prepare SQL request */
  ret = sqlite3_prepare_v2 (melo_library_db, sql, -1, &req, NULL);
  if (ret != SQLITE_OK)
    return false;

  /* Get value from results */
  while ((ret = sqlite3_step (req)) == SQLITE_ROW) {
    *value = sqlite3_column_int64 (req, 0);
    count++;
  }

  /* Finalize request */
  sqlite3_finalize (req);

  return ret != SQLITE_DONE || !count ? false : true;
}

void
melo_library_init (void)
{
  char *path, *db_path;
  uint64_t version;

  /* Create library path */
  path = g_build_filename (g_get_user_data_dir (), "melo", "library", NULL);
  g_mkdir_with_parents (path, 0700);

  /* Create database filename */
  db_path = g_build_filename (path, "library.db", NULL);
  g_free (path);

  /* Open library database */
  if (sqlite3_open (db_path, &melo_library_db) != SQLITE_OK) {
    MELO_LOGW ("failed to open library datase %s: %s", db_path,
        sqlite3_errmsg (melo_library_db));
    sqlite3_close (melo_library_db);
    melo_library_db = NULL;
  }
  g_free (db_path);

  /* No library database */
  if (!melo_library_db)
    return;

  /* Get database version */
  if (!melo_library_get_uint64 ("PRAGMA user_version;", &version) || !version) {
    MELO_LOGI ("Initialize library database");

    /* Initialize database */
    if (sqlite3_exec (melo_library_db, melo_library_sql, NULL, NULL, NULL) !=
        SQLITE_OK)
      MELO_LOGE (
          "failed to init database: %s", sqlite3_errmsg (melo_library_db));
  }
}

void
melo_library_deinit (void)
{
  int ret;

  /* Library database is opened */
  if (melo_library_db) {
    /* Close library database */
    ret = sqlite3_close (melo_library_db);
    if (ret != SQLITE_OK)
      MELO_LOGW ("failed to close library database: %d", ret);
  }
  melo_library_db = NULL;
}

static unsigned int
melo_library_insert_and_get (
    const char *table, const char *field, const char *value, bool trailing)
{
  const char *trail = trailing ? "/" : "";
  uint64_t id = 1;
  char *sql;

  if (!value)
    return id;

  /* Create SQL insertion */
  sql =
      sqlite3_mprintf ("INSERT INTO %s VALUES ('%q%s');", table, value, trail);
  if (sql) {
    /* Insert new value */
    sqlite3_exec (melo_library_db, sql, NULL, NULL, NULL);
    sqlite3_free (sql);
  }

  /* Create SQL selection */
  sql = sqlite3_mprintf (
      "SELECT rowid FROM %s WHERE %s = '%q%s';", table, field, value, trail);
  if (sql) {
    /* Get row ID */
    melo_library_get_uint64 (sql, &id);
    sqlite3_free (sql);
  }

  return id;
}

/**
 * melo_library_get_player_id:
 * @player: the Melo player
 *
 * This function can be used to get the player ID in library database for the
 * Melo player provided in @player.
 *
 * If the player doesn't exist, it will be added to the library database.
 *
 * This ID can be used within #MELO_LIBRARY_FIELD_PLAYER_ID.
 *
 * Returns: the player ID in the library database, 1 otherwise.
 */
uint64_t
melo_library_get_player_id (const char *player)
{
  return melo_library_insert_and_get ("player", "player", player, false);
}

/**
 * melo_library_get_path_id:
 * @path: the dirname of a media path
 *
 * This function can be used to get the path ID in library database for the
 * provided @path. It must be the dirname of the media full path and it must be
 * an absolute path, including (file://, http://, ...). A trailing '/' is
 * automatically added if necessary.
 *
 * If the path doesn't exist, it will be added to the library database.
 *
 * This ID can be used within #MELO_LIBRARY_FIELD_PATH_ID.
 *
 * Returns: the path ID in the library database, 1 otherwise.
 */
uint64_t
melo_library_get_path_id (const char *path)
{
  bool trailing = false;
  size_t len;

  if (!path)
    return 1;

  /* Remove trailing '/' in path */
  len = strlen (path);
  if (path[len - 1] != '/')
    trailing = true;

  return melo_library_insert_and_get ("path", "path", path, trailing);
}

/**
 * melo_library_add_media:
 * @player: the Melo player to use (overridden by @player_id or @media_id)
 * @player_id: the player ID in library database
 * @path: the dirname of the media path (overridden by @path_id or @media_id)
 * @path_id: the path ID in library database
 * @media: the basename of the media path (overridden by @media_id)
 * @media_id: the media ID in library database
 * @update: a combination of MELO_LIBRARY_SELECT() values
 * @name: (nullable): the name to display
 * @tags: (nullable): the media tags
 * @timestamp: the timestamp of the media
 *
 * This function can be used to add a media into the library and then be able to
 * retrieve it later with melo_library_find().
 *
 * When the media already exist, it will be updated, otherwise, it will be
 * added. A media can be addressed by two ways:
 *  - directly with @media_id which contains its ID in database: it must be
 *    got from database with melo_library_find(),
 *  - by combining the player (@player or @player_id), the path dirname (@path
 *    @path_id) and the media basename (@media).
 * Without these parameters, the function will fail.
 *
 * The @player_id and @path_id values can be respectively retrieved with
 * melo_library_get_player_id() and melo_library_get_path_id().
 * A trailing '/' is automatically added to @path.
 *
 * The @update parameter is a combination of MELO_LIBRARY_SELECT() with '|'
 * operator to specify which fields should be updated when the media is not yet
 * present in database, otherwise, the parameter is skipped.
 *
 * Returns: %true if the media has been added successfully, %false otherwise.
 */
bool
melo_library_add_media (const char *player, uint64_t player_id,
    const char *path, uint64_t path_id, const char *media, uint64_t media_id,
    unsigned int update, const char *name, MeloTags *tags, uint64_t timestamp)
{
  uint64_t artist_id, album_id, genre_id, cover_id;
  bool ret = false;
  char *sql;

  /* No media ID: find with player, path and media */
  if (!media_id) {
    /* Get player ID and path ID */
    if (player)
      player_id = melo_library_get_player_id (player);
    if (path)
      path_id = melo_library_get_path_id (path);

    /* Find media with player ID, path ID and media */
    sql = sqlite3_mprintf ("SELECT rowid FROM media WHERE player_id = %llu AND "
                           "path_id = %llu AND media = %Q;",
        player_id, path_id, media);
    melo_library_get_uint64 (sql, &media_id);
    sqlite3_free (sql);
  }

  /* Get IDs from tags */
  artist_id = melo_library_insert_and_get (
      "artist", "name", melo_tags_get_artist (tags), false);
  album_id = melo_library_insert_and_get (
      "album", "name", melo_tags_get_album (tags), false);
  genre_id = melo_library_insert_and_get (
      "genre", "name", melo_tags_get_genre (tags), false);
  cover_id = melo_library_insert_and_get (
      "cover", "cover", melo_tags_get_cover (tags), false);

  /* Generate SQL request */
  if (media_id) {
    char temp[MELO_LIBRARY_SQL_CONDS_SIZE];
    char *optional;
    GString *opts;

    /* Create optional string */
    opts = g_string_new_len (NULL, MELO_LIBRARY_SQL_CONDS_SIZE);
    if (!opts)
      return false;

    /* Set fields to update */
    if (update & MELO_LIBRARY_SELECT (NAME)) {
      sqlite3_snprintf (sizeof (temp), temp, "media = %Q,", media);
      g_string_append (opts, temp);
    }
    if (update & MELO_LIBRARY_SELECT (TITLE)) {
      sqlite3_snprintf (
          sizeof (temp), temp, "title = %Q,", melo_tags_get_title (tags));
      g_string_append (opts, temp);
    }
    if (update & MELO_LIBRARY_SELECT (ARTIST))
      g_string_append_printf (
          opts, "artist_id = %llu,", (unsigned long long) artist_id);
    if (update & MELO_LIBRARY_SELECT (ALBUM))
      g_string_append_printf (
          opts, "album_id = %llu,", (unsigned long long) album_id);
    if (update & MELO_LIBRARY_SELECT (GENRE))
      g_string_append_printf (
          opts, "genre_id = %llu,", (unsigned long long) genre_id);
    if (update & MELO_LIBRARY_SELECT (TRACK))
      g_string_append_printf (opts, "track = %u,", melo_tags_get_track (tags));
    if (update & MELO_LIBRARY_SELECT (COVER))
      g_string_append_printf (
          opts, "cover_id = %llu,", (unsigned long long) cover_id);
    if (update & MELO_LIBRARY_SELECT (TIMESTAMP))
      g_string_append_printf (
          opts, "timestamp = %llu,", (unsigned long long) timestamp);

    /* Get optional string */
    optional = g_string_free (opts, FALSE);

    /* Generate SQL update */
    sql = sqlite3_mprintf ("UPDATE media SET %sdate = %u WHERE rowid = %llu;",
        optional, 0, media_id);
    g_free (optional);
  } else
    /* Generate SQL insert */
    sql = sqlite3_mprintf (
        "INSERT INTO media VALUES "
        "(%llu,%llu,%Q,%Q,%Q,%llu,%llu,%llu,%u,%u,%llu,%llu);",
        player_id, path_id, media, name, melo_tags_get_title (tags), artist_id,
        album_id, genre_id, 0, melo_tags_get_track (tags), cover_id, timestamp);
  if (!sql)
    return ret;

  /* Execute SQL request */
  if (sqlite3_exec (melo_library_db, sql, NULL, NULL, NULL) == SQLITE_OK)
    ret = true;
  sqlite3_free (sql);

  return ret;
}

static bool
melo_library_vfind (MeloLibraryType type, MeloLibraryCb cb, void *user_data,
    unsigned int select, size_t count, off_t offset,
    MeloLibraryField sort_field, bool sort_desc, bool match,
    MeloLibraryField field, va_list args)
{
  sqlite3_stmt *req;
  GString *conds, *matches;
  const char *table, *order, *order_col, *order_dir;
  char fields[MELO_LIBRARY_SQL_COLS_SIZE], *end;
  char *conditions;
  char *sql;
  int ret;

  /* Select table */
  switch (type) {
  case MELO_LIBRARY_TYPE_MEDIA:
    table = "medias";
    break;
  case MELO_LIBRARY_TYPE_ARTIST:
    table = "artist";
    break;
  case MELO_LIBRARY_TYPE_ALBUM:
    table = "album";
    break;
  case MELO_LIBRARY_TYPE_GENRE:
    table = "genre";
    break;
  default:
    MELO_LOGE ("invalid type: %d", type);
    return false;
  }

  /* No callback */
  if (!cb)
    return true;

  /* Select columns to query */
  end = fields;
  if (select & MELO_LIBRARY_SELECT (PLAYER))
    end = g_stpcpy (end, "player,");
  if (select & MELO_LIBRARY_SELECT (PATH))
    end = g_stpcpy (end, "path,");
  if (select & MELO_LIBRARY_SELECT (MEDIA))
    end = g_stpcpy (end, "media,");
  if (select & MELO_LIBRARY_SELECT (NAME))
    end = g_stpcpy (end, "name,");
  if (select & MELO_LIBRARY_SELECT (TIMESTAMP))
    end = g_stpcpy (end, "timestamp,");
  if (select & MELO_LIBRARY_SELECT (TITLE))
    end = g_stpcpy (end, "title,");
  if (select & MELO_LIBRARY_SELECT (ARTIST))
    end = g_stpcpy (end, "artist,");
  if (select & MELO_LIBRARY_SELECT (ALBUM))
    end = g_stpcpy (end, "album,");
  if (select & MELO_LIBRARY_SELECT (GENRE))
    end = g_stpcpy (end, "genre,");
  if (select & MELO_LIBRARY_SELECT (TRACK))
    end = g_stpcpy (end, "track,");
  if (select & MELO_LIBRARY_SELECT (COVER))
    end = g_stpcpy (end, "cover,");
  g_stpcpy (end, type == MELO_LIBRARY_TYPE_MEDIA ? "media_id" : "rowid");

  /* Prepare string for conditions */
  conds = g_string_new_len (NULL, MELO_LIBRARY_SQL_CONDS_SIZE);
  if (!conds)
    return false;

  /* Prepare matches */
  if (match) {
    matches = g_string_new_len (NULL, MELO_LIBRARY_SQL_CONDS_SIZE);
    if (!matches) {
      g_string_free (conds, TRUE);
      return false;
    }
  }

  /* Generate condition */
  while (field != MELO_LIBRARY_FIELD_LAST) {
    char temp[MELO_LIBRARY_SQL_CONDS_SIZE];
    const char *m = match ? "MATCH" : "=";
    bool to_match = false;

    switch (field) {
    case MELO_LIBRARY_FIELD_PLAYER_ID:
      sqlite3_snprintf (
          sizeof (temp), temp, "player_id = %llu", va_arg (args, uint64_t));
      break;
    case MELO_LIBRARY_FIELD_PATH_ID:
      sqlite3_snprintf (
          sizeof (temp), temp, "path_id = %llu", va_arg (args, uint64_t));
      break;
    case MELO_LIBRARY_FIELD_MEDIA_ID:
      sqlite3_snprintf (
          sizeof (temp), temp, "media_id = %llu", va_arg (args, uint64_t));
      break;
    case MELO_LIBRARY_FIELD_ARTIST_ID:
      sqlite3_snprintf (
          sizeof (temp), temp, "artist_id = %llu", va_arg (args, uint64_t));
      break;
    case MELO_LIBRARY_FIELD_ALBUM_ID:
      sqlite3_snprintf (
          sizeof (temp), temp, "album_id = %llu", va_arg (args, uint64_t));
      break;
    case MELO_LIBRARY_FIELD_GENRE_ID:
      sqlite3_snprintf (
          sizeof (temp), temp, "genre_id = %llu", va_arg (args, uint64_t));
      break;
    case MELO_LIBRARY_FIELD_PLAYER:
      sqlite3_snprintf (
          sizeof (temp), temp, "player = %Q", va_arg (args, const char *));
      break;
    case MELO_LIBRARY_FIELD_PATH: {
      const char *path = va_arg (args, const char *);
      const char *fmt = "path = %Q";
      if (path) {
        size_t len = strlen (path);
        if (len > 0 && path[len - 1] != '/')
          fmt = "path = '%q/'";
      }
      sqlite3_snprintf (sizeof (temp), temp, fmt, path);
      break;
    }
    case MELO_LIBRARY_FIELD_MEDIA:
      sqlite3_snprintf (
          sizeof (temp), temp, "media %s %Q", m, va_arg (args, const char *));
      to_match = match;
      break;
    case MELO_LIBRARY_FIELD_NAME:
      sqlite3_snprintf (
          sizeof (temp), temp, "name %s %Q", m, va_arg (args, const char *));
      to_match = match;
      break;
    case MELO_LIBRARY_FIELD_TIMESTAMP:
      sqlite3_snprintf (
          sizeof (temp), temp, "timestamp = %llu", va_arg (args, uint64_t));
      break;
    case MELO_LIBRARY_FIELD_TITLE:
      sqlite3_snprintf (
          sizeof (temp), temp, "title %s %Q", m, va_arg (args, const char *));
      to_match = match;
      break;
    case MELO_LIBRARY_FIELD_ARTIST:
      sqlite3_snprintf (
          sizeof (temp), temp, "artist %s %Q", m, va_arg (args, const char *));
      to_match = match;
      break;
    case MELO_LIBRARY_FIELD_ALBUM:
      sqlite3_snprintf (
          sizeof (temp), temp, "album %s %Q", m, va_arg (args, const char *));
      to_match = match;
      break;
    case MELO_LIBRARY_FIELD_GENRE:
      sqlite3_snprintf (
          sizeof (temp), temp, "genre %s %Q", m, va_arg (args, const char *));
      to_match = match;
      break;
    case MELO_LIBRARY_FIELD_DATE:
      sqlite3_snprintf (
          sizeof (temp), temp, "date = %Q", va_arg (args, const char *));
      break;
    case MELO_LIBRARY_FIELD_TRACK:
      sqlite3_snprintf (
          sizeof (temp), temp, "track = %u", va_arg (args, unsigned int));
      break;
    case MELO_LIBRARY_FIELD_COVER:
      sqlite3_snprintf (
          sizeof (temp), temp, "cover = %Q", va_arg (args, const char *));
      break;
    default:
      MELO_LOGE ("invalid field %d", field);
      if (match)
        g_string_free (matches, TRUE);
      g_string_free (conds, TRUE);
      return false;
    }

    /* Append condition */
    if (to_match)
      g_string_append_printf (
          matches, "%s%s", matches->len ? " OR " : "", temp);
    else
      g_string_append (conds, temp);

    /* Get next field */
    field = va_arg (args, MeloLibraryField);
    if (field == MELO_LIBRARY_FIELD_LAST)
      break;

    /* Append condition join */
    if (!to_match)
      g_string_append (conds, " AND ");
  }

  /* Append matches condition */
  if (match) {
    size_t len = matches->len;
    char *m = g_string_free (matches, FALSE);
    if (len)
      g_string_append_printf (conds,
          "%smedia_id IN (SELECT docid FROM fts WHERE %s)",
          conds->len ? " AND " : "", m);
    g_free (m);
  }

  /* Finalize conditions */
  if (!conds->len)
    g_string_append (conds, "1");
  conditions = g_string_free (conds, FALSE);

  /* Generate order directive */
  if (sort_field != MELO_LIBRARY_FIELD_NONE &&
      sort_field < MELO_LIBRARY_FIELD_COUNT) {
    /* Setup order clause */
    order = "ORDER BY ";
    order_col = melo_library_type_str[sort_field];
    order_dir = sort_desc ? " COLLATE NOCASE DESC" : " COLLATE NOCASE ASC";
  } else
    order = order_col = order_dir = "";

  /* Create SQL request */
  sql = sqlite3_mprintf ("SELECT %s FROM %s WHERE %s %s%s%s LIMIT %u,%u;",
      fields, table, conditions, order, order_col, order_dir, offset, count);
  g_free (conditions);
  if (!sql)
    return false;

  /* Prepare request */
  ret = sqlite3_prepare_v2 (melo_library_db, sql, -1, &req, NULL);
  sqlite3_free (sql);
  if (ret != SQLITE_OK)
    return false;

  /* Get value from results */
  while ((ret = sqlite3_step (req)) == SQLITE_ROW) {
    MeloLibraryData data = {0};
    MeloTags *tags = NULL;
    uint64_t id;
    unsigned int i = 0;
    char id_str[21];

    /* Set data */
    if (select & MELO_LIBRARY_SELECT (PLAYER))
      data.player = (char *) sqlite3_column_text (req, i++);
    if (select & MELO_LIBRARY_SELECT (PATH))
      data.path = (char *) sqlite3_column_text (req, i++);
    if (select & MELO_LIBRARY_SELECT (MEDIA))
      data.media = (char *) sqlite3_column_text (req, i++);
    if (select & MELO_LIBRARY_SELECT (NAME))
      data.name = (char *) sqlite3_column_text (req, i++);
    if (select & MELO_LIBRARY_SELECT (TIMESTAMP))
      data.timestamp = sqlite3_column_int64 (req, i++);

    /* Set tags */
    if (select &
        (MELO_LIBRARY_SELECT (TITLE) | MELO_LIBRARY_SELECT (ARTIST) |
            MELO_LIBRARY_SELECT (ALBUM) | MELO_LIBRARY_SELECT (GENRE) |
            MELO_LIBRARY_SELECT (TRACK) | MELO_LIBRARY_SELECT (COVER))) {
      /* Create new tags */
      tags = melo_tags_new ();

      /* Set tags fields */
      if (select & MELO_LIBRARY_SELECT (TITLE))
        melo_tags_set_title (
            tags, (const char *) sqlite3_column_text (req, i++));
      if (select & MELO_LIBRARY_SELECT (ARTIST))
        melo_tags_set_artist (
            tags, (const char *) sqlite3_column_text (req, i++));
      if (select & MELO_LIBRARY_SELECT (ALBUM))
        melo_tags_set_album (
            tags, (const char *) sqlite3_column_text (req, i++));
      if (select & MELO_LIBRARY_SELECT (GENRE))
        melo_tags_set_genre (
            tags, (const char *) sqlite3_column_text (req, i++));
      if (select & MELO_LIBRARY_SELECT (TRACK))
        melo_tags_set_track (tags, sqlite3_column_int (req, i++));
      if (select & MELO_LIBRARY_SELECT (COVER))
        melo_tags_set_cover (
            tags, NULL, (const char *) sqlite3_column_text (req, i++));
    }

    /* Get database ID */
    id = sqlite3_column_int64 (req, i++);
    snprintf (id_str, sizeof (id_str), "%llu", (unsigned long long) id);
    data.id = id_str;

    /* Call callback */
    cb (&data, tags, user_data);

    /* Free tags */
    melo_tags_unref (tags);
  }

  /* Finalize request */
  sqlite3_finalize (req);

  return true;
}

/**
 * melo_library_find:
 * @type: the #MeloLibraryType to use
 * @cb: the function called for next results row
 * @user_data: the data to pass to @cb
 * @select: a combination of MELO_LIBRARY_SELECT() values
 * @count: the number of rows to medias
 * @offset: the offset from which to start fetch (start at 0)
 * @sort_field: the #MeloLibraryField to use for sort
 * @sort_desc: set to %true to sort in descending, otherwise in ascending order
 * @match: set to %true to match instead of strict selection
 * @field0: the first field to set for selection
 * @...: the value of the first field, followed optionally by more field/value
 *     pairs, followed by #MELO_LIBRARY_FIELD_LAST
 *
 * This function is used to retrieved one or more medias from the library
 * database. For each media, the @cb function will be called with the fields
 * requested with @select.
 *
 * The @sort_field and @sort_desc can control on which field the data will be
 * sorted and in which order. To keep the original order, @sort_field should be
 * set to #MELO_LIBRARY_FIELD_NONE.
 *
 * The @field0 and following parameters are used to fix some field values, such
 * as the artist or album, and then restrict the request to only some medias. If
 * @field0 is set to MELO_LIBRARY_FIELD_LAST, no restriction is applied.
 * Basically a 'AND' operator is used between the fields set. If @match is set,
 * the field representing string values will be matched (not strictly equal) and
 * a 'OR' operator will be applied. This is useful to make a search by keyword.
 *
 * Returns: %true if the data has been fetched successfully, %false otherwise.
 */
bool
melo_library_find (MeloLibraryType type, MeloLibraryCb cb, void *user_data,
    unsigned int select, size_t count, off_t offset,
    MeloLibraryField sort_field, bool sort_desc, bool match,
    MeloLibraryField field0, ...)
{
  va_list args;
  bool ret;

  /* Get list */
  va_start (args, field0);
  ret = melo_library_vfind (type, cb, user_data, select, count, offset,
      sort_field, sort_desc, match, field0, args);
  va_end (args);

  return ret;
}

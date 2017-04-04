/*
 * melo_library_file.c: File Library using file database
 *
 * Copyright (C) 2017 Alexandre Dilly <dillya@sparod.com>
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

#include "melo_library_file.h"

/* File library info */
static MeloBrowserInfo melo_library_file_info = {
  .name = "Browse media library",
  .description = "Navigate though whole media library",
  /* Search support */
  .search_support = FALSE,
  .search_hint_support = FALSE,
  .search_input_text = "Search a media by title, artist or album...",
  .search_button_text = "Search",
  /* Tags support */
  .tags_support = TRUE,
  .tags_cache_support = FALSE,
};

static const MeloBrowserInfo *melo_library_file_get_info (MeloBrowser *browser);
static MeloBrowserList *melo_library_file_get_list (MeloBrowser *browser,
                                                  const gchar *path,
                                                  gint offset, gint count,
                                                  const gchar *token,
                                                  MeloBrowserTagsMode tags_mode,
                                                  MeloTagsFields tags_fields);
static MeloTags *melo_library_file_get_tags (MeloBrowser *browser,
                                             const gchar *path,
                                             MeloTagsFields fields);
static gboolean melo_library_file_add (MeloBrowser *browser, const gchar *path);
static gboolean melo_library_file_play (MeloBrowser *browser,
                                        const gchar *path);

static gboolean melo_library_file_get_cover (MeloBrowser *browser,
                                             const gchar *path, GBytes **data,
                                             gchar **type);

struct _MeloLibraryFilePrivate {
  GMutex mutex;
  MeloFileDB *fdb;
};

G_DEFINE_TYPE_WITH_PRIVATE (MeloLibraryFile, melo_library_file, MELO_TYPE_BROWSER)

static void
melo_library_file_finalize (GObject *gobject)
{
  MeloLibraryFile *library_file = MELO_LIBRARY_FILE (gobject);
  MeloLibraryFilePrivate *priv =
                          melo_library_file_get_instance_private (library_file);

  /* Clear mutex */
  g_mutex_clear (&priv->mutex);

  /* Chain up to the parent class */
  G_OBJECT_CLASS (melo_library_file_parent_class)->finalize (gobject);
}

static void
melo_library_file_class_init (MeloLibraryFileClass *klass)
{
  MeloBrowserClass *bclass = MELO_BROWSER_CLASS (klass);
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  bclass->get_info = melo_library_file_get_info;
  bclass->get_list = melo_library_file_get_list;
  bclass->get_tags = melo_library_file_get_tags;
  bclass->add = melo_library_file_add;
  bclass->play = melo_library_file_play;

  bclass->get_cover = melo_library_file_get_cover;

  /* Add custom finalize() function */
  oclass->finalize = melo_library_file_finalize;
}

static void
melo_library_file_init (MeloLibraryFile *self)
{
  MeloLibraryFilePrivate *priv = melo_library_file_get_instance_private (self);

  self->priv = priv;

  /* Init mutex */
  g_mutex_init (&priv->mutex);
}

void
melo_library_file_set_db (MeloLibraryFile *bfile, MeloFileDB *fdb)
{
  bfile->priv->fdb = fdb;
}

static const MeloBrowserInfo *
melo_library_file_get_info (MeloBrowser *browser)
{
  return &melo_library_file_info;
}

static inline gchar *
melo_library_gen_id (gint64 id)
{
  return g_strdup_printf ("%u", id);
}

static gboolean
melo_library_file_gen_title (const gchar *path, const gchar *file, gint id,
                             MeloTags *tags, gpointer user_data)
{
  GList **list = (GList **) user_data;
  MeloBrowserItem *item;

  item = melo_browser_item_new (NULL, "media");
  item->name = melo_library_gen_id (id);
  item->full_name = g_strdup (tags->title);
  item->add = g_strdup ("Add to playlist");
  item->tags = tags;
  *list = g_list_prepend (*list, item);

  return TRUE;
}

static gboolean
melo_library_file_gen_artist (const gchar *path, const gchar *file, gint id,
                              MeloTags *tags, gpointer user_data)
{
  GList **list = (GList **) user_data;
  MeloBrowserItem *item;

  item = melo_browser_item_new (NULL, "category");
  item->name = melo_library_gen_id (id);
  item->full_name = g_strdup (tags->artist);
  item->tags = tags;
  *list = g_list_prepend (*list, item);

  return TRUE;
}

static gboolean
melo_library_file_gen_album (const gchar *path, const gchar *file, gint id,
                             MeloTags *tags, gpointer user_data)
{
  GList **list = (GList **) user_data;
  MeloBrowserItem *item;

  item = melo_browser_item_new (NULL, "category");
  item->name = melo_library_gen_id (id);
  item->full_name = g_strdup (tags->album);
  item->tags = tags;
  *list = g_list_prepend (*list, item);

  return TRUE;
}

static gboolean
melo_library_file_gen_genre (const gchar *path, const gchar *file, gint id,
                             MeloTags *tags, gpointer user_data)
{
  GList **list = (GList **) user_data;
  MeloBrowserItem *item;

  item = melo_browser_item_new (NULL, "category");
  item->name = melo_library_gen_id (id);
  item->full_name = g_strdup (tags->genre);
  item->tags = tags;
  *list = g_list_prepend (*list, item);

  return TRUE;
}

static gint
melo_library_file_get_id (const gchar *path)
{
  gint id = -1;

  /* Get ID from path */
  if (path[0] == '/' && path[1] != '\0') {
    id = strtoul (path + 1, NULL, 10);
    if (id == ULONG_MAX && errno == ERANGE)
      id = -1;
  }

  return id;
}

static gint
melo_library_file_get_last_id (const gchar *path, gint *level)
{
  gint id = -1;

  /* Get ID from path */
  while (*path++ != '\0') {
    id = strtoul (path, &path, 10);
    if (id == ULONG_MAX && errno == ERANGE)
      id = -1;
  }

  return id;
}

static MeloBrowserList *
melo_library_file_get_list (MeloBrowser *browser, const gchar *path,
                            gint offset, gint count, const gchar *token,
                            MeloBrowserTagsMode tags_mode,
                            MeloTagsFields tags_fields)
{
  MeloLibraryFile *lfile = MELO_LIBRARY_FILE (browser);
  MeloLibraryFilePrivate *priv = lfile->priv;
  MeloFileDBFields field = MELO_FILE_DB_FIELDS_END;
  GObject *obj = G_OBJECT (browser);
  MeloBrowserList *list;
  gint id = -1;
  GList *l;

  /* Create browser list */
  list = melo_browser_list_new (path);
  if (!list)
    return NULL;

  /* Check path */
  if (!path || *path != '/')
    return NULL;
  path++;

  /* Parse path and select list action */
  if (*path == '\0') {
    /* Root path: "/" */
    MeloBrowserItem *item;

    /* Add genre entry to browser by genre */
    item = melo_browser_item_new ("genre", "category");
    item->full_name = g_strdup ("Genre");
    list->items = g_list_append(list->items, item);

    /* Add album entry to browser by album */
    item = melo_browser_item_new ("album", "category");
    item->full_name = g_strdup ("Album");
    list->items = g_list_append(list->items, item);

    /* Add artist entry to browser by artist */
    item = melo_browser_item_new ("artist", "category");
    item->full_name = g_strdup ("Artist");
    list->items = g_list_append(list->items, item);

    /* Add title entry to browser by title */
    item = melo_browser_item_new ("title", "category");
    item->full_name = g_strdup ("Title");
    list->items = g_list_append(list->items, item);
  } else if (g_str_has_prefix (path, "title")) {
    melo_file_db_get_song_list (priv->fdb, obj, melo_library_file_gen_title,
                                &list->items, offset, count,
                                MELO_FILE_DB_SORT_TITLE, MELO_TAGS_FIELDS_FULL,
                                MELO_FILE_DB_FIELDS_END);
  } else if (g_str_has_prefix (path, "artist")) {
    id = melo_library_file_get_id (path+6);
    if (id < 0)
      melo_file_db_get_artist_list (priv->fdb, obj,
                                    melo_library_file_gen_artist, &list->items,
                                    offset, count, MELO_FILE_DB_SORT_ARTIST,
                                    MELO_TAGS_FIELDS_FULL,
                                    MELO_FILE_DB_FIELDS_END);
    else
      field = MELO_FILE_DB_FIELDS_ARTIST_ID;
  } else if (g_str_has_prefix (path, "album")) {
    id = melo_library_file_get_id (path+5);
    if (id < 0)
      melo_file_db_get_album_list (priv->fdb, obj, melo_library_file_gen_album,
                                   &list->items, offset, count,
                                   MELO_FILE_DB_SORT_ALBUM,
                                   MELO_TAGS_FIELDS_FULL,
                                   MELO_FILE_DB_FIELDS_END);
    else
      field = MELO_FILE_DB_FIELDS_ALBUM_ID;
  } else if (g_str_has_prefix (path, "genre")) {
    id = melo_library_file_get_id (path+5);
    if (id < 0)
      melo_file_db_get_genre_list (priv->fdb, obj, melo_library_file_gen_genre,
                                   &list->items, offset, count,
                                   MELO_FILE_DB_SORT_GENRE,
                                   MELO_TAGS_FIELDS_FULL,
                                   MELO_FILE_DB_FIELDS_END);
    else
      field = MELO_FILE_DB_FIELDS_GENRE_ID;
  }

  /* Get song list */
  if (id != -1)
    melo_file_db_get_song_list (priv->fdb, obj, melo_library_file_gen_title,
                                &list->items, offset, count,
                                MELO_FILE_DB_SORT_TITLE, MELO_TAGS_FIELDS_FULL,
                                field, id, MELO_FILE_DB_FIELDS_END);

  /* Reverse final list */
  list->items = g_list_reverse (list->items);

  return list;
}

static MeloTags *
melo_library_file_get_tags (MeloBrowser *browser, const gchar *path,
                            MeloTagsFields fields)
{
  MeloLibraryFile *lfile = MELO_LIBRARY_FILE (browser);
  MeloLibraryFilePrivate *priv = lfile->priv;
  GObject *obj = G_OBJECT (browser);
  gint id, level;

  /* Check path */
  if (!path || *path != '/')
    return NULL;
  path++;

  /* Get tags */
  if (g_str_has_prefix (path, "title")) {
    id = melo_library_file_get_last_id (path+5, NULL);
  } else if (g_str_has_prefix (path, "artist")) {
    id = melo_library_file_get_last_id (path+6, &level);
    if (id < 0)
      return NULL;
    if (level == 1)
      return melo_file_db_get_artist (priv->fdb, obj, MELO_TAGS_FIELDS_FULL,
                                      MELO_FILE_DB_FIELDS_ARTIST_ID, id,
                                      MELO_FILE_DB_FIELDS_END);
  } else if (g_str_has_prefix (path, "album")) {
    id = melo_library_file_get_last_id (path+5, &level);
    if (id < 0)
      return NULL;
    if (level == 1)
      return melo_file_db_get_album (priv->fdb, obj, MELO_TAGS_FIELDS_FULL,
                                     MELO_FILE_DB_FIELDS_ALBUM_ID, id,
                                     MELO_FILE_DB_FIELDS_END);
  } else if (g_str_has_prefix (path, "genre")) {
    id = melo_library_file_get_last_id (path+5, &level);
    if (id < 0)
      return NULL;
    if (level == 1)
      return melo_file_db_get_genre (priv->fdb, obj, MELO_TAGS_FIELDS_FULL,
                                     MELO_FILE_DB_FIELDS_GENRE_ID, id,
                                     MELO_FILE_DB_FIELDS_END);
  }

  /* Get song tags */
  return melo_file_db_get_song (priv->fdb, obj, MELO_TAGS_FIELDS_FULL,
                                MELO_FILE_DB_FIELDS_FILE_ID, id,
                                MELO_FILE_DB_FIELDS_END);
}

static gboolean
melo_library_file_add_cb (const gchar *path, const gchar *file, gint id,
                          MeloTags *tags, gpointer user_data)
{
  MeloPlayer *player = (MeloPlayer *) user_data;
  gboolean ret;
  gchar *uri;

  /* Generate URI */
  uri = g_strjoin ("/", path, file, NULL);
  if (!uri)
    return FALSE;

  /* Add media */
  ret = melo_player_add (player, uri, file, tags);
  melo_tags_unref (tags);
  g_free (uri);

  return ret;
}

static gboolean
melo_library_file_add (MeloBrowser *browser, const gchar *path)
{
  MeloLibraryFile *lfile = MELO_LIBRARY_FILE (browser);
  gint id;

  /* Extract file ID from path */
  id = melo_library_file_get_last_id (path, NULL);

  /* Read from media ID */
  return melo_file_db_get_file_list (lfile->priv->fdb, G_OBJECT (browser),
                                     melo_library_file_add_cb, browser->player,
                                     0, 1, MELO_FILE_DB_SORT_NONE,
                                     MELO_TAGS_FIELDS_FULL,
                                     MELO_FILE_DB_FIELDS_FILE_ID, id,
                                     MELO_FILE_DB_FIELDS_END);
}

static gboolean
melo_library_file_play_cb (const gchar *path, const gchar *file, gint id,
                          MeloTags *tags, gpointer user_data)
{
  MeloPlayer *player = (MeloPlayer *) user_data;
  gboolean ret;
  gchar *uri;

  /* Generate URI */
  uri = g_strjoin ("/", path, file, NULL);
  if (!uri)
    return FALSE;

  /* Play media */
  ret = melo_player_play (player, uri, file, tags, TRUE);
  melo_tags_unref (tags);
  g_free (uri);

  return ret;
}

static gboolean
melo_library_file_play (MeloBrowser *browser, const gchar *path)
{
  MeloLibraryFile *lfile = MELO_LIBRARY_FILE (browser);
  gint id;

  /* Extract file ID from path */
  id = melo_library_file_get_last_id (path, NULL);

  /* Read from media ID */
  return melo_file_db_get_file_list (lfile->priv->fdb, G_OBJECT (browser),
                                     melo_library_file_play_cb, browser->player,
                                     0, 1, MELO_FILE_DB_SORT_NONE,
                                     MELO_TAGS_FIELDS_FULL,
                                     MELO_FILE_DB_FIELDS_FILE_ID, id,
                                     MELO_FILE_DB_FIELDS_END);
}

static gboolean
melo_library_file_get_cover (MeloBrowser *browser, const gchar *path,
                             GBytes **data, gchar **type)
{
  MeloLibraryFilePrivate *priv = (MELO_LIBRARY_FILE (browser))->priv;

  GMappedFile *file;
  gchar *fpath;

  /* Generate cover file path */
  fpath = g_strdup_printf ("%s/%s", melo_file_db_get_cover_path (priv->fdb),
                           path);

  /* File doesn't not exist */
  if (!g_file_test (fpath, G_FILE_TEST_EXISTS)) {
    g_free (fpath);
    return FALSE;
  }

  /* Map file and create a GBytes */
  file = g_mapped_file_new (fpath, FALSE, NULL);
  *data = g_mapped_file_get_bytes (file);
  g_mapped_file_unref (file);
  g_free (fpath);

  return TRUE;
}

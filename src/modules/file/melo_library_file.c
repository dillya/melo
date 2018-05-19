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

#include <stdlib.h>
#include <string.h>

#include "melo_file_utils.h"

#include "melo_library_file.h"

/* File library info */
static MeloBrowserInfo melo_library_file_info = {
  .name = "Browse media library",
  .description = "Navigate though whole media library",
  /* Search support */
  .search_support = TRUE,
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
                                        const MeloBrowserGetListParams *params);
static MeloBrowserList *melo_library_file_search (MeloBrowser *browser,
                                         const gchar *input,
                                         const MeloBrowserSearchParams *params);
static MeloTags *melo_library_file_get_tags (MeloBrowser *browser,
                                             const gchar *path,
                                             MeloTagsFields fields);
static gboolean melo_library_file_action (MeloBrowser *browser,
                                         const gchar *path,
                                         MeloBrowserItemAction action,
                                         const MeloBrowserActionParams *params);

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
  bclass->search = melo_library_file_search;
  bclass->get_tags = melo_library_file_get_tags;
  bclass->action = melo_library_file_action;

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
melo_library_gen_id (gint id)
{
  return g_strdup_printf ("%d", id);
}

static gboolean
melo_library_file_parse (const gchar *path, MeloFileDBType *type,
                         MeloFileDBFields *filter, gint *id, gint *id2)
{
  const gchar *stype;
  MeloFileDBFields f;
  MeloFileDBType t;
  gint ids[2];
  gint i = 0;

  /* Parse path */
  stype = *path == '/' ? ++path : path;
  while (path && (path = strchr (path, '/')) != NULL && *(++path) != '\0') {
    gchar *p;
    ids[i] = strtoul (path, &p, 10);
    if (errno)
      break;
    path = p;
    i++;
  }
  while (i < 2)
    ids[i++] = -1;

  /* Get type */
  if (g_str_has_prefix (stype, "title")) {
    t = MELO_FILE_DB_TYPE_SONG;
    f = MELO_FILE_DB_FIELDS_END;
    ids[1] = ids[0];
  } else if (g_str_has_prefix (stype, "artist")) {
    t = MELO_FILE_DB_TYPE_ARTIST;
    f = MELO_FILE_DB_FIELDS_ARTIST_ID;
  } else if (g_str_has_prefix (stype, "album")) {
    t = MELO_FILE_DB_TYPE_ALBUM;
    f = MELO_FILE_DB_FIELDS_ALBUM_ID;
  } else if (g_str_has_prefix (stype, "genre")) {
    t = MELO_FILE_DB_TYPE_GENRE;
    f = MELO_FILE_DB_FIELDS_GENRE_ID;
  } else
    return FALSE;

  /* Set values */
  if (type)
    *type = t;
  if (filter)
    *filter = f;
  if (id)
    *id = ids[0];
  if (id2)
    *id2 = ids[1];

  return TRUE;
}

static gboolean
melo_library_file_gen (const gchar *path, const gchar *file, gint id,
                       MeloFileDBType type, MeloTags *tags, gpointer user_data)
{
  GList **list = (GList **) user_data;
  MeloBrowserItemType item_type;
  const gchar *name = NULL;
  MeloBrowserItem *item;

  switch (type) {
    case MELO_FILE_DB_TYPE_FILE:
    case MELO_FILE_DB_TYPE_SONG:
      item_type = MELO_BROWSER_ITEM_TYPE_MEDIA;
      name = tags && tags->title ? tags->title : file;
      break;
    case MELO_FILE_DB_TYPE_ARTIST:
      item_type = MELO_BROWSER_ITEM_TYPE_CATEGORY;
      name = tags->artist;
      break;
    case MELO_FILE_DB_TYPE_ALBUM:
      item_type = MELO_BROWSER_ITEM_TYPE_CATEGORY;
      name = tags->album;
      break;
    case MELO_FILE_DB_TYPE_GENRE:
      item_type = MELO_BROWSER_ITEM_TYPE_CATEGORY;
      name = tags->genre;
      break;
    default:
      return FALSE;
  }

  /* Create MeloBrowserItem */
  item = melo_browser_item_new (NULL, item_type);
  item->id = melo_library_gen_id (id);
  item->name = g_strdup (name);
  item->tags = tags;
  item->actions = MELO_BROWSER_ITEM_ACTION_FIELDS_ADD |
                  MELO_BROWSER_ITEM_ACTION_FIELDS_PLAY;
  *list = g_list_prepend (*list, item);

  return TRUE;
}

static gboolean
melo_library_file_get_title_list (MeloLibraryFile *lfile, GList **list,
                                  gint offset, gint count, MeloSort sort,
                                  MeloTagsFields tags_fields)
{
  MeloLibraryFilePrivate *priv = lfile->priv;
  GObject *obj = G_OBJECT (lfile);

  return melo_file_db_get_list (priv->fdb, obj, melo_library_file_gen, list,
                                offset, count, sort, FALSE,
                                MELO_FILE_DB_TYPE_SONG, tags_fields,
                                MELO_FILE_DB_FIELDS_END);
}

static gboolean
melo_library_file_get_artist_list (MeloLibraryFile *lfile, GList **list,
                                   gint id, gint offset, gint count,
                                   MeloSort sort, MeloTagsFields tags_fields)
{
  MeloLibraryFilePrivate *priv = lfile->priv;
  GObject *obj = G_OBJECT (lfile);

  /* Get song list with artist ID */
  if (id >= 0)
    return melo_file_db_get_list (priv->fdb, obj, melo_library_file_gen, list,
                                  offset, count, sort, FALSE,
                                  MELO_FILE_DB_TYPE_SONG, tags_fields,
                                  MELO_FILE_DB_FIELDS_ARTIST_ID, id,
                                  MELO_FILE_DB_FIELDS_END);

  /* Get artist list */
  sort = melo_sort_replace (sort, MELO_SORT_ARTIST);
  return melo_file_db_get_list (priv->fdb, obj, melo_library_file_gen, list,
                                offset, count, sort, FALSE,
                                MELO_FILE_DB_TYPE_ARTIST,
                                tags_fields | MELO_TAGS_FIELDS_ARTIST,
                                MELO_FILE_DB_FIELDS_END);
}

static gboolean
melo_library_file_get_album_list (MeloLibraryFile *lfile, GList **list,
                                  gint id, gint offset, gint count,
                                  MeloSort sort, MeloTagsFields tags_fields)
{
  MeloLibraryFilePrivate *priv = lfile->priv;
  GObject *obj = G_OBJECT (lfile);

  /* Get song list with album ID */
  if (id >= 0)
    return melo_file_db_get_list (priv->fdb, obj, melo_library_file_gen, list,
                                  offset, count, sort, FALSE,
                                  MELO_FILE_DB_TYPE_SONG, tags_fields,
                                  MELO_FILE_DB_FIELDS_ALBUM_ID, id,
                                  MELO_FILE_DB_FIELDS_END);

  /* Get album list */
  sort = melo_sort_replace (sort, MELO_SORT_ALBUM);
  return melo_file_db_get_list (priv->fdb, obj, melo_library_file_gen, list,
                                offset, count, sort, FALSE,
                                MELO_FILE_DB_TYPE_ALBUM,
                                tags_fields | MELO_TAGS_FIELDS_ALBUM,
                                MELO_FILE_DB_FIELDS_END);
}

static gboolean
melo_library_file_get_genre_list (MeloLibraryFile *lfile, GList **list,
                                  gint id, gint offset, gint count,
                                  MeloSort sort, MeloTagsFields tags_fields)
{
  MeloLibraryFilePrivate *priv = lfile->priv;
  GObject *obj = G_OBJECT (lfile);

  /* Get song list with genre ID */
  if (id >= 0)
    return melo_file_db_get_list (priv->fdb, obj, melo_library_file_gen, list,
                                  offset, count, sort, FALSE,
                                  MELO_FILE_DB_TYPE_SONG, tags_fields,
                                  MELO_FILE_DB_FIELDS_GENRE_ID, id,
                                  MELO_FILE_DB_FIELDS_END);

  /* Get genre list */
  sort = melo_sort_replace (sort, MELO_SORT_GENRE);
  return melo_file_db_get_list (priv->fdb, obj, melo_library_file_gen, list,
                                offset, count, sort, FALSE,
                                MELO_FILE_DB_TYPE_GENRE,
                                tags_fields | MELO_TAGS_FIELDS_GENRE,
                                MELO_FILE_DB_FIELDS_END);
}

static MeloBrowserList *
melo_library_file_get_list (MeloBrowser *browser, const gchar *path,
                            const MeloBrowserGetListParams *params)
{
  MeloLibraryFile *lfile = MELO_LIBRARY_FILE (browser);
  MeloBrowserList *list;
  MeloFileDBType type;
  gint id;

  /* Check path */
  if (!path || *path != '/')
    return NULL;

  /* Create browser list */
  list = melo_browser_list_new (path++);
  if (!list)
    return NULL;

  /* Parse path and select list action */
  if (*path == '\0') {
    /* Root path: "/" */
    MeloBrowserItem *item;

    /* Add title entry to browser by title */
    item = melo_browser_item_new ("title", MELO_BROWSER_ITEM_TYPE_CATEGORY);
    item->name = g_strdup ("Title");
    list->items = g_list_append(list->items, item);

    /* Add artist entry to browser by artist */
    item = melo_browser_item_new ("artist", MELO_BROWSER_ITEM_TYPE_CATEGORY);
    item->name = g_strdup ("Artist");
    list->items = g_list_append(list->items, item);

    /* Add album entry to browser by album */
    item = melo_browser_item_new ("album", MELO_BROWSER_ITEM_TYPE_CATEGORY);
    item->name = g_strdup ("Album");
    list->items = g_list_append(list->items, item);

    /* Add genre entry to browser by genre */
    item = melo_browser_item_new ("genre", MELO_BROWSER_ITEM_TYPE_CATEGORY);
    item->name = g_strdup ("Genre");
    list->items = g_list_append(list->items, item);

    return list;
  }

  /* Parse path */
  if (!melo_library_file_parse (path, &type, NULL, &id, NULL))
    return list;

  /* Get list */
  switch (type) {
    case MELO_FILE_DB_TYPE_SONG:
      melo_library_file_get_title_list (lfile, &list->items, params->offset,
                                        params->count, params->sort,
                                        params->tags_fields);
      break;
    case MELO_FILE_DB_TYPE_ARTIST:
      melo_library_file_get_artist_list (lfile, &list->items, id,
                                         params->offset, params->count,
                                         params->sort, params->tags_fields);
      break;
    case MELO_FILE_DB_TYPE_ALBUM:
      melo_library_file_get_album_list (lfile, &list->items, id, params->offset,
                                        params->count, params->sort,
                                        params->tags_fields);
      break;
    case MELO_FILE_DB_TYPE_GENRE:
      melo_library_file_get_genre_list (lfile, &list->items, id, params->offset,
                                        params->count, params->sort,
                                        params->tags_fields);
      break;
  }

  /* Reverse final list */
  list->items = g_list_reverse (list->items);

  return list;
}

static MeloBrowserList *
melo_library_file_search (MeloBrowser *browser, const gchar *input,
                          const MeloBrowserSearchParams *params)
{
  MeloLibraryFile *lfile = MELO_LIBRARY_FILE (browser);
  MeloLibraryFilePrivate *priv = lfile->priv;
  GObject *obj = G_OBJECT (lfile);
  MeloBrowserList *list;
  MeloFileDBType type;
  gint id;

  /* Check path */
  if (!input)
    return NULL;

  /* Create browser list */
  list = melo_browser_list_new ("/title/");
  if (!list)
    return NULL;

  melo_file_db_get_list (priv->fdb, obj, melo_library_file_gen, &list->items,
                         params->offset, params->count, params->sort, TRUE,
                         MELO_FILE_DB_TYPE_SONG, params->tags_fields,
                         MELO_FILE_DB_FIELDS_TITLE, input,
                         MELO_FILE_DB_FIELDS_FILE, input,
                         MELO_FILE_DB_FIELDS_END);

  return list;
}

static MeloTags *
melo_library_file_get_tags (MeloBrowser *browser, const gchar *path,
                            MeloTagsFields fields)
{
  MeloLibraryFile *lfile = MELO_LIBRARY_FILE (browser);
  MeloLibraryFilePrivate *priv = lfile->priv;
  GObject *obj = G_OBJECT (browser);
  MeloFileDBFields filter;
  MeloFileDBType type;
  gint id, id2;

  /* Check path */
  if (!path || *path != '/')
    return NULL;
  path++;

  /* Parse path */
  if (!melo_library_file_parse (path, &type, &filter, &id, &id2) || id == -1)
    return NULL;

  /* Get tags */
  if (id2 == -1)
    return melo_file_db_get_tags (priv->fdb, obj, type, MELO_TAGS_FIELDS_FULL,
                                  filter, id, MELO_FILE_DB_FIELDS_END);

  /* Get song tags */
  return melo_file_db_get_tags (priv->fdb, obj, type, MELO_TAGS_FIELDS_FULL,
                                MELO_FILE_DB_FIELDS_FILE_ID, id2,
                                MELO_FILE_DB_FIELDS_END);
}

static gboolean
melo_library_file_add_cb (const gchar *path, const gchar *file, gint id,
                          MeloFileDBType type, MeloTags *tags,
                          gpointer user_data)
{
  MeloPlayer *player = (MeloPlayer *) user_data;
  gboolean ret;
  gchar *uri;

  /* Generate URI */
  uri = g_strjoin ("/", path, file, NULL);
  if (!uri)
    return FALSE;

  /* Check and mount volume for remote files */
  if (!melo_file_utils_check_and_mount_uri (uri, NULL, NULL)) {
    g_free (uri);
    return FALSE;
  }

  /* Add media */
  ret = melo_player_add (player, uri, file, tags);
  melo_tags_unref (tags);
  g_free (uri);

  return ret;
}

static gboolean
melo_library_file_add (MeloBrowser *browser, const gchar *path,
                       const MeloBrowserActionParams *params)
{
  MeloLibraryFile *lfile = MELO_LIBRARY_FILE (browser);
  MeloFileDBFields filter;
  MeloFileDBType type;
  gint id, id2;

  /* Parse path */
  if (!melo_library_file_parse (path, &type, &filter, &id, &id2) ||
      (type != MELO_FILE_DB_TYPE_SONG && id == -1))
    return FALSE;

  /* Add all medias to playlist */
  if (id2 == -1)
    return melo_file_db_get_list (lfile->priv->fdb, G_OBJECT (browser),
                                  melo_library_file_add_cb, browser->player, 0,
                                  -1, params->sort, FALSE,
                                  MELO_FILE_DB_TYPE_FILE, MELO_TAGS_FIELDS_FULL,
                                  filter, id, MELO_FILE_DB_FIELDS_END);

  /* Add media to playlist */
  return melo_file_db_get_list (lfile->priv->fdb, G_OBJECT (browser),
                                melo_library_file_add_cb, browser->player, 0, 1,
                                params->sort, FALSE,
                                MELO_FILE_DB_TYPE_FILE, MELO_TAGS_FIELDS_FULL,
                                MELO_FILE_DB_FIELDS_FILE_ID, id2,
                                MELO_FILE_DB_FIELDS_END);
}

static gboolean
melo_library_file_play_cb (const gchar *path, const gchar *file, gint id,
                           MeloFileDBType type, MeloTags *tags,
                           gpointer user_data)
{
  MeloPlayer *player = (MeloPlayer *) user_data;
  gboolean ret;
  gchar *uri;

  /* Generate URI */
  uri = g_strjoin ("/", path, file, NULL);
  if (!uri)
    return FALSE;

  /* Check and mount volume for remote files */
  if (!melo_file_utils_check_and_mount_uri (uri, NULL, NULL)) {
    g_free (uri);
    return FALSE;
  }

  /* Play media */
  ret = melo_player_play (player, uri, file, tags, TRUE);
  melo_tags_unref (tags);
  g_free (uri);

  return ret;
}

static gboolean
melo_library_file_play (MeloBrowser *browser, const gchar *path,
                        const MeloBrowserActionParams *params)
{
  MeloLibraryFile *lfile = MELO_LIBRARY_FILE (browser);
  MeloFileDBFields filter;
  MeloFileDBType type;
  gint id, id2;

  /* Parse path */
  if (!melo_library_file_parse (path, &type, &filter, &id, &id2) ||
      (type != MELO_FILE_DB_TYPE_SONG && id == -1))
    return FALSE;

  /* Play first media and add other to playlist */
  if (id2 == -1) {
    /* Play first media */
    melo_file_db_get_list (lfile->priv->fdb, G_OBJECT (browser),
                           melo_library_file_play_cb, browser->player, 0, 1,
                           params->sort, FALSE,
                           MELO_FILE_DB_TYPE_FILE, MELO_TAGS_FIELDS_FULL,
                           filter, id,
                           MELO_FILE_DB_FIELDS_END);

    /* Add other to playlist */
    return melo_file_db_get_list (lfile->priv->fdb, G_OBJECT (browser),
                                  melo_library_file_add_cb, browser->player, 1,
                                  -1, params->sort, FALSE,
                                  MELO_FILE_DB_TYPE_FILE, MELO_TAGS_FIELDS_FULL,
                                  filter, id, MELO_FILE_DB_FIELDS_END);
  }


  /* Play media */
  return melo_file_db_get_list (lfile->priv->fdb, G_OBJECT (browser),
                                melo_library_file_play_cb, browser->player, 0,
                                1, params->sort, FALSE,
                                MELO_FILE_DB_TYPE_FILE, MELO_TAGS_FIELDS_FULL,
                                MELO_FILE_DB_FIELDS_FILE_ID, id2,
                                MELO_FILE_DB_FIELDS_END);
}
static gboolean
melo_library_file_action (MeloBrowser *browser, const gchar *path,
                          MeloBrowserItemAction action,
                          const MeloBrowserActionParams *params)
{
  switch (action) {
    case MELO_BROWSER_ITEM_ACTION_ADD:
      return melo_library_file_add (browser, path, params);
    case MELO_BROWSER_ITEM_ACTION_PLAY:
      return melo_library_file_play (browser, path, params);
    default:
      ;
  }

  return FALSE;
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

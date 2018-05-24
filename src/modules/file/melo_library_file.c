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

#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>

#include "melo_file_utils.h"

#include "melo_library_file.h"

/* Count of sub location in a path */
#define MELO_LIBRARY_FILE_PARSE_COUNT_MAX 3

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

typedef struct _MeloLibraryFileParse {
  MeloFileDBType type;
  guint id;
  MeloFileDBFields filter;
} MeloLibraryFileParse;

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

static gint
melo_library_file_parse (const gchar *path, MeloLibraryFileParse *parse,
                         guint parse_count)
{
  guint count = 0;

  /* Check path */
  if (!path)
    return -1;

  /* Find first correct value */
  while (*path == '/')
    path++;

  /* Reset parse array */
  memset (parse, 0, sizeof (*parse) * parse_count);

  /* Parse path as /type/ID/type/ID/... */
  while (count < parse_count) {
    const gchar *type;
    gchar *next;
    guint len;

    /* Get type length */
    type = path;
    path = strchrnul (path, '/');
    len = path - type;

    /* Parse type */
    if (len == 4 && *type == 's') {
      parse->type = MELO_FILE_DB_TYPE_SONG;
      parse->filter = MELO_FILE_DB_FIELDS_FILE_ID;
    } else if (len == 5 && *type == 'a') {
      parse->type = MELO_FILE_DB_TYPE_ALBUM;
      parse->filter = MELO_FILE_DB_FIELDS_ALBUM_ID;
    } else if (len == 6 && *type == 'a') {
      parse->type = MELO_FILE_DB_TYPE_ARTIST;
      parse->filter = MELO_FILE_DB_FIELDS_ARTIST_ID;
    } else if (len == 5 && *type == 'g') {
      parse->type = MELO_FILE_DB_TYPE_GENRE;
      parse->filter = MELO_FILE_DB_FIELDS_GENRE_ID;
    } else
      return -1;

    /* Get next ID */
    while (*path == '/')
      path++;
    parse->id = strtoul (path, &next, 0);
    if (*next != '\0' && path == next)
      return -1;

    /* No ID */
    if (!parse->id)
      parse->filter = MELO_FILE_DB_FIELDS_END;

    /* Next parse element */
    parse++;
    count++;

    /* Find next type */
    path = next;
    while (*path == '/')
      path++;
    if (*path == '\0')
      break;
  }

  return count;
}
static gboolean
melo_library_file_gen (const gchar *path, const gchar *file, gint id,
                       MeloFileDBType type, MeloTags *tags, gpointer user_data)
{
  GList **list = (GList **) user_data;
  MeloBrowserItemType item_type;
  const gchar *type_name = "";
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
      type_name = "/album";
      break;
    case MELO_FILE_DB_TYPE_ALBUM:
      item_type = MELO_BROWSER_ITEM_TYPE_CATEGORY;
      name = tags->album;
      type_name = "/song";
      break;
    case MELO_FILE_DB_TYPE_GENRE:
      item_type = MELO_BROWSER_ITEM_TYPE_CATEGORY;
      name = tags->genre;
      type_name = "/album";
      break;
    default:
      return FALSE;
  }

  /* Create MeloBrowserItem */
  item = melo_browser_item_new (NULL, item_type);
  item->id = g_strdup_printf ("%d%s", id, type_name);
  item->name = g_strdup (name);
  item->tags = tags;
  item->actions = MELO_BROWSER_ITEM_ACTION_FIELDS_ADD |
                  MELO_BROWSER_ITEM_ACTION_FIELDS_PLAY;
  *list = g_list_prepend (*list, item);

  return TRUE;
}

static MeloBrowserList *
melo_library_file_get_list (MeloBrowser *browser, const gchar *path,
                            const MeloBrowserGetListParams *params)
{
  MeloLibraryFileParse parse[MELO_LIBRARY_FILE_PARSE_COUNT_MAX];
  gint count = MELO_LIBRARY_FILE_PARSE_COUNT_MAX;
  MeloLibraryFile *lfile = MELO_LIBRARY_FILE (browser);
  MeloLibraryFilePrivate *priv = lfile->priv;
  GObject *obj = G_OBJECT (browser);
  MeloBrowserList *list;
  MeloBrowserItem *item;
  MeloTagsFields tags_fields;
  MeloFileDBType type;
  MeloSort sort;

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

    /* Add song entry to browse by song */
    item = melo_browser_item_new ("song", MELO_BROWSER_ITEM_TYPE_CATEGORY);
    item->name = g_strdup ("Song");
    list->items = g_list_append(list->items, item);

    /* Add artist entry to browse by artist */
    item = melo_browser_item_new ("artist", MELO_BROWSER_ITEM_TYPE_CATEGORY);
    item->name = g_strdup ("Artist");
    list->items = g_list_append(list->items, item);

    /* Add album entry to browse by album */
    item = melo_browser_item_new ("album", MELO_BROWSER_ITEM_TYPE_CATEGORY);
    item->name = g_strdup ("Album");
    list->items = g_list_append(list->items, item);

    /* Add genre entry to browse by genre */
    item = melo_browser_item_new ("genre", MELO_BROWSER_ITEM_TYPE_CATEGORY);
    item->name = g_strdup ("Genre");
    list->items = g_list_append(list->items, item);

    return list;
  }

  /* Parse path */
  count = melo_library_file_parse (path, parse, count);
  if (count <= 0)
    goto error;

  /* Update tags fields and sort to fit the type */
  type = parse[count-1].type;
  tags_fields = params->tags_fields;
  sort = params->sort;
  if (type == MELO_FILE_DB_TYPE_ARTIST) {
    tags_fields |= MELO_TAGS_FIELDS_ARTIST;
    sort = melo_sort_replace (sort, MELO_SORT_ARTIST);
  } else if (type == MELO_FILE_DB_TYPE_ALBUM) {
    tags_fields |= MELO_TAGS_FIELDS_ALBUM;
    sort = melo_sort_replace (sort, MELO_SORT_ALBUM);
  } else if (type == MELO_FILE_DB_TYPE_GENRE) {
    tags_fields |= MELO_TAGS_FIELDS_GENRE;
    sort = melo_sort_replace (sort, MELO_SORT_GENRE);
  }

  /* Generate list */
  if (!melo_file_db_get_list (priv->fdb, obj, melo_library_file_gen,
                              &list->items, params->offset, params->count, sort,
                              FALSE, type, tags_fields,
                              parse[0].filter, parse[0].id,
                              parse[1].filter, parse[1].id,
                              parse[2].filter, parse[2].id,
                              MELO_FILE_DB_FIELDS_END))
    goto error;



  /* Reverse final list */
  list->items = g_list_reverse (list->items);

  return list;

error:
  melo_browser_list_free (list);
  return NULL;
}

static MeloBrowserList *
melo_library_file_search (MeloBrowser *browser, const gchar *input,
                          const MeloBrowserSearchParams *params)
{
  MeloLibraryFile *lfile = MELO_LIBRARY_FILE (browser);
  MeloLibraryFilePrivate *priv = lfile->priv;
  GObject *obj = G_OBJECT (lfile);
  MeloBrowserList *list;

  /* Check path */
  if (!input)
    return NULL;

  /* Create browser list */
  list = melo_browser_list_new ("/song/");
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
  MeloLibraryFileParse parse[MELO_LIBRARY_FILE_PARSE_COUNT_MAX];
  gint count = MELO_LIBRARY_FILE_PARSE_COUNT_MAX;
  MeloLibraryFile *lfile = MELO_LIBRARY_FILE (browser);
  MeloLibraryFilePrivate *priv = lfile->priv;
  GObject *obj = G_OBJECT (browser);

  /* Parse path */
  count = melo_library_file_parse (path, parse, count);
  if (count <= 0 || !parse[count-1].id)
    return NULL;

  /* Get tags */
  return melo_file_db_get_tags (priv->fdb, obj, parse[count-1].type,
                                MELO_TAGS_FIELDS_FULL,
                                parse[0].filter, parse[0].id,
                                parse[1].filter, parse[1].id,
                                parse[2].filter, parse[2].id,
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
  MeloLibraryFileParse parse[MELO_LIBRARY_FILE_PARSE_COUNT_MAX];
  gint count = MELO_LIBRARY_FILE_PARSE_COUNT_MAX;
  MeloLibraryFile *lfile = MELO_LIBRARY_FILE (browser);
  MeloLibraryFilePrivate *priv = lfile->priv;
  GObject *obj = G_OBJECT (browser);
  guint media_count = -1;

  /* Parse path */
  count = melo_library_file_parse (path, parse, count);
  if (count <= 0)
    return FALSE;

  /* Check media type */
  if (parse[count-1].type == MELO_FILE_DB_TYPE_SONG && parse[count-1].id)
    media_count = 1;

  /* Add media(s) to playlist */
  return melo_file_db_get_list (priv->fdb, obj, melo_library_file_add_cb,
                                browser->player, 0, media_count, params->sort,
                                FALSE, MELO_FILE_DB_TYPE_FILE,
                                MELO_TAGS_FIELDS_FULL,
                                parse[0].filter, parse[0].id,
                                parse[1].filter, parse[1].id,
                                parse[2].filter, parse[2].id,
                                MELO_FILE_DB_FIELDS_END);

  return FALSE;
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
  MeloLibraryFileParse parse[MELO_LIBRARY_FILE_PARSE_COUNT_MAX];
  gint count = MELO_LIBRARY_FILE_PARSE_COUNT_MAX;
  MeloLibraryFile *lfile = MELO_LIBRARY_FILE (browser);
  MeloLibraryFilePrivate *priv = lfile->priv;
  GObject *obj = G_OBJECT (browser);
  gboolean ret;

  /* Parse path */
  count = melo_library_file_parse (path, parse, count);
  if (count <= 0)
    return FALSE;

  /* Play first media */
  ret = melo_file_db_get_list (priv->fdb, obj, melo_library_file_play_cb,
                               browser->player, 0, 1, params->sort, FALSE,
                               MELO_FILE_DB_TYPE_FILE, MELO_TAGS_FIELDS_FULL,
                               parse[0].filter, parse[0].id,
                               parse[1].filter, parse[1].id,
                               parse[2].filter, parse[2].id,
                               MELO_FILE_DB_FIELDS_END);

  /* Add other media to playlist */
  if (parse[count-1].type != MELO_FILE_DB_TYPE_SONG || !parse[count-1].id)
    ret = melo_file_db_get_list (priv->fdb, obj, melo_library_file_add_cb,
                                 browser->player, 1, -1, params->sort, FALSE,
                                 MELO_FILE_DB_TYPE_FILE, MELO_TAGS_FIELDS_FULL,
                                 parse[0].filter, parse[0].id,
                                 parse[1].filter, parse[1].id,
                                 parse[2].filter, parse[2].id,
                                 MELO_FILE_DB_FIELDS_END);

  return ret;
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

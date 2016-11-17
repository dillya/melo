/*
 * melo_browser.h: Browser base class
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

#ifndef __MELO_BROWSER_H__
#define __MELO_BROWSER_H__

#include <glib-object.h>

#include "melo_player.h"

G_BEGIN_DECLS

#define MELO_TYPE_BROWSER             (melo_browser_get_type ())
#define MELO_BROWSER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), MELO_TYPE_BROWSER, MeloBrowser))
#define MELO_IS_BROWSER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MELO_TYPE_BROWSER))
#define MELO_BROWSER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), MELO_TYPE_BROWSER, MeloBrowserClass))
#define MELO_IS_BROWSER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), MELO_TYPE_BROWSER))
#define MELO_BROWSER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), MELO_TYPE_BROWSER, MeloBrowserClass))

typedef struct _MeloBrowser MeloBrowser;
typedef struct _MeloBrowserClass MeloBrowserClass;
typedef struct _MeloBrowserPrivate MeloBrowserPrivate;

typedef struct _MeloBrowserInfo MeloBrowserInfo;
typedef struct _MeloBrowserList MeloBrowserList;
typedef struct _MeloBrowserItem MeloBrowserItem;

typedef enum _MeloBrowserTagsMode MeloBrowserTagsMode;

struct _MeloBrowser {
  GObject parent_instance;

  /*< protected */
  MeloPlayer *player;
  MeloPlaylist *playlist;

  /*< private >*/
  MeloBrowserPrivate *priv;
};

struct _MeloBrowserClass {
  GObjectClass parent_class;

  const MeloBrowserInfo *(*get_info) (MeloBrowser *browser);
  MeloBrowserList *(*get_list) (MeloBrowser *browser, const gchar *path,
                                gint offset, gint count,
                                MeloBrowserTagsMode tags_mode,
                                MeloTagsFields tags_fields);
  MeloBrowserList *(*search) (MeloBrowser *browser, const gchar *input,
                              gint offset, gint count,
                              MeloBrowserTagsMode tags_mode,
                              MeloTagsFields tags_fields);
  gchar *(*search_hint) (MeloBrowser *browser, const gchar *input);
  MeloTags *(*get_tags) (MeloBrowser *browser, const gchar *path,
                         MeloTagsFields fields);
  gboolean (*add) (MeloBrowser *browser, const gchar *path);
  gboolean (*play) (MeloBrowser *browser, const gchar *path);
  gboolean (*remove) (MeloBrowser *browser, const gchar *path);
};

struct _MeloBrowserInfo {
  const gchar *name;
  const gchar *description;
  /* Search support */
  gboolean search_support;
  gboolean search_hint_support;
  const gchar *search_input_text;
  const gchar *search_button_text;
  /* Go support */
  gboolean go_support;
  gboolean go_list_support;
  gboolean go_play_support;
  gboolean go_add_support;
  const gchar *go_input_text;
  const gchar *go_button_list_text;
  const gchar *go_button_play_text;
  const gchar *go_button_add_text;
  /* Tags support */
  gboolean tags_support;
  gboolean tags_cache_support;
};

struct _MeloBrowserList {
  gint count;
  GList *items;
};

struct _MeloBrowserItem {
  gchar *name;
  gchar *full_name;
  gchar *type;
  gchar *add;
  gchar *remove;
  MeloTags *tags;
};

enum _MeloBrowserTagsMode {
  MELO_BROWSER_TAGS_MODE_NONE = 0,
  MELO_BROWSER_TAGS_MODE_FULL,
  MELO_BROWSER_TAGS_MODE_ONLY_CACHED,
  MELO_BROWSER_TAGS_MODE_NONE_WITH_CACHING,
  MELO_BROWSER_TAGS_MODE_FULL_WITH_CACHING,
};

GType melo_browser_get_type (void);

MeloBrowser *melo_browser_new (GType type, const gchar *id);
const gchar *melo_browser_get_id (MeloBrowser *browser);
const MeloBrowserInfo *melo_browser_get_info (MeloBrowser *browser);
MeloBrowser *melo_browser_get_browser_by_id (const gchar *id);

void melo_browser_set_player (MeloBrowser *browser, MeloPlayer *player);
MeloPlayer *melo_browser_get_player (MeloBrowser *browser);

MeloBrowserList *melo_browser_get_list (MeloBrowser *browser, const gchar *path,
                                        gint offset, gint count,
                                        MeloBrowserTagsMode tags_mode,
                                        MeloTagsFields tags_fields);
MeloBrowserList *melo_browser_search (MeloBrowser *browser, const gchar *input,
                                      gint offset, gint count,
                                      MeloBrowserTagsMode tags_mode,
                                      MeloTagsFields tags_fields);
gchar *melo_browser_search_hint (MeloBrowser *browser, const gchar *input);
MeloTags *melo_browser_get_tags (MeloBrowser *browser, const gchar *path,
                                 MeloTagsFields fields);
gboolean melo_browser_add (MeloBrowser *browser, const gchar *path);
gboolean melo_browser_play (MeloBrowser *browser, const gchar *path);
gboolean melo_browser_remove (MeloBrowser *browser, const gchar *path);

MeloBrowserList *melo_browser_list_new (void);
void melo_browser_list_free (MeloBrowserList *list);

MeloBrowserItem *melo_browser_item_new (const gchar *name, const gchar *type);
gint melo_browser_item_cmp (const MeloBrowserItem *a, const MeloBrowserItem *b);
void melo_browser_item_free (MeloBrowserItem *item);

G_END_DECLS

#endif /* __MELO_BROWSER_H__ */

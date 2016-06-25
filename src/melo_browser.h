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
typedef struct _MeloBrowserItem MeloBrowserItem;

struct _MeloBrowser {
  GObject parent_instance;

  /*< private >*/
  MeloBrowserPrivate *priv;
};

struct _MeloBrowserClass {
  GObjectClass parent_class;

  const MeloBrowserInfo *(*get_info) (MeloBrowser *browser);
  GList *(*get_list) (MeloBrowser *browser, const gchar *path);
};

struct _MeloBrowserInfo {
  const gchar *name;
  const gchar *description;
};

struct _MeloBrowserItem {
  gchar *name;
  gchar *full_name;
  gchar *type;
};

GType melo_browser_get_type (void);

MeloBrowser *melo_browser_new (GType type, const gchar *id);
const gchar *melo_browser_get_id (MeloBrowser *browser);
const MeloBrowserInfo *melo_browser_get_info (MeloBrowser *browser);
MeloBrowser *melo_browser_get_browser_by_id (const gchar *id);

GList *melo_browser_get_list (MeloBrowser *browser, const gchar *path);

MeloBrowserItem *melo_browser_item_new (const gchar *name, const gchar *type);
gint melo_browser_item_cmp (const MeloBrowserItem *a, const MeloBrowserItem *b);
void melo_browser_item_free (MeloBrowserItem *item);

/* JSON-RPC methods */
void melo_browser_register_methods (void);
void melo_browser_unregister_methods (void);

G_END_DECLS

#endif /* __MELO_BROWSER_H__ */

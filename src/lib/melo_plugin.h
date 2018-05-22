/*
 * melo_plugin.h: Plugin manager (Dynamic loader)
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

#ifndef __MELO_PLUGIN_H__
#define __MELO_PLUGIN_H__

#include <glib.h>
#include <gmodule.h>

#define MELO_API_VERSION 3

typedef struct _MeloPlugin MeloPlugin;
typedef gboolean (*MeloPluginEnable) (void);
typedef gboolean (*MeloPluginDisable) (void);

typedef struct _MeloPluginItem MeloPluginItem;

struct _MeloPlugin {
  const gchar *name;
  const gchar *description;
  MeloPluginEnable enable;
  MeloPluginDisable disable;
  guint api_version;
};

struct _MeloPluginItem {
  gchar *id;
  gchar *name;
  gchar *description;
  gboolean is_enabled;
};

gboolean melo_plugin_load (const gchar *name, gboolean enable);
gboolean melo_plugin_unload (const gchar *name);

gboolean melo_plugin_enable (const gchar *name);
gboolean melo_plugin_disable (const gchar *name);

void melo_plugin_load_all (gboolean enable);
void melo_plugin_unload_all ();

GList *melo_plugin_get_list ();
void melo_plugin_item_free (MeloPluginItem *item);

#define DECLARE_MELO_PLUGIN(_name,_description,_enable_func,_disable_func) \
  G_MODULE_EXPORT \
  const MeloPlugin melo_plugin = { \
    .name = _name, \
    .description = _description, \
    .enable = _enable_func, \
    .disable = _disable_func, \
    .api_version = MELO_API_VERSION, \
  };

#endif /* __MELO_PLUGIN_H__ */

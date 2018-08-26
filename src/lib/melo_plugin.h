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

#define MELO_API_VERSION 6

typedef struct _MeloPlugin MeloPlugin;
typedef struct _MeloPluginItem MeloPluginItem;

/**
 * MeloPluginEnable:
 *
 * The function called when a #MeloPlugin is enabled. It is the place for all
 * initialization and #MeloModule registration.
 *
 * Returns: %TRUE if the plugin has been enabled successfully, %FALSE otherwise.
 */
typedef gboolean (*MeloPluginEnable) (void);

/**
 * MeloPluginDisable:
 *
 * The function called when a #MeloPlugin is disabled. It is the place for all
 * release and #MeloModule unregistration.
 *
 * Returns: %TRUE if the plugin has been disabled successfully, %FALSE
 * otherwise.
 */
typedef gboolean (*MeloPluginDisable) (void);

/**
 * MeloPlugin:
 * @name: the display name of the plugin
 * @description: the description of the plugin
 * @enable: the enable function to use
 * @disable: the disable function to use
 * @api_version: the API version
 *
 * The #MeloPlugin describes a plugin and provides the #MeloPluginEnable and
 * the #MeloPluginDisable functions to use for enabling and disabling a plugin.
 *
 * To fill this structure, please use the DECLARE_MELO_PLUGIN() macro which
 * will set respectively all values correctly.
 */
struct _MeloPlugin {
  const gchar *name;
  const gchar *description;
  MeloPluginEnable enable;
  MeloPluginDisable disable;
  guint api_version;
};

/**
 * MeloPluginItem:
 * @id: the ID of the plugin (name used to identity it internally)
 * @name: the display name of the plugin
 * @description: the description of the plugin
 * @is_enabled: set to %TRUE if the plugin is enabled
 *
 * #MeloPluginItem is used with a #GList to provide all details of a loaded
 * plugin in Melo.
 * To free a #MeloPluginItem, please use melo_plugin_item_free().
 */
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

/**
 * DECLARE_MELO_PLUGIN:
 * @_name: the display name of the plugin
 * @_description: the description of the plugin
 * @_enable_func: the enable function to use
 * @_disable_func: the disable function to use
 *
 * Declare a new #MeloPlugin named melo_plugin with all necessary informations.
 *
 * The API version is automatically set by this macro.
 */
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

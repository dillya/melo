/*
 * melo_plugin.c: Plugin manager (Dynamic loader)
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

#include <string.h>

#include "melo_plugin.h"

#ifndef MELO_PLUGIN_PATH
#define MELO_PLUGIN_PATH "/usr/local/lib/melo"
#endif

/* Global plugin list */
G_LOCK_DEFINE_STATIC (melo_plugin_mutex);
static GList *melo_plugin_list;

typedef struct _MeloPluginContext {
  gchar *name;
  GModule *module;
  const MeloPlugin *plugin;
  gboolean is_enabled;
} MeloPluginContext;

static MeloPluginContext *
melo_plugin_find (const gchar *name)
{
  MeloPluginContext *ctx = NULL;
  GList *l;

  /* Find plugin context */
  for (l = melo_plugin_list; l != NULL; l = l->next) {
    MeloPluginContext *c = l->data;

    /* Check plugin name */
    if (!g_strcmp0 (c->name, name)) {
      ctx = c;
      break;
    }
  }

  return ctx;
}

static gboolean
melo_plugin_context_enable (MeloPluginContext *ctx)
{
  if (!ctx->plugin || !ctx->plugin->enable)
    return FALSE;

  if (ctx->is_enabled)
    return TRUE;

  if (!ctx->plugin->enable ())
    return FALSE;

  ctx->is_enabled = TRUE;
  return TRUE;
}

static gboolean
melo_plugin_load_unclock (const gchar *name, gboolean enable)
{
  MeloPluginContext *ctx;
  MeloPlugin *plugin;
  GModule *module;
  gchar *full_name;
  gchar *path;

  /* Check if module is already open */
  ctx = melo_plugin_find (name);
  if (ctx)
    return TRUE;

  /* Build plugin path */
  full_name = g_strdup_printf ("libmelo_%s", name);
  path = g_module_build_path (MELO_PLUGIN_PATH, full_name);
  g_free (full_name);

  /* Open plugin */
  module = g_module_open (path, G_MODULE_BIND_LAZY);
  g_free (path);
  if (!module)
      goto err_free;

  /* Get main symbol from plugin */
  if (!g_module_symbol (module, "melo_plugin", (gpointer *) &plugin))
      goto err_close;

  /* Add plugin to list */
  ctx = g_slice_new0 (MeloPluginContext);
  if (!ctx)
    goto err_close;
  ctx->name = g_strdup (name);
  ctx->module = module;
  ctx->plugin = plugin;
  melo_plugin_list = g_list_prepend (melo_plugin_list, ctx);

  /* Enable plugin */
  if (enable)
    melo_plugin_context_enable (ctx);

  return TRUE;
err_close:
  g_module_close (module);
err_free:
  return FALSE;
}

static gboolean
melo_plugin_context_unload (MeloPluginContext *ctx)
{
  /* Disable MeloModule */
  if (ctx->is_enabled && ctx->plugin && ctx->plugin->disable)
    ctx->plugin->disable ();

  /* Close plugin */
  return g_module_close (ctx->module);
}

gboolean
melo_plugin_load (const gchar *name, gboolean enable)
{
  gboolean ret;

  G_LOCK (melo_plugin_mutex);

  /* Load plugin */
  ret = melo_plugin_load_unclock (name, enable);

  G_UNLOCK (melo_plugin_mutex);

  return ret;
}

gboolean
melo_plugin_unload (const gchar *name)
{
  MeloPluginContext *ctx;
  gboolean ret = FALSE;

  G_LOCK (melo_plugin_mutex);

  /* Find plugin */
  ctx = melo_plugin_find (name);

  /* Unload plugin */
  if (ctx && melo_plugin_context_unload (ctx)) {
    /* Remove plugin from list */
    melo_plugin_list = g_list_remove (melo_plugin_list, ctx);
    g_free (ctx->name);
    g_slice_free (MeloPluginContext, ctx);
    ret = TRUE;
  }

  G_UNLOCK (melo_plugin_mutex);

  return ret;
}

gboolean
melo_plugin_enable (const gchar *name)
{
  MeloPluginContext *ctx;
  gboolean ret = FALSE;

  G_LOCK (melo_plugin_mutex);

  /* Find plugin */
  ctx = melo_plugin_find (name);

  /* Enable plugin */
  if (ctx)
    ret = melo_plugin_context_enable (ctx);

  G_UNLOCK (melo_plugin_mutex);

  return ret;
}

gboolean
melo_plugin_disable (const gchar *name)
{
  MeloPluginContext *ctx;
  gboolean ret = FALSE;

  G_LOCK (melo_plugin_mutex);

  /* Find plugin */
  ctx = melo_plugin_find (name);

  /* Diable plugin */
  if (ctx && ctx->is_enabled && ctx->plugin && ctx->plugin->disable) {
    ret = ctx->plugin->disable ();
    if (ret)
      ctx->is_enabled = FALSE;
  }

  G_UNLOCK (melo_plugin_mutex);

  return ret;
}

void
melo_plugin_load_all (gboolean enable)
{
  const gchar *d_name;
  gchar *name, *n;
  GDir *dir;

  G_LOCK (melo_plugin_mutex);

  /* List all plugins in directory */
  dir = g_dir_open (MELO_PLUGIN_PATH, 0, NULL);
  if (dir) {
    /* List all entries in directory exceopt . and .. */
    while ((d_name = g_dir_read_name (dir))) {
      if ((!strcmp (d_name, "..")) || !strcmp (d_name, "."))
        continue;

      /* Extract plugin name from filename */
      if (strncmp (d_name, "libmelo_", 8))
        continue;
      name = g_strdup (d_name + 8);
      n = strchr (name, '.');
      if (n)
        *n = '\0';

      /* Add plugin */
      melo_plugin_load_unclock (name, enable);
      g_free (name);
    }

    /* Close directory */
    g_dir_close (dir);
  }

  G_UNLOCK (melo_plugin_mutex);
}

void
melo_plugin_unload_all (void)
{
  MeloPluginContext *ctx;
  GList *list;

  G_LOCK (melo_plugin_mutex);

  /* Find plugin context */
  for (list = melo_plugin_list; list != NULL;) {
    GList *l = list;
    list = list->next;
    ctx = l->data;

    /* Unload plugin */
    if (melo_plugin_context_unload (ctx)) {
      /* Remove plugin from list */
      melo_plugin_list = g_list_delete_link (melo_plugin_list, l);
      g_free (ctx->name);
      g_slice_free (MeloPluginContext, ctx);
    }
  }

  G_UNLOCK (melo_plugin_mutex);
}

GList *
melo_plugin_get_list (void)
{
  MeloPluginContext *ctx;
  MeloPluginItem *item;
  GList *list = NULL, *l;

  G_LOCK (melo_plugin_mutex);

  /* Find plugin context */
  for (l = melo_plugin_list; l != NULL; l = l->next) {
    ctx = l->data;

    /* Create a new item */
    item = g_slice_new0 (MeloPluginItem);
    if (!item)
      continue;

    /* Add item to plugin list */
    item->id = g_strdup (ctx->name);
    item->is_enabled = ctx->is_enabled;
    if (ctx->plugin) {
      item->name = g_strdup (ctx->plugin->name);
      item->description = g_strdup (ctx->plugin->description);
    }
    list = g_list_prepend (list, item);
  }

  G_UNLOCK (melo_plugin_mutex);

  return list;
}

void
melo_plugin_item_free (MeloPluginItem *item)
{
  g_free (item->id);
  g_free (item->name);
  g_free (item->description);
  g_slice_free (MeloPluginItem, item);
}

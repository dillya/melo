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

#include <dirent.h>
#include <stdbool.h>
#include <string.h>

#include <gmodule.h>

#include <config.h>

#define MELO_LOG_TAG "module"
#include "melo/melo_log.h"

#include "melo/melo_module.h"

/* Default module path */
#ifndef MELO_MODULE_PATH
#define MELO_MODULE_PATH "/usr/lib/melo"
#endif

/* Default module prefix */
#ifndef MELO_MODULE_PREFIX
#define MELO_MODULE_PREFIX "libmelo_"
#endif
#define MELO_MODULE_PREFIX_LENGTH (sizeof (MELO_MODULE_PREFIX) - 1)

typedef struct _MeloModuleItem MeloModuleItem;

struct _MeloModuleItem {
  GModule *module;
  MeloModule *m;
};

/* Global module list */
static GHashTable *melo_module_list;

static bool
melo_module_load_file (const char *name)
{
  gchar *full_name, *path;
  MeloModuleItem *item;
  GModule *module;
  MeloModule *m;

  /* Invalid name */
  if (!name || !strcmp (name, "proto"))
    return false;

  /* Build file path */
  full_name = g_strconcat (MELO_MODULE_PREFIX, name, NULL);
  path = g_module_build_path (MELO_MODULE_PATH, full_name);
  g_free (full_name);

  /* Open plugin */
  module = g_module_open (path, G_MODULE_BIND_LAZY);
  g_free (path);
  if (!module)
    return false;

  /* Get main symbol from plugin */
  if (!g_module_symbol (module, MELO_MODULE_SYM_STR, (gpointer *) &m)) {
    g_module_close (module);
    return false;
  }

  /* Check module */
  if (!m->id || !m->enable_cb || !m->disable_cb) {
    MELO_LOGE ("invalid module file %s", name);
    g_module_close (module);
    return false;
  }

  /* Verify module is already registered */
  if (g_hash_table_contains (melo_module_list, m->id) == TRUE) {
    g_module_close (module);
    return false;
  }

  /* Create item */
  item = malloc (sizeof (item));
  if (!item) {
    MELO_LOGE ("failed to create module '%s' item", m->id);
    g_module_close (module);
    return false;
  }
  item->module = module;
  item->m = m;

  /* Store module reference */
  if (g_hash_table_insert (melo_module_list, (gpointer) m->id, item) == FALSE) {
    MELO_LOGE ("failed to load module '%s'", m->id);
    g_module_close (module);
    free (item);
    return false;
  }

  MELO_LOGI ("module '%s' loaded", m->id);

  /* Enable module */
  m->enable_cb ();

  return true;
}

/**
 * melo_module_load:
 *
 * Loads Melo modules.
 */
void
melo_module_load (void)
{
  struct dirent *dir;
  DIR *dirp;

  /* Create global module list */
  if (!melo_module_list) {
    melo_module_list = g_hash_table_new (g_str_hash, g_str_equal);
    if (!melo_module_list) {
      MELO_LOGW ("failed to create global module list");
      return;
    }
  }

  /* Open module directory */
  dirp = opendir (MELO_MODULE_PATH);
  if (!dirp) {
    MELO_LOGW ("failed to open " MELO_MODULE_PATH);
    return;
  }

  /* List all melo modules */
  while ((dir = readdir (dirp)) != NULL) {
    char *name, *n;
    /* Check only regular file */
    if (dir->d_type != DT_REG)
      continue;

    /* Check file start with module prefix */
    if (strncmp (dir->d_name, MELO_MODULE_PREFIX, MELO_MODULE_PREFIX_LENGTH))
      continue;

    /* Extract library name */
    name = dir->d_name + MELO_MODULE_PREFIX_LENGTH;
    n = strchr (name, '.');
    if (n)
      *n = '\0';

    /* Load module */
    melo_module_load_file (name);
  }

  /* Close directory */
  if (closedir (dirp))
    MELO_LOGW ("failed to close " MELO_MODULE_PATH);
}

static gboolean
melo_module_unload_cb (gpointer key, gpointer value, gpointer user_data)
{
  MeloModuleItem *item = value;

  /* Disable module */
  item->m->disable_cb ();

  MELO_LOGI ("module '%s' unloaded", item->m->id);

  /* Close module */
  g_module_close (item->module);

  /* Free module item */
  free (item);

  return true;
}

/**
 * melo_module_unload:
 *
 * Unloads Melo modules.
 */
void
melo_module_unload (void)
{
  /* No module loaded */
  if (!melo_module_list)
    return;

  /* Unload each module */
  g_hash_table_foreach_steal (melo_module_list, melo_module_unload_cb, NULL);

  /* Destroy module list */
  g_hash_table_destroy (melo_module_list);
  melo_module_list = NULL;
}

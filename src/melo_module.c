/*
 * melo_module.c: Module base class
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

#include "melo_module.h"

/* Internal module list */
G_LOCK_DEFINE_STATIC (melo_module_mutex);
static GHashTable *melo_modules_hash = NULL;

G_DEFINE_ABSTRACT_TYPE (MeloModule, melo_module, G_TYPE_OBJECT)

static void
melo_module_class_init (MeloModuleClass *klass)
{
}

static void
melo_module_init (MeloModule *self)
{
}

gboolean
melo_module_register (GType type, const gchar *name)
{
  gchar *module_name;
  MeloModule *mod;

  g_return_val_if_fail (g_type_is_a (type, MELO_TYPE_MODULE), FALSE);

  /* Lock module list */
  G_LOCK (melo_module_mutex);

  /* Create module list */
  if (!melo_modules_hash)
    melo_modules_hash = g_hash_table_new_full (g_str_hash, g_str_equal,
                                               g_free, g_object_unref);

  /* Check if module is already registered */
  if (g_hash_table_lookup (melo_modules_hash, name))
    goto failed;

  /* Create a new instance of module */
  mod = g_object_new (type, NULL);
  if (!mod)
    goto failed;

  /* Copy module name */
  module_name = g_strdup (name);

  /* Add module instance to modules list */
  g_hash_table_insert (melo_modules_hash, module_name, mod);

  /* Unlock module list */
  G_UNLOCK (melo_module_mutex);

  return TRUE;

failed:
  G_UNLOCK (melo_module_mutex);
  return FALSE;
}

void
melo_module_unregister (const gchar *name)
{
  /* Lock module list */
  G_LOCK (melo_module_mutex);

  /* Remove module from hash table */
  g_hash_table_remove (melo_modules_hash, name);

  /* Module list is empty */
  if (!g_hash_table_size (melo_modules_hash)) {
    /* Free hash table */
    g_hash_table_unref (melo_modules_hash);
    melo_modules_hash = NULL;
  }

  /* Unlock module list */
  G_UNLOCK (melo_module_mutex);
}

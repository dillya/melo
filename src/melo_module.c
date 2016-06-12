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
static GList *melo_modules_list = NULL;

struct _MeloModulePrivate {
  gchar *id;
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (MeloModule, melo_module, G_TYPE_OBJECT)

static void
melo_module_finalize (GObject *gobject)
{
  MeloModulePrivate *priv = melo_module_get_instance_private (
                                                         MELO_MODULE (gobject));

  if (priv->id);
    g_free (priv->id);

  /* Chain up to the parent class */
  G_OBJECT_CLASS (melo_module_parent_class)->finalize (gobject);
}

static void
melo_module_class_init (MeloModuleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  /* Add custom finalize() function */
  object_class->finalize = melo_module_finalize;
}

static void
melo_module_init (MeloModule *self)
{
  MeloModulePrivate *priv = melo_module_get_instance_private (self);

  self->priv = priv;
  priv->id = NULL;
}

static void
melo_module_set_id (MeloModule *self, const gchar *id)
{
  if (self->priv->id)
    g_free (self->priv->id);
  self->priv->id = g_strdup (id);
}

const gchar *
melo_module_get_id (MeloModule *module)
{
  return module->priv->id;
}

const MeloModuleInfo *
melo_module_get_info (MeloModule *module)
{
  MeloModuleClass *mclass = MELO_MODULE_GET_CLASS (module);

  if (!mclass->get_info)
    return NULL;

  return mclass->get_info (module);
}

/* Register a new module */
gboolean
melo_module_register (GType type, const gchar *id)
{
  MeloModule *mod;

  g_return_val_if_fail (g_type_is_a (type, MELO_TYPE_MODULE), FALSE);

  /* Lock module list */
  G_LOCK (melo_module_mutex);

  /* Create module list */
  if (!melo_modules_hash)
    melo_modules_hash = g_hash_table_new_full (g_str_hash, g_str_equal,
                                               g_free, g_object_unref);

  /* Check if module is already registered */
  if (g_hash_table_lookup (melo_modules_hash, id))
    goto failed;

  /* Create a new instance of module */
  mod = g_object_new (type, NULL);
  if (!mod)
    goto failed;

  /* Set ID to module */
  melo_module_set_id (mod, id);

  /* Add module instance to modules list */
  g_hash_table_insert (melo_modules_hash, g_strdup (id), mod);
  melo_modules_list = g_list_append (melo_modules_list, mod);

  /* Unlock module list */
  G_UNLOCK (melo_module_mutex);

  return TRUE;

failed:
  G_UNLOCK (melo_module_mutex);
  return FALSE;
}

void
melo_module_unregister (const gchar *id)
{
  MeloModule *mod;

  /* Lock module list */
  G_LOCK (melo_module_mutex);

  /* Find module in hash table */
  mod = g_hash_table_lookup (melo_modules_hash, id);
  if (!mod)
    goto unlock;

  /* Remove module from list */
  melo_modules_list = g_list_remove (melo_modules_list, id);
  g_hash_table_remove (melo_modules_hash, id);

  /* Module list is empty */
  if (!g_hash_table_size (melo_modules_hash)) {
    /* Free hash table */
    g_hash_table_unref (melo_modules_hash);
    melo_modules_hash = NULL;
  }

unlock:
  /* Unlock module list */
  G_UNLOCK (melo_module_mutex);
}

GList *
melo_module_get_module_list (void)
{
  GList *list;

  /* Lock module list */
  G_LOCK (melo_module_mutex);

  /* Copy list */
  list = g_list_copy_deep (melo_modules_list, (GCopyFunc) g_object_ref, NULL);

  /* Unlock module list */
  G_UNLOCK (melo_module_mutex);

  return list;
}

MeloModule *
melo_module_get_module_by_id (const gchar *id)
{
  MeloModule *mod;

  /* Lock module list */
  G_LOCK (melo_module_mutex);

  /* Get module by id */
  mod = g_hash_table_lookup (melo_modules_hash, id);

  /* Increment ref count */
  if (mod)
    g_object_ref (mod);

  /* Unlock module list */
  G_UNLOCK (melo_module_mutex);

  return mod;
}

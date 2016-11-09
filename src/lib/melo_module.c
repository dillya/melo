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

  /* Browser list */
  GMutex browser_mutex;
  GList *browser_list;

  /* Player list */
  GMutex player_mutex;
  GList *player_list;
};

enum {
  PROP_0,
  PROP_ID,
  PROP_LAST
};

static void melo_module_set_property (GObject *object, guint property_id,
                                      const GValue *value, GParamSpec *pspec);
static void melo_module_get_property (GObject *object, guint property_id,
                                      GValue *value, GParamSpec *pspec);

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (MeloModule, melo_module, G_TYPE_OBJECT)

static void
melo_module_finalize (GObject *gobject)
{
  MeloModulePrivate *priv = melo_module_get_instance_private (
                                                         MELO_MODULE (gobject));

  if (priv->id)
    g_free (priv->id);

  /* Free player list */
  g_list_free_full (priv->player_list, g_object_unref);
  g_mutex_clear (&priv->player_mutex);

  /* Free browser list */
  g_list_free_full (priv->browser_list, g_object_unref);
  g_mutex_clear (&priv->browser_mutex);

  /* Chain up to the parent class */
  G_OBJECT_CLASS (melo_module_parent_class)->finalize (gobject);
}

static void
melo_module_class_init (MeloModuleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  /* Add custom finalize() function */
  object_class->finalize = melo_module_finalize;
  object_class->set_property = melo_module_set_property;
  object_class->get_property = melo_module_get_property;

  /* Install ID property */
  g_object_class_install_property (object_class, PROP_ID,
      g_param_spec_string ("id", "ID", "Module ID", NULL,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                           G_PARAM_STATIC_STRINGS));
}

static void
melo_module_init (MeloModule *self)
{
  MeloModulePrivate *priv = melo_module_get_instance_private (self);

  self->priv = priv;
  priv->id = NULL;

  /* Init browser list */
  g_mutex_init (&priv->browser_mutex);
  priv->browser_list = NULL;

  /* Init player list */
  g_mutex_init (&priv->player_mutex);
  priv->player_list = NULL;
}

const gchar *
melo_module_get_id (MeloModule *module)
{
  return module->priv->id;
}

static void
melo_module_set_property (GObject *object, guint property_id,
                          const GValue *value, GParamSpec *pspec)
{
  MeloModule *module = MELO_MODULE (object);

  switch (property_id)
    {
    case PROP_ID:
      g_free (module->priv->id);
      module->priv->id = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
melo_module_get_property (GObject *object, guint property_id, GValue *value,
                          GParamSpec *pspec)
{
  MeloModule *module = MELO_MODULE (object);

  switch (property_id)
    {
    case PROP_ID:
      g_value_set_string (value, melo_module_get_id (module));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

const MeloModuleInfo *
melo_module_get_info (MeloModule *module)
{
  MeloModuleClass *mclass = MELO_MODULE_GET_CLASS (module);

  if (!mclass->get_info)
    return NULL;

  return mclass->get_info (module);
}

/* Register a browser into module */
gboolean
melo_module_register_browser (MeloModule *module, MeloBrowser *browser)
{
  MeloModulePrivate *priv = module->priv;

  /* Lock browser list */
  g_mutex_lock (&priv->browser_mutex);

  /* Check if browser is already registered */
  if (g_list_find (priv->browser_list, browser))
    goto failed;

  /* Add to browser list */
  priv->browser_list = g_list_append (priv->browser_list,
                                      g_object_ref (browser));

  /* Unlock browser list */
  g_mutex_unlock (&priv->browser_mutex);

  return TRUE;

failed:
  g_mutex_unlock (&priv->browser_mutex);
  return FALSE;
}

void
melo_module_unregister_browser (MeloModule *module, const char *id)
{
  MeloModulePrivate *priv = module->priv;
  MeloBrowser *bro;

  /* Lock browser list */
  g_mutex_lock (&priv->browser_mutex);

  /* Find browser with its id */
  bro = melo_browser_get_browser_by_id (id);
  if (!bro)
    goto unlock;

  /* Remove browser from list */
  priv->browser_list = g_list_remove (priv->browser_list, bro);
  g_object_unref (bro);
  g_object_unref (bro);

unlock:
  /* Unlock browser list */
  g_mutex_unlock (&priv->browser_mutex);
}

GList *
melo_module_get_browser_list (MeloModule *module)
{
  MeloModulePrivate *priv = module->priv;
  GList *list;

  /* Lock browser list */
  g_mutex_lock (&priv->browser_mutex);

  /* Copy list */
  list = g_list_copy_deep (priv->browser_list, (GCopyFunc) g_object_ref, NULL);

  /* Unlock browser list */
  g_mutex_unlock (&priv->browser_mutex);

  return list;
}

/* Register a player into module */
gboolean
melo_module_register_player (MeloModule *module, MeloPlayer *player)
{
  MeloModulePrivate *priv = module->priv;

  /* Lock player list */
  g_mutex_lock (&priv->player_mutex);

  /* Check if player is already registered */
  if (g_list_find (priv->player_list, player))
    goto failed;

  /* Add to player list */
  priv->player_list = g_list_append (priv->player_list,
                                      g_object_ref (player));

  /* Unlock player list */
  g_mutex_unlock (&priv->player_mutex);

  return TRUE;

failed:
  g_mutex_unlock (&priv->player_mutex);
  return FALSE;
}

void
melo_module_unregister_player (MeloModule *module, const char *id)
{
  MeloModulePrivate *priv = module->priv;
  MeloPlayer *play;

  /* Lock player list */
  g_mutex_lock (&priv->player_mutex);

  /* Find player with its id */
  play = melo_player_get_player_by_id (id);
  if (!play)
    goto unlock;

  /* Remove player from list */
  priv->player_list = g_list_remove (priv->player_list, play);
  g_object_unref (play);
  g_object_unref (play);

unlock:
  /* Unlock player list */
  g_mutex_unlock (&priv->player_mutex);
}

GList *
melo_module_get_player_list (MeloModule *module)
{
  MeloModulePrivate *priv = module->priv;
  GList *list;

  /* Lock player list */
  g_mutex_lock (&priv->player_mutex);

  /* Copy list */
  list = g_list_copy_deep (priv->player_list, (GCopyFunc) g_object_ref, NULL);

  /* Unlock player list */
  g_mutex_unlock (&priv->player_mutex);

  return list;
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
  mod = g_object_new (type, "id", id, NULL);
  if (!mod)
    goto failed;

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
  melo_modules_list = g_list_remove (melo_modules_list, mod);
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

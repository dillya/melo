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

/**
 * SECTION:melo_module
 * @title: MeloModule
 * @short_description: Base class for Melo Module
 *
 * #MeloModule is the main object used in Melo to bring new functionalities to
 * Melo. A #MeloModule can handle one or more #MeloBrowser and #MeloPlayer to
 * bring browsing and/or playing capabilities for a specific service and/or
 * protocol.
 *
 * A new #MeloModule is instance is added to Melo with melo_module_register()
 * and it can be removed (and freed) with melo_module_unregister().
 * Each #MeloModule instance is associated to an unique ID which can be used to
 * retrieve it with melo_module_get_module_by_id().
 *
 * In the #MeloModule implementation, a #MeloBrowser instance can be attached
 * with melo_module_register_browser() and a #MeloPlayer instance can be
 * attached with melo_module_register_player(). The #MeloModule takes a
 * reference on the #MeloBrowser / #MeloPlayer instance, so when calling
 * melo_module_unregister_browser() or melo_module_unregister_player() the
 * reference is only decremented for the instance.
 *
 * A list of #MeloBrowser / #MeloPlayer attached instance can be retrieved
 * respectively with melo_module_get_browser_list() and
 * melo_module_get_player_list().
 *
 * Each #MeloModule must defines a #MeloModuleInfo which provides the details
 * about the #MeloModule instance (its name, description, capabilities, ...).
 * This structure is retrieved through the melo_module_get_info() which must be
 * implemented in #MeloModule derived class.
 *
 * A #MeloModule instance comes always with a default directory where to store
 * specific files for persistence. The path can be retrieved with
 * melo_module_build_path().
 *
 * A #MeloConfig can be attached to the #MeloModule instance in defining the @id
 * field of the #MeloModuleInfo defined.
 */

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

enum {
  REGISTER_BROWSER,
  UNREGISTER_BROWSER,
  REGISTER_PLAYER,
  UNREGISTER_PLAYER,
  LAST_SIGNAL
};

static guint melo_module_signals[LAST_SIGNAL];

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

  /**
   * MeloModule:id:
   *
   * The ID of the module. This must be set during the construct and it can
   * be only read after instantiation.
   */
  g_object_class_install_property (object_class, PROP_ID,
      g_param_spec_string ("id", "ID", "Module ID", NULL,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                           G_PARAM_STATIC_STRINGS));

  /**
   * MeloModule::register-browser:
   * @module: the module
   * @browser: the #MeloBrowser which has been registered
   *
   * Will be emitted after the browser was attached to the module.
   */
  melo_module_signals[REGISTER_BROWSER] =
    g_signal_new ("register-browser", G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 1,
                  MELO_TYPE_BROWSER);

  /**
   * MeloModule::unregister-browser:
   * @module: the module
   * @browser: the #MeloBrowser which has been unregistered
   *
   * Will be emitted after the browser was detached from the module and before
   * being unrefed.
   */
  melo_module_signals[UNREGISTER_BROWSER] =
    g_signal_new ("unregister-browser", G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 1,
                  MELO_TYPE_BROWSER);

  /**
   * MeloModule::register-player:
   * @module: the module
   * @player: the #MeloPlayer which has been registered
   *
   * Will be emitted after the player was attached to the module.
   */
  melo_module_signals[REGISTER_PLAYER] =
    g_signal_new ("register-player", G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 1,
                  MELO_TYPE_PLAYER);
  /**
   * MeloModule::unregister-player:
   * @module: the module
   * @player: the #MeloPlayer which has been unregistered
   *
   * Will be emitted after the player was detached from the module and before
   * being unrefed.
   */
  melo_module_signals[UNREGISTER_PLAYER] =
    g_signal_new ("unregister-player", G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 1,
                  MELO_TYPE_PLAYER);
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

/**
 * melo_module_get_id:
 * @module: the module
 *
 * Get the #MeloModule ID.
 *
 * Returns: the ID of the #MeloModule.
 */
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

  switch (property_id) {
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

  switch (property_id) {
    case PROP_ID:
      g_value_set_string (value, melo_module_get_id (module));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

/**
 * melo_module_get_info:
 * @module: the module
 *
 * Get the details of the #MeloModule.
 *
 * Returns: a #MeloModuleInfo or %NULL if get_info callback is not defined.
 */
const MeloModuleInfo *
melo_module_get_info (MeloModule *module)
{
  MeloModuleClass *mclass = MELO_MODULE_GET_CLASS (module);

  if (!mclass->get_info)
    return NULL;

  return mclass->get_info (module);
}

/**
 * melo_module_register_browser:
 * @module: the module
 * @browser: the #MeloBrowser to attach
 *
 * The #MeloBrowser is attached to the #MeloModule and its instance will be
 * listed with melo_module_get_browser_list().
 *
 * Returns: %TRUE if browser has been attached to the module.
 */
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

  /* Signal module for attachment */
  g_signal_emit (module, melo_module_signals[REGISTER_BROWSER], 0, browser);

  /* Unlock browser list */
  g_mutex_unlock (&priv->browser_mutex);

  return TRUE;

failed:
  g_mutex_unlock (&priv->browser_mutex);
  return FALSE;
}

/**
 * melo_module_unregister_browser:
 * @module: the module
 * @id: the ID of the #MeloBrowser to detach
 *
 * The #MeloBrowser is retrieved by its ID and removed from internal list of the
 * #MeloModule.
 */
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

  /* Signal module for remove */
  g_signal_emit (module, melo_module_signals[UNREGISTER_BROWSER], 0, bro);

  /* Remove browser from list */
  priv->browser_list = g_list_remove (priv->browser_list, bro);
  g_object_unref (bro);
  g_object_unref (bro);

unlock:
  /* Unlock browser list */
  g_mutex_unlock (&priv->browser_mutex);
}

/**
 * melo_module_get_browser_list:
 * @module: the module
 *
 * Get a #GList of all #MeloBrowser attached to the #MeloModule.
 *
 * Returns: (transfer full) (element-type MeloBrowser): a #GList of all
 * #MeloBrowser attached to the module. You must free list and its data when you
 * are done with it. You can use g_list_free_full() with g_object_unref() to do
 * this.
 */
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

/**
 * melo_module_register_player:
 * @module: the module
 * @player: the #MeloPlayer to attach
 *
 * The #MeloPlayer is attached to the #MeloModule and its instance will be
 * listed with melo_module_get_player_list().
 *
 * Returns: %TRUE if player has been attached to the module.
 */
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

  /* Signal module for attachment */
  g_signal_emit (module, melo_module_signals[REGISTER_PLAYER], 0, player);

  /* Unlock player list */
  g_mutex_unlock (&priv->player_mutex);

  return TRUE;

failed:
  g_mutex_unlock (&priv->player_mutex);
  return FALSE;
}

/**
 * melo_module_unregister_player:
 * @module: the module
 * @id: the ID of the #MeloPlayer to detach
 *
 * The #MeloPlayer is retrieved by its ID and removed from internal list of the
 * #MeloModule.
 */
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

  /* Signal module for remove */
  g_signal_emit (module, melo_module_signals[UNREGISTER_PLAYER], 0, play);

  /* Remove player from list */
  priv->player_list = g_list_remove (priv->player_list, play);
  g_object_unref (play);
  g_object_unref (play);

unlock:
  /* Unlock player list */
  g_mutex_unlock (&priv->player_mutex);
}

/**
 * melo_module_get_player_list:
 * @module: the module
 *
 * Get a #GList of all #MeloPlayer attached to the #MeloModule.
 *
 * Returns: (transfer full) (element-type MeloPlayer): a #GList of all
 * #MeloPlayer attached to the module. You must free list and its data when you
 * are done with it. You can use g_list_free_full() with g_object_unref() to do
 * this.
 */
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

/**
 * melo_module_register:
 * @type: the type ID of the #MeloModule subtype to register (and instantiate)
 * @id: the #MeloModule ID to use for the new instance
 *
 * A new #MeloModule subtype is instantiated and registered into global module
 * list of Melo. The @id provided is used to identify the #MeloModule instance
 * and can be used to retrieve it with melo_module_get_module_by_id().
 *
 * Returns: %TRUE if module has been instantiated and registered.
 */
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

/**
 * melo_module_unregister:
 * @id: the #MeloModule ID to unregister and unref
 *
 * The #MeloModule instance is retrieved with its ID and removed from global
 * module list. Then, it is unref.
 */
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

/**
 * melo_module_get_module_list:
 *
 * Get a #GList of all #MeloModule registered.
 *
 * Returns: (transfer full): a #GList of all #MeloModule registered. You must
 * free list and its data when you are done with it. You can use
 * g_list_free_full() with g_object_unref() to do this.
 */
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

/**
 * melo_module_get_module_by_id:
 * @id: the #MeloModule ID to retrieve
 *
 * Get an instance of the #MeloModule with its ID.
 *
 * Returns: (transfer full): the #MeloModule instance or %NULL if not found. Use
 * g_object_unref() after usage.
 */
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

/**
 * melo_module_build_path:
 * @module: the module
 * @file: name of the file to use
 *
 * Generate the full path to a file which will be stored in the default
 * dedicated folder of the #MeloModule.
 *
 * Returns: (transfer full): the full path of the file. The string must be freed
 * after usage with g_free().
 */
gchar *
melo_module_build_path (MeloModule *module, const gchar *file)
{
  return g_strdup_printf ("%s/melo/%s/%s", g_get_user_data_dir (),
                          module->priv->id, file);
}

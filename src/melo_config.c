/*
 * melo_config.c: Configuration class
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

#include "melo_config.h"

/* Internal config list */
G_LOCK_DEFINE_STATIC (melo_config_mutex);
static GHashTable *melo_config_hash = NULL;
static GList *melo_config_list = NULL;

static const gchar *melo_config_types[MELO_CONFIG_TYPE_COUNT] = {
  [MELO_CONFIG_TYPE_NONE] = "none",
  [MELO_CONFIG_TYPE_BOOLEAN] = "boolean",
  [MELO_CONFIG_TYPE_INTEGER] = "integer",
  [MELO_CONFIG_TYPE_DOUBLE] = "double",
  [MELO_CONFIG_TYPE_STRING] = "string",
};

static const gchar *melo_config_elements[MELO_CONFIG_ELEMENT_COUNT] = {
  [MELO_CONFIG_ELEMENT_NONE] = "none",
  [MELO_CONFIG_ELEMENT_CHECKBOX] = "checkbox",
  [MELO_CONFIG_ELEMENT_NUMBER] = "number",
  [MELO_CONFIG_ELEMENT_TEXT] = "text",
  [MELO_CONFIG_ELEMENT_PASSWORD] = "password",
};

struct _MeloConfigPrivate {
  gchar *id;

  const MeloConfigGroup *groups;
  gint groups_count;
  GHashTable *ids;

  GMutex mutex;
  MeloConfigValues *values;
  gsize values_size;
};

G_DEFINE_TYPE_WITH_PRIVATE (MeloConfig, melo_config, G_TYPE_OBJECT)

static void
melo_config_finalize (GObject *gobject)
{
  MeloConfig *config = MELO_CONFIG (gobject);
  MeloConfigPrivate *priv = melo_config_get_instance_private (config);
  gint i;

  /* Lock config list */
  G_LOCK (melo_config_mutex);

  /* Remove object from config list */
  melo_config_list = g_list_remove (melo_config_list, config);
  g_hash_table_remove (melo_config_hash, priv->id);

  /* Unlock config list */
  G_UNLOCK (melo_config_mutex);

  /* Clear mutex */
  g_mutex_clear (&priv->mutex);

  /* Free all values for each groups */
  for (i = 0; i < priv->groups_count; i++) {
    g_slice_free1 (priv->values[i].size, priv->values[i].values);
    g_hash_table_unref (priv->values[i].ids);
  }

  /* Free values */
  g_slice_free1 (priv->values_size, priv->values);

  /* Free group IDs hash table */
  g_hash_table_unref (priv->ids);

  /* Free ID */
  g_free (priv->id);

  /* Chain up to the parent class */
  G_OBJECT_CLASS (melo_config_parent_class)->finalize (gobject);
}

static void
melo_config_class_init (MeloConfigClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  /* Add custom finalize() function */
  object_class->finalize = melo_config_finalize;
}

static void
melo_config_init (MeloConfig *self)
{
  MeloConfigPrivate *priv = melo_config_get_instance_private (self);

  self->priv = priv;
  priv->id = NULL;
}

MeloConfig *
melo_config_new (const gchar *id, const MeloConfigGroup *groups,
                 gint groups_count)
{
  MeloConfigPrivate *priv;
  MeloConfig *config;
  gint i, j;

  g_return_val_if_fail (id && groups, NULL);

  /* Lock config list */
  G_LOCK (melo_config_mutex);

  /* Create config list */
  if (!melo_config_hash)
    melo_config_hash = g_hash_table_new (g_str_hash, g_str_equal);

  /* Check if name is already used */
  if (g_hash_table_lookup (melo_config_hash, id))
    goto failed;

  /* Create new instance */
  config = g_object_new (MELO_TYPE_CONFIG, NULL);
  if (!config)
    goto failed;

  /* Fill */
  priv = config->priv;
  priv->id = g_strdup (id);

  /* Init mutex */
  g_mutex_init (&priv->mutex);

  /* Copy groups */
  priv->groups = groups;
  priv->groups_count = groups_count;

  /* Allocate values table */
  priv->values_size = groups_count * sizeof (MeloConfigValues);
  priv->values = g_slice_alloc (priv->values_size);

  /* Init all values for each groups */
  priv->ids = g_hash_table_new (g_str_hash, g_str_equal);
  for (i = 0; i < groups_count; i++) {
    g_hash_table_insert (priv->ids, groups[i].id, GINT_TO_POINTER (i));

    priv->values[i].ids = g_hash_table_new (g_str_hash, g_str_equal);
    priv->values[i].size = groups[i].items_count * sizeof (MeloConfigValue);
    priv->values[i].values = g_slice_alloc0 (priv->values[i].size);

    /* Fill hash table with ids */
    for (j = 0; j < groups[i].items_count; j++) {
      if (!groups[i].items[j].id)
        continue;
      g_hash_table_insert (priv->values[i].ids, groups[i].items[j].id,
                           GINT_TO_POINTER (j));
    }
  }

  /* Add new config instance to config list */
  g_hash_table_insert (melo_config_hash, priv->id, config);
  melo_config_list = g_list_append (melo_config_list, config);

  /* Unlock config list */
  G_UNLOCK (melo_config_mutex);

  return config;

failed:
  G_UNLOCK (melo_config_mutex);
  return NULL;
}

MeloConfig *
melo_config_get_config_by_id (const gchar *id)
{
  MeloConfig *cfg;

  /* Lock config list */
  G_LOCK (melo_config_mutex);

  /* Get config by id */
  cfg = g_hash_table_lookup (melo_config_hash, id);

  /* Increment ref count */
  if (cfg)
    g_object_ref (cfg);

  /* Unlock config list */
  G_UNLOCK (melo_config_mutex);

  return cfg;
}

const gchar *
melo_config_type_to_string (MeloConfigType type)
{
  return melo_config_types[type];
}

const gchar *
melo_config_element_to_string (MeloConfigElement element)
{
  return melo_config_elements[element];
}

const MeloConfigGroup *
melo_config_get_groups (MeloConfig *config, gint *count)
{
  if (count)
    *count = config->priv->groups_count;
  return config->priv->groups;
}

void
melo_config_load_default (MeloConfig *config)
{
  MeloConfigValues *groups_values = config->priv->values;
  const MeloConfigGroup *groups = config->priv->groups;
  gint i, j;

  /* Lock config access */
  g_mutex_lock (&config->priv->mutex);

  /* Set default values in each groups */
  for (i = 0; i < config->priv->groups_count; i++) {
    MeloConfigValue *values = groups_values[i].values;
    MeloConfigItem *items = groups[i].items;

    /* Set default values */
    for (j = 0; j < groups[i].items_count; j++)
      values[j] = items[j].def;
  }

  /* Unlock config access */
  g_mutex_unlock (&config->priv->mutex);
}

gboolean
melo_config_load_from_file (MeloConfig *config, const gchar *filename)
{
  MeloConfigValues *groups_values = config->priv->values;
  const MeloConfigGroup *groups = config->priv->groups;
  GKeyFile *kfile;
  gint i, j;

  /* Load file */
  kfile = g_key_file_new ();
  if (!g_key_file_load_from_file (kfile, filename, 0, NULL)) {
    g_key_file_unref (kfile);
    return FALSE;
  }

  /* Lock config access */
  g_mutex_lock (&config->priv->mutex);

  /* Load values in each groups */
  for (i = 0; i < config->priv->groups_count; i++) {
    MeloConfigValue *values = groups_values[i].values;
    MeloConfigItem *items = groups[i].items;
    const gchar *gid = groups[i].id;

    /* Set default values */
    for (j = 0; j < groups[i].items_count; j++) {
      const gchar *id = items[j].id;

      switch (items[j].type) {
        case MELO_CONFIG_TYPE_BOOLEAN:
          values[j]._boolean = g_key_file_get_boolean (kfile, gid, id, NULL);
          break;
        case MELO_CONFIG_TYPE_INTEGER:
          values[j]._integer = g_key_file_get_int64 (kfile, gid, id, NULL);
          break;
        case MELO_CONFIG_TYPE_DOUBLE:
          values[j]._double = g_key_file_get_double (kfile, gid, id, NULL);
          break;
        case MELO_CONFIG_TYPE_STRING:
          g_free (values[j]._string);
          values[j]._string = g_key_file_get_string (kfile, gid, id, NULL);
          break;
        default:
          values[j]._integer = 0;
      }
    }
  }

  /* Unlock config access */
  g_mutex_unlock (&config->priv->mutex);

  /* Close file */
  g_key_file_unref (kfile);

  return TRUE;
}

gboolean
melo_config_save_to_file (MeloConfig *config, const gchar *filename)
{
  MeloConfigValues *groups_values = config->priv->values;
  const MeloConfigGroup *groups = config->priv->groups;
  gboolean ret = FALSE;
  GKeyFile *kfile;
  gchar *path;
  gint i, j;

  /* Load file */
  kfile = g_key_file_new ();

  /* Lock config access */
  g_mutex_lock (&config->priv->mutex);

  /* Load values in each groups */
  for (i = 0; i < config->priv->groups_count; i++) {
    MeloConfigValue *values = groups_values[i].values;
    MeloConfigItem *items = groups[i].items;
    const gchar *gid = groups[i].id;

    /* Set default values */
    for (j = 0; j < groups[i].items_count; j++) {
      const gchar *id = items[j].id;

      switch (items[j].type) {
        case MELO_CONFIG_TYPE_BOOLEAN:
          g_key_file_set_boolean (kfile, gid, id, values[j]._boolean);
          break;
        case MELO_CONFIG_TYPE_INTEGER:
          g_key_file_set_integer (kfile, gid, id, values[j]._integer);
          break;
        case MELO_CONFIG_TYPE_DOUBLE:
          g_key_file_set_double (kfile, gid, id, values[j]._double);
          break;
        case MELO_CONFIG_TYPE_STRING:
          g_key_file_set_string (kfile, gid, id, values[j]._string);
          break;
        default:
          ;
      }
    }
  }

  /* Unlock config access */
  g_mutex_unlock (&config->priv->mutex);

  /* Save to file (create direcory if necessary) */
  path = g_path_get_dirname (filename);
  if (!g_mkdir_with_parents (path, 0700))
    ret = g_key_file_save_to_file (kfile, filename, NULL);
  g_free (path);

  /* Close file */
  g_key_file_unref (kfile);

  return ret;
}

static inline gboolean
melo_config_find_item (MeloConfigPrivate *priv, const gchar *group,
                       const gchar *id, gint *group_idx, gint *item_idx)
{
  /* Find group index */
  if (!g_hash_table_lookup_extended (priv->ids, group,
                                     NULL, (gpointer *) group_idx))
    return FALSE;

  /* Find item index */
  if (!g_hash_table_lookup_extended (priv->values[*group_idx].ids, id,
                                     NULL, (gpointer *) item_idx))
    return FALSE;

  return TRUE;
}

static gboolean
melo_config_get_value (MeloConfig *config, const gchar *group, const gchar *id,
                       MeloConfigType type, gpointer value)
{
  MeloConfigPrivate *priv = config->priv;
  gboolean ret = TRUE;
  gint g, i;

  /* Get indexes */
  if (!melo_config_find_item (priv, group, id, &g, &i))
    return FALSE;

  /* Check value type */
  if (priv->groups[g].items[i].type != type)
    return FALSE;

  /* Lock config access */
  g_mutex_lock (&priv->mutex);

  /* Copy value */
  switch (type) {
    case MELO_CONFIG_TYPE_BOOLEAN:
      *((gboolean *)value) = priv->values[g].values[i]._boolean;
      break;
    case MELO_CONFIG_TYPE_INTEGER:
      *((gint64 *)value) = priv->values[g].values[i]._integer;
      break;
    case MELO_CONFIG_TYPE_DOUBLE:
      *((gdouble *)value) = priv->values[g].values[i]._double;
      break;
    case MELO_CONFIG_TYPE_STRING:
      *((gchar **)value) = g_strdup (priv->values[g].values[i]._string);
      break;
    default:
      ret = FALSE;
  }

  /* Unlock config access */
  g_mutex_unlock (&priv->mutex);

  return ret;
}

gboolean
melo_config_get_boolean (MeloConfig *config, const gchar *group,
                         const gchar *id, gboolean *value)
{
  return melo_config_get_value (config, group, id,
                                MELO_CONFIG_TYPE_BOOLEAN, value);
}

gboolean
melo_config_get_integer (MeloConfig *config, const gchar *group,
                         const gchar *id, gint64 *value)
{
  return melo_config_get_value (config, group, id,
                                MELO_CONFIG_TYPE_INTEGER, value);
}

gboolean
melo_config_get_double (MeloConfig *config, const gchar *group,
                        const gchar *id, gdouble *value)
{
  return melo_config_get_value (config, group, id,
                                MELO_CONFIG_TYPE_DOUBLE, value);
}

gboolean
melo_config_get_string (MeloConfig *config, const gchar *group,
                        const gchar *id, gchar **value)
{
  return melo_config_get_value (config, group, id,
                                MELO_CONFIG_TYPE_STRING, value);
}

/* Advanced functions */
gpointer
melo_config_read_all (MeloConfig *config, MeloConfigFunc callback,
                      gpointer user_data)
{
  MeloConfigPrivate *priv = config->priv;
  gpointer ret = NULL;

  /* Lock config access */
  g_mutex_lock (&priv->mutex);

  /* Call the callback */
  if (callback)
    ret = callback (priv->groups, priv->groups_count, priv->values, user_data);

  /* Unlock config access */
  g_mutex_unlock (&priv->mutex);

  return ret;
}

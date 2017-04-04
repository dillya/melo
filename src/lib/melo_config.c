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

#include <string.h>

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

typedef struct _MeloConfigValues {
  GHashTable *ids;
  MeloConfigValue *values;
  MeloConfigValue *new_values;
  gboolean *updated_values;
  gsize bsize;
  gsize size;

  MeloConfigCheckFunc check_cb;
  gpointer check_data;

  MeloConfigUpdateFunc update_cb;
  gpointer update_data;
} MeloConfigValues;

struct _MeloConfigPrivate {
  gchar *id;

  const MeloConfigGroup *groups;
  gint groups_count;
  GHashTable *ids;

  GMutex mutex;
  MeloConfigValues *values;
  gsize values_size;
  gboolean save_to_def;
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
    int j;

    /* Free string values */
    for (j = 0; j < priv->groups[i].items_count; j++) {
      if (priv->groups[i].items[j].type == MELO_CONFIG_TYPE_STRING)
        g_free (priv->values[i].values[j]._string);
    }

    /* Free values arrays */
    g_slice_free1 (priv->values[i].bsize, priv->values[i].updated_values);
    g_slice_free1 (priv->values[i].size, priv->values[i].new_values);
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
  priv->values = g_slice_alloc0 (priv->values_size);

  /* Init all values for each groups */
  priv->ids = g_hash_table_new (g_str_hash, g_str_equal);
  for (i = 0; i < groups_count; i++) {
    g_hash_table_insert (priv->ids, groups[i].id, GINT_TO_POINTER (i));

    priv->values[i].ids = g_hash_table_new (g_str_hash, g_str_equal);
    priv->values[i].size = groups[i].items_count * sizeof (MeloConfigValue);
    priv->values[i].bsize = groups[i].items_count * sizeof (gboolean);
    priv->values[i].values = g_slice_alloc0 (priv->values[i].size);
    priv->values[i].new_values = g_slice_alloc0 (priv->values[i].size);
    priv->values[i].updated_values = g_slice_alloc0 (priv->values[i].bsize);

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
      if (items[j].type == MELO_CONFIG_TYPE_STRING)
        values[j]._string = g_strdup (items[j].def._string);
      else
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
    GError *err = NULL;

    /* Set default values */
    for (j = 0; j < groups[i].items_count; j++) {
      const gchar *id = items[j].id;

      switch (items[j].type) {
        case MELO_CONFIG_TYPE_BOOLEAN:
          values[j]._boolean = g_key_file_get_boolean (kfile, gid, id, &err);
          if (err)
            values[j]._boolean = items[j].def._boolean;
          break;
        case MELO_CONFIG_TYPE_INTEGER:
          values[j]._integer = g_key_file_get_int64 (kfile, gid, id, &err);
          if (err)
            values[j]._integer = items[j].def._integer;
          break;
        case MELO_CONFIG_TYPE_DOUBLE:
          values[j]._double = g_key_file_get_double (kfile, gid, id, &err);
          if (err)
            values[j]._double = items[j].def._double;
          break;
        case MELO_CONFIG_TYPE_STRING:
          g_free (values[j]._string);
          values[j]._string = g_key_file_get_string (kfile, gid, id, &err);
          if (err)
            values[j]._string = g_strdup (items[j].def._string);
          break;
        default:
          values[j]._integer = 0;
      }
      g_clear_error (&err);
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

      /* Do not save this item */
      if (items[j].flags & MELO_CONFIG_FLAGS_DONT_SAVE)
        continue;

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
          if (values[j]._string)
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

static inline gchar *
melo_config_get_def_file (MeloConfig *config)
{
  return g_strdup_printf ("%s/melo/%s.cfg", g_get_user_config_dir (),
                          config->priv->id);
}

gboolean
melo_config_load_from_def_file (MeloConfig *config)
{
  gchar *filename;
  gboolean ret;

  /* Load from default config file */
  filename = melo_config_get_def_file (config);
  ret = melo_config_load_from_file (config, filename);
  g_free (filename);

  return ret;
}

gboolean
melo_config_save_to_def_file (MeloConfig *config)
{
  gchar *filename;
  gboolean ret;

  /* Save to default config file */
  filename = melo_config_get_def_file (config);
  ret = melo_config_save_to_file (config, filename);
  g_free (filename);

  return ret;
}

void
melo_config_save_to_def_file_at_update (MeloConfig *config, gboolean save)
{
  config->priv->save_to_def = save;
}

static inline gboolean
melo_config_find (MeloConfigPrivate *priv, const gchar *group, const gchar *id,
                  gint *group_idx, gint *item_idx)
{
  gpointer p;

  /* Find group index */
  if (!g_hash_table_lookup_extended (priv->ids, group, NULL, &p))
    return FALSE;
  *group_idx = GPOINTER_TO_INT (p);

  /* Find only the group */
  if (!id)
    return TRUE;

  /* Find item index */
  if (!g_hash_table_lookup_extended (priv->values[*group_idx].ids, id, NULL, &p))
    return FALSE;
  *item_idx = GPOINTER_TO_INT (p);

  return TRUE;
}

static gboolean
melo_config_get_value (MeloConfig *config, const gchar *group, const gchar *id,
                       MeloConfigType type, gpointer value)
{
  MeloConfigPrivate *priv = config->priv;
  gboolean ret = TRUE;
  gint g, i = 0;

  /* Get indexes */
  if (!melo_config_find (priv, group, id, &g, &i))
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

static gboolean
melo_config_set_value (MeloConfig *config, const gchar *group, const gchar *id,
                       MeloConfigType type, gpointer value)
{
  MeloConfigPrivate *priv = config->priv;
  gboolean ret = TRUE;
  gint g, i = 0;

  /* Get indexes */
  if (!melo_config_find (priv, group, id, &g, &i))
    return FALSE;

  /* Check value type */
  if (priv->groups[g].items[i].type != type)
    return FALSE;

  /* Lock config access */
  g_mutex_lock (&priv->mutex);

  /* Copy value */
  switch (type) {
    case MELO_CONFIG_TYPE_BOOLEAN:
      priv->values[g].values[i]._boolean = (gboolean) GPOINTER_TO_INT (value);
      break;
    case MELO_CONFIG_TYPE_INTEGER:
      priv->values[g].values[i]._integer = (gint64) GPOINTER_TO_INT (value);
      break;
    case MELO_CONFIG_TYPE_DOUBLE:
      priv->values[g].values[i]._double = (gdouble) GPOINTER_TO_INT (value);
      break;
    case MELO_CONFIG_TYPE_STRING:
      g_free (priv->values[g].values[i]._string);
      priv->values[g].values[i]._string = g_strdup ((gchar *) value);
      break;
    default:
      ret = FALSE;
  }

  /* Unlock config access */
  g_mutex_unlock (&priv->mutex);

  return ret;
}

gboolean
melo_config_set_boolean (MeloConfig *config, const gchar *group,
                         const gchar *id, gboolean value)
{
  return melo_config_set_value (config, group, id, MELO_CONFIG_TYPE_BOOLEAN,
                                GINT_TO_POINTER (value));
}

gboolean
melo_config_set_integer (MeloConfig *config, const gchar *group,
                         const gchar *id, gint64 value)
{
  return melo_config_set_value (config, group, id, MELO_CONFIG_TYPE_INTEGER,
                                GINT_TO_POINTER (value));
}

gboolean
melo_config_set_double (MeloConfig *config, const gchar *group,
                        const gchar *id, gdouble value)
{
  return melo_config_set_value (config, group, id, MELO_CONFIG_TYPE_DOUBLE,
                                GINT_TO_POINTER (value));
}

gboolean
melo_config_set_string (MeloConfig *config, const gchar *group,
                        const gchar *id, const gchar *value)
{
  return melo_config_set_value (config, group, id, MELO_CONFIG_TYPE_STRING,
                                (gpointer) value);
}

void
melo_config_set_check_callback (MeloConfig *config, const gchar *group,
                                MeloConfigCheckFunc callback,
                                gpointer user_data)
{
  gint idx;

  /* Find group */
  if (!melo_config_find (config->priv, group, NULL, &idx, NULL))
    return;

  /* Lock config access */
  g_mutex_lock (&config->priv->mutex);

  /* Set callback */
  config->priv->values[idx].check_cb = callback;
  config->priv->values[idx].check_data = user_data;

  /* Unlock config access */
  g_mutex_unlock (&config->priv->mutex);
}

void
melo_config_set_update_callback (MeloConfig *config, const gchar *group,
                                 MeloConfigUpdateFunc callback,
                                 gpointer user_data)
{
  gint idx;

  /* Find group */
  if (!melo_config_find (config->priv, group, NULL, &idx, NULL))
    return;

  /* Lock config access */
  g_mutex_lock (&config->priv->mutex);

  /* Set callback */
  config->priv->values[idx].update_cb = callback;
  config->priv->values[idx].update_data = user_data;

  /* Unlock config access */
  g_mutex_unlock (&config->priv->mutex);
}

/* Advanced functions */
struct _MeloConfigContext {
  MeloConfigPrivate *priv;
  gint group_idx;
  gint item_idx;
  const MeloConfigGroup *group;
  const MeloConfigValues *values;
  MeloConfigType type;

  gboolean update;
  MeloConfigValue *new_value;
  gboolean *updated_value;
};

gpointer
melo_config_parse (MeloConfig *config, MeloConfigFunc callback,
                   gpointer user_data)
{
  MeloConfigPrivate *priv = config->priv;
  MeloConfigContext context = {
    .priv = priv,
    .group_idx = 0,
  };
  gpointer ret = NULL;

  /* Lock config access */
  g_mutex_lock (&priv->mutex);

  /* Call the callback */
  if (callback)
    ret = callback (&context, user_data);

  /* Unlock config access */
  g_mutex_unlock (&priv->mutex);

  return ret;
}

gboolean
melo_config_update (MeloConfig *config, MeloConfigUpFunc callback,
                    gpointer user_data, gchar **error)
{
  MeloConfigPrivate *priv = config->priv;
  MeloConfigContext context = {
    .priv = priv,
    .group_idx = 0,
    .update = TRUE,
  };
  gint i, j;

  /* Lock config access */
  g_mutex_lock (&priv->mutex);

  /* Reset updated values */
  for (i = 0; i < priv->groups_count; i++) {
    memset (priv->values[i].new_values, 0, priv->values[i].size);
    memset (priv->values[i].updated_values, 0, priv->values[i].bsize);
  }

  /* Call the callback */
  if (callback && !callback (&context, user_data, error))
    goto failed;

  /* Check new values */
  context.group_idx = 0;
  while (melo_config_next_group (&context, NULL, NULL)) {
    if (context.values->check_cb &&
        !context.values->check_cb (&context, context.values->check_data, error))
      goto failed;
  }

  /* Copy updated values */
  context.group_idx = 0;
  while (melo_config_next_group (&context, NULL, NULL)) {
    /* Call update callback */
    if (context.values->update_cb)
      context.values->update_cb (&context, context.values->update_data);

    /* Copy all updated values */
    for (j = 0; j < context.group->items_count; j++) {
      if (context.values->updated_values[j]) {
        if (context.group->items[j].type == MELO_CONFIG_TYPE_STRING)
          g_free (context.values->values[j]._string);
        context.values->values[j] = context.values->new_values[j];
      }
    }
  }

  /* Unlock config access */
  g_mutex_unlock (&priv->mutex);

  /* Save to default file */
  if (priv->save_to_def)
    melo_config_save_to_def_file (config);

  return TRUE;
failed:
  g_mutex_unlock (&priv->mutex);
  return FALSE;
}

gint
melo_config_get_group_count (MeloConfigContext *context)
{
  return context->priv->groups_count;
}

gboolean
melo_config_next_group (MeloConfigContext *context,
                        const MeloConfigGroup **group, gint *items_count)
{
  MeloConfigPrivate *priv = context->priv;

  /* End of group list */
  if (context->group_idx >= priv->groups_count)
    return FALSE;

  /* Update context */
  context->group = &priv->groups[context->group_idx];
  context->values = &priv->values[context->group_idx];
  context->group_idx++;
  context->item_idx = 0;
  if (context->update) {
    context->new_value = &context->values->new_values[0];
    context->updated_value = &context->values->updated_values[0];
    context->type = context->group->items[0].type;
  }

  /* Set values */
  if (group)
    *group = context->group;
  if (items_count)
    *items_count = context->group->items_count;

  return TRUE;
}

gboolean
melo_config_next_item (MeloConfigContext *context, MeloConfigItem **item,
                       MeloConfigValue *value)
{
  /* End of group list */
  if (context->item_idx >= context->group->items_count)
    return FALSE;

  /* Set values */
  if (item)
    *item = &context->group->items[context->item_idx];
  if (value)
    *value = context->values->values[context->item_idx];

  /* Set update context */
  if (context->update) {
    context->new_value = &context->values->new_values[context->item_idx];
    context->updated_value = &context->values->updated_values[context->item_idx];
    context->type = context->group->items[context->item_idx].type;
  }

  /* Update context */
  context->item_idx++;

  return TRUE;
}

gboolean
melo_config_find_group (MeloConfigContext *context, const gchar *group_id,
                        const MeloConfigGroup **group, gint *items_count)
{
  MeloConfigPrivate *priv = context->priv;
  gpointer p;
  gint idx;

  /* Find group */
  if (!g_hash_table_lookup_extended (priv->ids, group_id, NULL, &p))
    return FALSE;
  idx = GPOINTER_TO_INT (p);

  /* Update context */
  context->group = &priv->groups[idx];
  context->values = &priv->values[idx];
  context->group_idx = idx + 1;
  context->item_idx = 0;
  if (context->update) {
    context->new_value = &context->values->new_values[0];
    context->updated_value = &context->values->updated_values[0];
    context->type = context->group->items[0].type;
  }

  /* Set values */
  if (group)
    *group = context->group;
  if (items_count)
    *items_count = context->group->items_count;

  return TRUE;
}

gboolean
melo_config_find_item (MeloConfigContext *context, const gchar *item_id,
                       MeloConfigItem **item, MeloConfigValue *value)
{
  gpointer p;
  gint idx;

  /* Find item */
  if (!g_hash_table_lookup_extended (context->values->ids, item_id, NULL, &p))
    return FALSE;
  idx = GPOINTER_TO_INT (p);

  /* Set values */
  if (item)
    *item = &context->group->items[idx];
  if (value)
    *value = context->values->values[idx];

  /* Set update context */
  if (context->update) {
    context->new_value = &context->values->new_values[idx];
    context->updated_value = &context->values->updated_values[idx];
    context->type = context->group->items[idx].type;
  }

  /* Update context */
  context->item_idx = idx + 1;

  return TRUE;
}

void
melo_config_update_boolean (MeloConfigContext *context, gboolean value)
{
  if (!context->update)
    return;
  context->new_value->_boolean = value;
  *context->updated_value = TRUE;
}

void
melo_config_update_integer (MeloConfigContext *context, gint64 value)
{
  if (!context->update)
    return;
  context->new_value->_integer = value;
  *context->updated_value = TRUE;
}

void
melo_config_update_double (MeloConfigContext *context, gdouble value)
{
  if (!context->update)
    return;
  context->new_value->_double = value;
  *context->updated_value = TRUE;
}

void
melo_config_update_string (MeloConfigContext *context, const gchar *value)
{
  if (!context->update)
    return;
  g_free (context->new_value->_string);
  context->new_value->_string = g_strdup (value);
  *context->updated_value = TRUE;
}

void
melo_config_remove_update (MeloConfigContext *context)
{
  if (context->type == MELO_CONFIG_TYPE_STRING)
    g_clear_pointer (&context->new_value->_string, g_free);
  *context->updated_value = FALSE;
}

#define DEFINE_GET_UPDATED(_type, _field) \
gboolean \
melo_config_get_updated_##_field (MeloConfigContext *context, const gchar *id, \
                                 _type *value, _type *old_value) \
{ \
  MeloConfigValue val; \
 \
  if (!context->update || !melo_config_find_item (context, id, NULL, &val) || \
      !*context->updated_value) \
    return FALSE; \
 \
  if (old_value) \
    *old_value = val._##_field; \
  *value = context->new_value->_ ##_field; \
  return TRUE; \
}

DEFINE_GET_UPDATED(gboolean, boolean)
DEFINE_GET_UPDATED(gint64, integer)
DEFINE_GET_UPDATED(gdouble, double)
DEFINE_GET_UPDATED(const gchar *, string)

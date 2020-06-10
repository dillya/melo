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

#include <inttypes.h>
#include <stdio.h>

#define MELO_LOG_TAG "settings"
#include "melo/melo_log.h"

#include "melo/melo_settings.h"

#include "settings.pb-c.h"

/* Global settings list */
G_LOCK_DEFINE_STATIC (melo_settings_mutex);
static GHashTable *melo_settings_list;

typedef enum {
  MELO_SETTINGS_TYPE_BOOLEAN = 0,
  MELO_SETTINGS_TYPE_INT32,
  MELO_SETTINGS_TYPE_UINT32,
  MELO_SETTINGS_TYPE_INT64,
  MELO_SETTINGS_TYPE_UINT64,
  MELO_SETTINGS_TYPE_FLOAT,
  MELO_SETTINGS_TYPE_DOUBLE,
  MELO_SETTINGS_TYPE_STRING,
} MeloSettingsType;

typedef union {
  bool _boolean;
  int32_t _int32;
  uint32_t _uint32;
  int64_t _int64;
  uint64_t _uint64;
  float _float;
  double _double;
  char *_string;
} MeloSettingsValue;

struct _MeloSettingsEntry {
  const char *id;
  const char *name;
  const char *description;
  MeloSettingsType type;
  MeloSettingsValue value;
  MeloSettingsValue new_value;
  MeloSettingsValue default_value;
  const MeloSettingsEntry *depends;
  unsigned int flags;
};

struct _MeloSettingsGroup {
  const char *id;
  const char *name;
  const char *description;

  /* Entries */
  GList *entries;
  GList *last_entry;
  unsigned int entry_count;

  /* Update callback */
  MeloSettingsUpdateCb cb;
  void *user_data;
};

struct _MeloSettings {
  /* Parent instance */
  GObject parent_instance;

  /* Settings ID */
  char *id;
  char *filename;

  /* Group list */
  GList *groups;
  GList *last_group;
  unsigned int group_count;
};

G_DEFINE_TYPE (MeloSettings, melo_settings, G_TYPE_OBJECT)

static void
melo_settings_finalize (GObject *gobject)
{
  MeloSettings *self = MELO_SETTINGS (gobject);

  /* Free filename */
  g_free (self->filename);

  /* Release groups */
  while (self->groups) {
    GList *g = self->groups;
    MeloSettingsGroup *group = g->data;

    /* Release entries */
    while (group->entries) {
      GList *e = group->entries;
      MeloSettingsEntry *entry = e->data;

      /* Free string */
      if (entry->type == MELO_SETTINGS_TYPE_STRING &&
          !(entry->flags & MELO_SETTINGS_FLAG_READ_ONLY))
        g_free (entry->value._string);

      /* Free entry */
      g_slice_free (MeloSettingsEntry, entry);
      group->entries = g_list_delete_link (group->entries, e);
    }

    /* Free group */
    g_slice_free (MeloSettingsGroup, group);
    self->groups = g_list_delete_link (self->groups, g);
  }

  /* Un-register settings */
  if (self->id) {
    /* Lock settings list */
    G_LOCK (melo_settings_mutex);

    /* Remove settings from list */
    if (melo_settings_list)
      g_hash_table_remove (melo_settings_list, self->id);

    /* Destroy settings list */
    if (melo_settings_list && !g_hash_table_size (melo_settings_list)) {
      g_hash_table_destroy (melo_settings_list);
      melo_settings_list = NULL;
    }

    /* Unlock settings list */
    G_UNLOCK (melo_settings_mutex);

    /* Free ID */
    g_free (self->id);
  }

  /* Chain up to the parent class */
  G_OBJECT_CLASS (melo_settings_parent_class)->finalize (gobject);
}

static void
melo_settings_class_init (MeloSettingsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  /* Attach finalize function */
  object_class->finalize = melo_settings_finalize;
}

static void
melo_settings_init (MeloSettings *self)
{
}

/**
 * melo_settings_new:
 * @id: (nullable): the unique settings ID
 *
 * Create a new Settings object.
 *
 * If @id is %NULL, the settings won't be accessible through requests and the
 * settings won't be saved to the file system.
 *
 * Returns: (transfer full): the new #MeloSettings.
 */
MeloSettings *
melo_settings_new (const char *id)
{
  MeloSettings *settings;

  /* Create new object */
  settings = g_object_new (MELO_TYPE_SETTINGS, NULL);
  if (settings && id) {
    char *path;

    /* Set ID */
    settings->id = g_strdup (id);

    /* Lock settings list */
    G_LOCK (melo_settings_mutex);

    /* Create settings list */
    if (!melo_settings_list)
      melo_settings_list = g_hash_table_new (g_str_hash, g_str_equal);

    /* Insert settings into list */
    if (melo_settings_list &&
        g_hash_table_contains (melo_settings_list, settings->id) == FALSE) {
      g_hash_table_insert (
          melo_settings_list, (gpointer) settings->id, settings);
    }

    /* Unlock settings list */
    G_UNLOCK (melo_settings_mutex);

    /* Create configuration path */
    path = g_build_filename (g_get_user_config_dir (), "melo", NULL);
    g_mkdir_with_parents (path, 0700);

    /* Create configuration filename */
    settings->filename = g_build_filename (path, settings->id, NULL);
    g_free (path);
  }

  return settings;
}

/**
 * melo_settings_add_group:
 * @settings: a #MeloSettings instance
 * @id: the unique group ID
 * @name: the name to display
 * @description: (nullable): the group description
 * @cb: (optional): the function to call when settings have been updated and
 *     should be validated
 * @user_data: the data to pass to @cb
 *
 * This function can be used to register a new group to @settings. The callback
 * is called only when the settings are updated through a request processed by
 * melo_settings_handle_request().
 *
 * This function should be called after settings creation and before call to
 * melo_settings_load().
 *
 * Returns: (transfer none): the new #MeloSettingsGroup group. The instance is
 *     valid until the settings object is destroyed.
 */
MeloSettingsGroup *
melo_settings_add_group (MeloSettings *settings, const char *id,
    const char *name, const char *description, MeloSettingsUpdateCb cb,
    void *user_data)
{
  MeloSettingsGroup *group;

  if (!settings || !name)
    return NULL;

  /* Create new group */
  group = g_slice_new0 (MeloSettingsGroup);
  if (!group) {
    MELO_LOGE ("failed to create group %s", name);
    return NULL;
  }

  /* Set group */
  group->id = id;
  group->name = name;
  group->description = description;

  /* Set callbacks */
  group->cb = cb;
  group->user_data = user_data;

  /* Add group to list */
  settings->groups = g_list_prepend (settings->groups, group);
  if (!settings->last_group)
    settings->last_group = settings->groups;
  settings->group_count++;

  return group;
}

static MeloSettingsEntry *
melo_settings_new_entry (MeloSettingsGroup *group, const char *id,
    const char *name, const char *description, const MeloSettingsEntry *depends,
    unsigned int flags)
{
  MeloSettingsEntry *entry;

  if (!group || !id || !name)
    return NULL;

  /* Create new entry */
  entry = g_slice_new0 (MeloSettingsEntry);
  if (!entry) {
    MELO_LOGE ("failed to create entry %s in %s", name, group->name);
    return NULL;
  }

  /* Set entry */
  entry->id = id;
  entry->name = name;
  entry->description = description;
  entry->depends = depends;
  entry->flags = flags;

  /* Add entry to list */
  group->entries = g_list_prepend (group->entries, entry);
  if (!group->last_entry)
    group->last_entry = group->entries;
  group->entry_count++;

  return entry;
}

/**
 * melo_settings_find_group:
 * @settings: a #MeloSettings instance
 * @id: the ID of the group to find
 *
 * This function will search for a #MeloSettingsGroup by its ID.
 *
 * Returns: (transfer none): a borrowed reference to a #MeloSettingsGroup or
 *     %NULL if not found.
 */
MeloSettingsGroup *
melo_settings_find_group (MeloSettings *settings, const char *id)
{
  GList *l;

  if (!settings || !id)
    return NULL;

  /* Find group by ID */
  for (l = settings->groups; l != NULL; l = l->next) {
    MeloSettingsGroup *group = l->data;

    /* Group found */
    if (!strcmp (group->id, id))
      return group;
  }

  return NULL;
}

/**
 * melo_settings_group_find_entry:
 * @group: a #MeloSettingsGroup instance
 * @id: the ID of the entry to find
 *
 * This function will search for a #MeloSettingsEntry by its ID.
 *
 * Returns: (transfer none): a borrowed reference to a #MeloSettingsEntry or
 *     %NULL if not found.
 */
MeloSettingsEntry *
melo_settings_group_find_entry (MeloSettingsGroup *group, const char *id)
{
  GList *l;

  if (!group || !id)
    return NULL;

  /* Find entry by ID */
  for (l = group->entries; l != NULL; l = l->next) {
    MeloSettingsEntry *entry = l->data;

    /* Entry found */
    if (!strcmp (entry->id, id))
      return entry;
  }

  return NULL;
}

#define MELO_SETTINGS_FUNC(_TYPE, _type, _ctype) \
  MeloSettingsEntry *melo_settings_group_add_##_type ( \
      MeloSettingsGroup *group, const char *id, const char *name, \
      const char *description, _ctype default_value, \
      const MeloSettingsEntry *depends, unsigned int flags) \
  { \
    MeloSettingsEntry *entry; \
\
    entry = melo_settings_new_entry ( \
        group, id, name, description, depends, flags); \
    if (entry) { \
      entry->value._##_type = default_value; \
      entry->new_value._##_type = default_value; \
      entry->default_value._##_type = default_value; \
      entry->type = MELO_SETTINGS_TYPE_##_TYPE; \
    } \
\
    return entry; \
  } \
\
  bool melo_settings_entry_get_##_type ( \
      MeloSettingsEntry *entry, _ctype *value, _ctype *old_value) \
  { \
    if (!entry || entry->type != MELO_SETTINGS_TYPE_##_TYPE) \
      return false; \
\
    if (value) \
      *value = entry->new_value._##_type; \
    if (old_value) \
      *old_value = entry->value._##_type; \
\
    return true; \
  } \
\
  bool melo_settings_entry_set_##_type ( \
      MeloSettingsEntry *entry, _ctype value) \
  { \
    if (!entry || entry->type != MELO_SETTINGS_TYPE_##_TYPE) \
      return false; \
\
    entry->value._##_type = value; \
    return true; \
  }

/**
 * melo_settings_group_add_boolean:
 * @group: a #MeloSettingsGroup instance
 * @id: the unique ID of the entry
 * @name: the name to display for the entry
 * @description: (nullable): the description of the entry
 * @default_value: the default value
 * @depends: (nullable): a #MeloSettingsEntry instance
 * @flags: a combination of #MeloSettingsFlag
 *
 * This function can be used to register a new boolean entry into @group. The
 * @depends parameter can be used to specify on which this entry depends on: it
 * should be a boolean entry.
 *
 * This function should be called after settings creation and before call to
 * melo_settings_load().
 *
 * Returns: (transfer none): a borrowed reference to the newly created
 *     #MeloSettingsEntry. It stays valid until group or settings is destroyed.
 */
/**
 * melo_settings_entry_get_boolean:
 * @entry: a #MeloSettingsEntry instance
 * @value: (out) (transfer none) (optional): the new value to validate
 * @old_value: (out) (transfer none) (optional): the current value
 *
 * This function should be used only in a #MeloSettingsUpdateCb implementation,
 * to validate a group values change.
 *
 * Returns: %true if the entry has been found, %false otherwise.
 */
/**
 * melo_settings_entry_set_boolean:
 * @entry: a #MeloSettingsEntry instance
 * @value: (transfer none): the new value to set
 *
 * This function can be used to set a new boolean value to @entry. The
 * registered #MeloSettingsUpdateCb callback is not called.
 *
 * Returns: %true if the entry has been found and set, %false otherwise.
 */
MELO_SETTINGS_FUNC (BOOLEAN, boolean, bool)

/**
 * melo_settings_group_add_int32:
 * @group: a #MeloSettingsGroup instance
 * @id: the unique ID of the entry
 * @name: the name to display for the entry
 * @description: (nullable): the description of the entry
 * @default_value: the default value
 * @depends: (nullable): a #MeloSettingsEntry instance
 * @flags: a combination of #MeloSettingsFlag
 *
 * This function can be used to register a new int32 entry into @group. The
 * @depends parameter can be used to specify on which this entry depends on: it
 * should be a int32 entry.
 *
 * This function should be called after settings creation and before call to
 * melo_settings_load().
 *
 * Returns: (transfer none): a borrowed reference to the newly created
 *     #MeloSettingsEntry. It stays valid until group or settings is destroyed.
 */
/**
 * melo_settings_entry_get_int32:
 * @entry: a #MeloSettingsEntry instance
 * @value: (out) (transfer none) (optional): the new value to validate
 * @old_value: (out) (transfer none) (optional): the current value
 *
 * This function should be used only in a #MeloSettingsUpdateCb implementation,
 * to validate a group values change.
 *
 * Returns: %true if the entry has been found, %false otherwise.
 */
/**
 * melo_settings_entry_set_int32:
 * @entry: a #MeloSettingsEntry instance
 * @value: (transfer none): the new value to set
 *
 * This function can be used to set a new int32 value to @entry. The
 * registered #MeloSettingsUpdateCb callback is not called.
 *
 * Returns: %true if the entry has been found and set, %false otherwise.
 */
MELO_SETTINGS_FUNC (INT32, int32, int32_t)

/**
 * melo_settings_group_add_uint32:
 * @group: a #MeloSettingsGroup instance
 * @id: the unique ID of the entry
 * @name: the name to display for the entry
 * @description: (nullable): the description of the entry
 * @default_value: the default value
 * @depends: (nullable): a #MeloSettingsEntry instance
 * @flags: a combination of #MeloSettingsFlag
 *
 * This function can be used to register a new uint32 entry into @group. The
 * @depends parameter can be used to specify on which this entry depends on: it
 * should be a uint32 entry.
 *
 * This function should be called after settings creation and before call to
 * melo_settings_load().
 *
 * Returns: (transfer none): a borrowed reference to the newly created
 *     #MeloSettingsEntry. It stays valid until group or settings is destroyed.
 */
/**
 * melo_settings_entry_get_uint32:
 * @entry: a #MeloSettingsEntry instance
 * @value: (out) (transfer none) (optional): the new value to validate
 * @old_value: (out) (transfer none) (optional): the current value
 *
 * This function should be used only in a #MeloSettingsUpdateCb implementation,
 * to validate a group values change.
 *
 * Returns: %true if the entry has been found, %false otherwise.
 */
/**
 * melo_settings_entry_set_uint32:
 * @entry: a #MeloSettingsEntry instance
 * @value: (transfer none): the new value to set
 *
 * This function can be used to set a new uint32 value to @entry. The
 * registered #MeloSettingsUpdateCb callback is not called.
 *
 * Returns: %true if the entry has been found and set, %false otherwise.
 */
MELO_SETTINGS_FUNC (UINT32, uint32, uint32_t)

/**
 * melo_settings_group_add_int64:
 * @group: a #MeloSettingsGroup instance
 * @id: the unique ID of the entry
 * @name: the name to display for the entry
 * @description: (nullable): the description of the entry
 * @default_value: the default value
 * @depends: (nullable): a #MeloSettingsEntry instance
 * @flags: a combination of #MeloSettingsFlag
 *
 * This function can be used to register a new int64 entry into @group. The
 * @depends parameter can be used to specify on which this entry depends on: it
 * should be a int64 entry.
 *
 * This function should be called after settings creation and before call to
 * melo_settings_load().
 *
 * Returns: (transfer none): a borrowed reference to the newly created
 *     #MeloSettingsEntry. It stays valid until group or settings is destroyed.
 */
/**
 * melo_settings_entry_get_int64:
 * @entry: a #MeloSettingsEntry instance
 * @value: (out) (transfer none) (optional): the new value to validate
 * @old_value: (out) (transfer none) (optional): the current value
 *
 * This function should be used only in a #MeloSettingsUpdateCb implementation,
 * to validate a group values change.
 *
 * Returns: %true if the entry has been found, %false otherwise.
 */
/**
 * melo_settings_entry_set_int64:
 * @entry: a #MeloSettingsEntry instance
 * @value: (transfer none): the new value to set
 *
 * This function can be used to set a new int64 value to @entry. The
 * registered #MeloSettingsUpdateCb callback is not called.
 *
 * Returns: %true if the entry has been found and set, %false otherwise.
 */
MELO_SETTINGS_FUNC (INT64, int64, int64_t)

/**
 * melo_settings_group_add_uint64:
 * @group: a #MeloSettingsGroup instance
 * @id: the unique ID of the entry
 * @name: the name to display for the entry
 * @description: (nullable): the description of the entry
 * @default_value: the default value
 * @depends: (nullable): a #MeloSettingsEntry instance
 * @flags: a combination of #MeloSettingsFlag
 *
 * This function can be used to register a new uint64 entry into @group. The
 * @depends parameter can be used to specify on which this entry depends on: it
 * should be a uint64 entry.
 *
 * This function should be called after settings creation and before call to
 * melo_settings_load().
 *
 * Returns: (transfer none): a borrowed reference to the newly created
 *     #MeloSettingsEntry. It stays valid until group or settings is destroyed.
 */
/**
 * melo_settings_entry_get_uint64:
 * @entry: a #MeloSettingsEntry instance
 * @value: (out) (transfer none) (optional): the new value to validate
 * @old_value: (out) (transfer none) (optional): the current value
 *
 * This function should be used only in a #MeloSettingsUpdateCb implementation,
 * to validate a group values change.
 *
 * Returns: %true if the entry has been found, %false otherwise.
 */
/**
 * melo_settings_entry_set_uint64:
 * @entry: a #MeloSettingsEntry instance
 * @value: (transfer none): the new value to set
 *
 * This function can be used to set a new uint64 value to @entry. The
 * registered #MeloSettingsUpdateCb callback is not called.
 *
 * Returns: %true if the entry has been found and set, %false otherwise.
 */
MELO_SETTINGS_FUNC (UINT64, uint64, uint64_t)

/**
 * melo_settings_group_add_float:
 * @group: a #MeloSettingsGroup instance
 * @id: the unique ID of the entry
 * @name: the name to display for the entry
 * @description: (nullable): the description of the entry
 * @default_value: the default value
 * @depends: (nullable): a #MeloSettingsEntry instance
 * @flags: a combination of #MeloSettingsFlag
 *
 * This function can be used to register a new float entry into @group. The
 * @depends parameter can be used to specify on which this entry depends on: it
 * should be a float entry.
 *
 * This function should be called after settings creation and before call to
 * melo_settings_load().
 *
 * Returns: (transfer none): a borrowed reference to the newly created
 *     #MeloSettingsEntry. It stays valid until group or settings is destroyed.
 */
/**
 * melo_settings_entry_get_float:
 * @entry: a #MeloSettingsEntry instance
 * @value: (out) (transfer none) (optional): the new value to validate
 * @old_value: (out) (transfer none) (optional): the current value
 *
 * This function should be used only in a #MeloSettingsUpdateCb implementation,
 * to validate a group values change.
 *
 * Returns: %true if the entry has been found, %false otherwise.
 */
/**
 * melo_settings_entry_set_float:
 * @entry: a #MeloSettingsEntry instance
 * @value: (transfer none): the new value to set
 *
 * This function can be used to set a new float value to @entry. The
 * registered #MeloSettingsUpdateCb callback is not called.
 *
 * Returns: %true if the entry has been found and set, %false otherwise.
 */
MELO_SETTINGS_FUNC (FLOAT, float, float)

/**
 * melo_settings_group_add_double:
 * @group: a #MeloSettingsGroup instance
 * @id: the unique ID of the entry
 * @name: the name to display for the entry
 * @description: (nullable): the description of the entry
 * @default_value: the default value
 * @depends: (nullable): a #MeloSettingsEntry instance
 * @flags: a combination of #MeloSettingsFlag
 *
 * This function can be used to register a new double entry into @group. The
 * @depends parameter can be used to specify on which this entry depends on: it
 * should be a double entry.
 *
 * This function should be called after settings creation and before call to
 * melo_settings_load().
 *
 * Returns: (transfer none): a borrowed reference to the newly created
 *     #MeloSettingsEntry. It stays valid until group or settings is destroyed.
 */
/**
 * melo_settings_entry_get_double:
 * @entry: a #MeloSettingsEntry instance
 * @value: (out) (transfer none) (optional): the new value to validate
 * @old_value: (out) (transfer none) (optional): the current value
 *
 * This function should be used only in a #MeloSettingsUpdateCb implementation,
 * to validate a group values change.
 *
 * Returns: %true if the entry has been found, %false otherwise.
 */
/**
 * melo_settings_entry_set_double:
 * @entry: a #MeloSettingsEntry instance
 * @value: (transfer none): the new value to set
 *
 * This function can be used to set a new double value to @entry. The
 * registered #MeloSettingsUpdateCb callback is not called.
 *
 * Returns: %true if the entry has been found and set, %false otherwise.
 */
MELO_SETTINGS_FUNC (DOUBLE, double, double)

/**
 * melo_settings_group_add_string:
 * @group: a #MeloSettingsGroup instance
 * @id: the unique ID of the entry
 * @name: the name to display for the entry
 * @description: (nullable): the description of the entry
 * @default_value: the default value
 * @depends: (nullable): a #MeloSettingsEntry instance
 * @flags: a combination of #MeloSettingsFlag
 *
 * This function can be used to register a new string entry into @group. The
 * @depends parameter can be used to specify on which this entry depends on: it
 * should be a boolean entry.
 *
 * This function should be called after settings creation and before call to
 * melo_settings_load().
 *
 * Returns: (transfer none): a borrowed reference to the newly created
 *     #MeloSettingsEntry. It stays valid until group or settings is destroyed.
 */
MeloSettingsEntry *
melo_settings_group_add_string (MeloSettingsGroup *group, const char *id,
    const char *name, const char *description, const char *default_value,
    const MeloSettingsEntry *depends, unsigned int flags)
{
  MeloSettingsEntry *entry;

  /* Find entry */
  entry =
      melo_settings_new_entry (group, id, name, description, depends, flags);
  if (entry) {
    /* Initialize string */
    entry->value._string = g_strdup (default_value);
    entry->new_value._string = entry->value._string;
    entry->default_value._string = (char *) default_value;
    entry->type = MELO_SETTINGS_TYPE_STRING;
  }

  return entry;
}

/**
 * melo_settings_entry_get_string:
 * @entry: a #MeloSettingsEntry instance
 * @value: (out) (transfer none) (optional): the new value to validate
 * @old_value: (out) (transfer none) (optional): the current value
 *
 * This function should be used only in a #MeloSettingsUpdateCb implementation,
 * to validate a group values change.
 *
 * Returns: %true if the entry has been found, %false otherwise.
 */
bool
melo_settings_entry_get_string (
    MeloSettingsEntry *entry, const char **value, const char **old_value)
{
  if (!entry || entry->type != MELO_SETTINGS_TYPE_STRING)
    return false;

  /* Set values */
  if (value)
    *value = entry->new_value._string;
  if (old_value)
    *old_value = entry->value._string;

  return true;
}

/**
 * melo_settings_entry_set_string:
 * @entry: a #MeloSettingsEntry instance
 * @value: (transfer none): the new value to set
 *
 * This function can be used to set a new string value to @entry. The previous
 * string will be automatically freed. The registered #MeloSettingsUpdateCb
 * callback is not called.
 *
 * Returns: %true if the entry has been found and set, %false otherwise.
 */
bool
melo_settings_entry_set_string (MeloSettingsEntry *entry, const char *value)
{
  if (!entry || entry->type != MELO_SETTINGS_TYPE_STRING)
    return false;

  /* Change value */
  g_free (entry->value._string);
  entry->value._string = g_strdup (value);

  return true;
}

/**
 * melo_settings_load:
 * @settings: a #MeloSettings instance
 *
 * This function tries to load settings from its dedicated file. If the file
 * doesn't exist or the file is malformed, the default values will be used.
 *
 * It should be called only after all groups and entries registration are
 * finished.
 *
 * The settings are automatically saved to the file at end of this function or
 * when a setting is updated through a request.
 * If an entry has been modified with a direct setter, the settings are not
 * saved to the file. Then, the developer should call explicitly the
 * melo_settings_save() function.
 */
void
melo_settings_load (MeloSettings *settings)
{
  if (!settings)
    return;

  /* Load settings from file */
  if (settings->filename) {
    MeloSettingsGroup *group = NULL;
    FILE *fp;

    /* Open file */
    fp = fopen (settings->filename, "r");
    if (!fp)
      goto save;

    /* Read file line by line */
    while (!feof (fp)) {
      MeloSettingsEntry *entry;
      char line[256];
      char *value, *p;

      /* Read next line */
      if (!fgets (line, sizeof (line), fp))
        break;

      /* Skip comments */
      if (line[0] == ';' || line[0] == '#')
        continue;

      /* Group detected */
      if (line[0] == '[') {
        /* Find group closing */
        p = strchr (line + 1, ']');
        if (!p)
          continue;
        *p++ = '\0';

        /* Find group */
        group = melo_settings_find_group (settings, line + 1);
        continue;
      }

      /* Find delimiter */
      value = strchr (line, '=');
      if (!value)
        continue;
      *value++ = '\0';

      /* Remove new line character */
      p = strchr (value, '\n');
      if (p)
        *p = '\0';

      /* Find entry */
      entry = melo_settings_group_find_entry (group, line);
      if (!entry)
        continue;

      /* Parse value */
      switch (entry->type) {
      case MELO_SETTINGS_TYPE_BOOLEAN:
        if (!strcmp (value, "true"))
          entry->value._boolean = entry->new_value._boolean = true;
        else
          entry->value._boolean = entry->new_value._boolean = false;
        break;
      case MELO_SETTINGS_TYPE_INT32:
        entry->value._int32 = entry->new_value._int32 = strtol (value, &p, 10);
        if (value == p)
          entry->value = entry->new_value = entry->default_value;
        break;
      case MELO_SETTINGS_TYPE_UINT32:
        entry->value._uint32 = entry->new_value._uint32 =
            strtoul (value, &p, 10);
        if (value == p)
          entry->value = entry->new_value = entry->default_value;
        break;
      case MELO_SETTINGS_TYPE_INT64:
        entry->value._int64 = entry->new_value._int64 = strtoll (value, &p, 10);
        if (value == p)
          entry->value = entry->new_value = entry->default_value;
        break;
      case MELO_SETTINGS_TYPE_UINT64:
        entry->value._uint64 = entry->new_value._uint64 =
            strtoull (value, &p, 10);
        if (value == p)
          entry->value = entry->new_value = entry->default_value;
        break;
      case MELO_SETTINGS_TYPE_FLOAT:
        entry->value._float = entry->new_value._float = strtof (value, &p);
        if (value == p)
          entry->value = entry->new_value = entry->default_value;
        break;
      case MELO_SETTINGS_TYPE_DOUBLE:
        entry->value._double = entry->new_value._double = strtod (value, &p);
        if (value == p)
          entry->value = entry->new_value = entry->default_value;
        break;
      case MELO_SETTINGS_TYPE_STRING:
        g_free (entry->value._string);
        entry->value._string = entry->new_value._string = g_strdup (value);
        break;
      }
    }

    /* Close file */
    fclose (fp);
  }

save:
  /* Save settings to file */
  melo_settings_save (settings);
}

/**
 * melo_settings_save:
 * @settings: a #MeloSettings instance
 *
 * This function will save all current settings to a file. Then, on next launch,
 * the previously settings will be loaded during melo_settings_load() call.
 *
 * This function should be called only after one or more entries direct set
 * (with one of the setters): the melo_settings_load() and the
 * melo_settings_handle_request() functions will automatically save settings at
 * end of their execution.
 */
void
melo_settings_save (MeloSettings *settings)
{
  FILE *fp;
  GList *g;

  if (!settings->filename)
    return;

  /* Create new key file */
  fp = fopen (settings->filename, "w");
  if (!fp) {
    MELO_LOGE ("failed to open %s", settings->filename);
    return;
  }

  /* Parse groups */
  for (g = settings->last_group; g != NULL; g = g->prev) {
    MeloSettingsGroup *group = g->data;
    GList *e;

    /* Save group */
    fprintf (fp, "[%s]\n", group->id);

    /* Parse entries */
    for (e = group->last_entry; e != NULL; e = e->prev) {
      MeloSettingsEntry *entry = e->data;

      /* Save values */
      switch (entry->type) {
      case MELO_SETTINGS_TYPE_BOOLEAN:
        fprintf (
            fp, "%s=%s\n", entry->id, entry->value._boolean ? "true" : "false");
        break;
      case MELO_SETTINGS_TYPE_INT32:
        fprintf (fp, "%s=%" PRId32 "\n", entry->id, entry->value._int32);
        break;
      case MELO_SETTINGS_TYPE_UINT32:
        fprintf (fp, "%s=%" PRIu32 "\n", entry->id, entry->value._uint32);
        break;
      case MELO_SETTINGS_TYPE_INT64:
        fprintf (fp, "%s=%" PRId64 "\n", entry->id, entry->value._int64);
        break;
      case MELO_SETTINGS_TYPE_UINT64:
        fprintf (fp, "%s=%" PRIu64 "\n", entry->id, entry->value._uint64);
        break;
      case MELO_SETTINGS_TYPE_FLOAT:
        fprintf (fp, "%s=%f\n", entry->id, entry->value._float);
        break;
      case MELO_SETTINGS_TYPE_DOUBLE:
        fprintf (fp, "%s=%lf\n", entry->id, entry->value._double);
        break;
      case MELO_SETTINGS_TYPE_STRING:
        if (entry->value._string)
          fprintf (fp, "%s=%s\n", entry->id, entry->value._string);
        break;
      }
    }

    /* Separate groups */
    fprintf (fp, "\n");
  }

  /* Close file */
  fclose (fp);
}

static void
melo_settings_get_entry_list (MeloSettingsGroup *group, Settings__Group *g)
{
  Settings__Entry **entries_ptr;
  Settings__Entry *entries;
  unsigned int i = 0;
  GList *l;

  /* Allocate entry list */
  entries_ptr = malloc (sizeof (*entries_ptr) * group->entry_count);
  entries = malloc (sizeof (*entries) * group->entry_count);

  /* Fill entry list */
  for (l = group->last_entry; l != NULL; l = l->prev) {
    MeloSettingsEntry *entry = l->data;

    /* Skip entry */
    if (entry->flags & MELO_SETTINGS_FLAG_NO_EXPORT)
      continue;

    /* Initialize entry */
    settings__entry__init (&entries[i]);
    entries_ptr[i] = &entries[i];

    /* Set entry */
    entries[i].id = (char *) entry->id;
    entries[i].name = (char *) entry->name;
    entries[i].description = (char *) entry->description;

    /* Set flags */
    if (entry->flags & MELO_SETTINGS_FLAG_READ_ONLY)
      entries[i].read_only = true;
    if (entry->flags & MELO_SETTINGS_FLAG_PASSWORD)
      entries[i].password = true;

    /* Set value */
    switch (entry->type) {
    case MELO_SETTINGS_TYPE_BOOLEAN:
      entries[i].value_case = SETTINGS__ENTRY__VALUE_BOOLEAN;
      entries[i].boolean = entry->value._boolean;
      break;
    case MELO_SETTINGS_TYPE_INT32:
      entries[i].value_case = SETTINGS__ENTRY__VALUE_INT32;
      entries[i].int32 = entry->value._int32;
      break;
    case MELO_SETTINGS_TYPE_UINT32:
      entries[i].value_case = SETTINGS__ENTRY__VALUE_UINT32;
      entries[i].uint32 = entry->value._uint32;
      break;
    case MELO_SETTINGS_TYPE_INT64:
      entries[i].value_case = SETTINGS__ENTRY__VALUE_INT64;
      entries[i].int64 = entry->value._int64;
      break;
    case MELO_SETTINGS_TYPE_UINT64:
      entries[i].value_case = SETTINGS__ENTRY__VALUE_UINT64;
      entries[i].uint64 = entry->value._uint64;
      break;
    case MELO_SETTINGS_TYPE_FLOAT:
      entries[i].value_case = SETTINGS__ENTRY__VALUE_FLOAT;
      entries[i].float_ = entry->value._float;
      break;
    case MELO_SETTINGS_TYPE_DOUBLE:
      entries[i].value_case = SETTINGS__ENTRY__VALUE_DOUBLE;
      entries[i].double_ = entry->value._double;
      break;
    case MELO_SETTINGS_TYPE_STRING:
      entries[i].value_case = SETTINGS__ENTRY__VALUE_STRING;
      if (entry->flags & MELO_SETTINGS_FLAG_PASSWORD || !entry->value._string)
        entries[i].string = "";
      else
        entries[i].string = entry->value._string;
      break;
    default:
      break;
    }
    i++;
  }

  /* List is empty */
  if (!i) {
    free (entries_ptr);
    free (entries);
    return;
  }

  /* Set list */
  g->n_entries = i;
  g->entries = entries_ptr;
}

static bool
melo_settings_get_group_list (MeloSettings *settings, const char *group_id,
    MeloAsyncCb cb, void *user_data)
{
  Settings__Response resp = SETTINGS__RESPONSE__INIT;
  Settings__Response__GroupList list = SETTINGS__RESPONSE__GROUP_LIST__INIT;
  Settings__Group **groups_ptr, *groups;
  unsigned int count = 1, i = 0;
  MeloMessage *msg;
  GList *l;

  /* Set response */
  resp.resp_case = SETTINGS__RESPONSE__RESP_GROUP_LIST;
  resp.group_list = &list;

  /* List all groups */
  if (!group_id || group_id == protobuf_c_empty_string || *group_id == '\0') {
    count = settings->group_count;
    group_id = NULL;
  }

  /* Allocate group list */
  groups_ptr = malloc (sizeof (*groups_ptr) * count);
  groups = calloc (1, sizeof (*groups) * count);

  /* Fill list group */
  for (l = settings->last_group; l != NULL; l = l->prev) {
    MeloSettingsGroup *group = l->data;

    /* Compare group ID */
    if (group_id && strcmp (group_id, group->id))
      continue;

    /* Initialize group */
    settings__group__init (&groups[i]);
    groups_ptr[i] = &groups[i];

    /* Set group */
    groups[i].id = (char *) group->id;
    groups[i].name = (char *) group->name;
    groups[i].description = (char *) group->description;
    groups[i].entries = NULL;

    /* Set entry list */
    melo_settings_get_entry_list (group, &groups[i++]);
  }

  /* Set list */
  list.n_groups = i;
  list.groups = groups_ptr;

  /* Pack message */
  msg = melo_message_new (settings__response__get_packed_size (&resp));
  melo_message_set_size (
      msg, settings__response__pack (&resp, melo_message_get_data (msg)));

  /* Free response */
  for (i = 0; i < list.n_groups; i++) {
    if (groups[i].entries)
      free (groups[i].entries[0]);
    free (groups[i].entries);
  }
  free (groups_ptr);
  free (groups);

  /* Send response */
  if (cb)
    cb (msg, user_data);
  melo_message_unref (msg);

  return false;
}

static bool
melo_settings_set_group (MeloSettings *settings, Settings__Group *req,
    MeloAsyncCb cb, void *user_data)
{
  Settings__Response resp = SETTINGS__RESPONSE__INIT;
  Settings__Group resp_group = SETTINGS__GROUP__INIT;
  MeloSettingsGroup *group;
  MeloMessage *msg;
  unsigned int i;
  char *error = NULL;
  GList *l;

  if (!req || !req->id) {
    MELO_LOGE ("invalid set group request");
    return false;
  }

  /* Find group by ID */
  group = melo_settings_find_group (settings, req->id);
  if (!group)
    return false;

  /* Copy new values */
  for (i = 0; i < req->n_entries; i++) {
    MeloSettingsEntry *entry;

    /* Find entry */
    entry = melo_settings_group_find_entry (group, req->entries[i]->id);
    if (!entry)
      return false;

    /* Set entry */
    switch (req->entries[i]->value_case) {
    case SETTINGS__ENTRY__VALUE_BOOLEAN:
      if (entry->type != MELO_SETTINGS_TYPE_BOOLEAN)
        return false;
      entry->new_value._boolean = req->entries[i]->boolean;
      break;
    case SETTINGS__ENTRY__VALUE_INT32:
      if (entry->type != MELO_SETTINGS_TYPE_INT32)
        return false;
      entry->new_value._int32 = req->entries[i]->int32;
      break;
    case SETTINGS__ENTRY__VALUE_UINT32:
      if (entry->type != MELO_SETTINGS_TYPE_UINT32)
        return false;
      entry->new_value._uint32 = req->entries[i]->uint32;
      break;
    case SETTINGS__ENTRY__VALUE_INT64:
      if (entry->type != MELO_SETTINGS_TYPE_INT64)
        return false;
      entry->new_value._int64 = req->entries[i]->int64;
      break;
    case SETTINGS__ENTRY__VALUE_UINT64:
      if (entry->type != MELO_SETTINGS_TYPE_UINT64)
        return false;
      entry->new_value._uint64 = req->entries[i]->uint64;
      break;
    case SETTINGS__ENTRY__VALUE_FLOAT:
      if (entry->type != MELO_SETTINGS_TYPE_FLOAT)
        return false;
      entry->new_value._float = req->entries[i]->float_;
      break;
    case SETTINGS__ENTRY__VALUE_DOUBLE:
      if (entry->type != MELO_SETTINGS_TYPE_DOUBLE)
        return false;
      entry->new_value._double = req->entries[i]->double_;
      break;
    case SETTINGS__ENTRY__VALUE_STRING:
      if (entry->type != MELO_SETTINGS_TYPE_STRING)
        return false;
      entry->new_value._string = req->entries[i]->string;
      break;
    default:
      MELO_LOGE ("invalid entry type %d", req->entries[i]->value_case);
    }
  }

  /* Call update callback */
  if (group->cb && !group->cb (settings, group, &error, group->user_data)) {
    /* Set error */
    resp.resp_case = SETTINGS__RESPONSE__RESP_ERROR;
    resp.error = error ? error : "Failed to save settings";

    /* Pack message */
    msg = melo_message_new (settings__response__get_packed_size (&resp));
    melo_message_set_size (
        msg, settings__response__pack (&resp, melo_message_get_data (msg)));

    /* Free error */
    g_free (error);

    /* Send response */
    if (cb)
      cb (msg, user_data);
    melo_message_unref (msg);

    return true;
  }

  /* Copy new values */
  for (l = group->entries; l != NULL; l = l->next) {
    MeloSettingsEntry *entry = l->data;

    /* Set new value */
    if (entry->type == MELO_SETTINGS_TYPE_STRING) {
      if (entry->value._string == entry->new_value._string)
        continue;
      g_free (entry->value._string);
      entry->value._string = g_strdup (entry->new_value._string);
      entry->new_value._string = entry->value._string;
    } else
      entry->value = entry->new_value;
  }

  /* Set response */
  resp.resp_case = SETTINGS__RESPONSE__RESP_GROUP;
  resp.group = &resp_group;

  /* Generate response */
  melo_settings_get_entry_list (group, &resp_group);

  /* Pack message */
  msg = melo_message_new (settings__response__get_packed_size (&resp));
  melo_message_set_size (
      msg, settings__response__pack (&resp, melo_message_get_data (msg)));

  /* Free response */
  if (resp_group.entries)
    free (resp_group.entries[0]);
  free (resp_group.entries);

  /* Send response */
  if (cb)
    cb (msg, user_data);
  melo_message_unref (msg);

  /* Save to file */
  melo_settings_save (settings);

  return true;
}

static MeloSettings *
melo_settings_get_by_id (const char *id)
{
  MeloSettings *settings = NULL;

  /* Lock settings list */
  G_LOCK (melo_settings_mutex);

  if (melo_settings_list && id)
    settings = g_hash_table_lookup (melo_settings_list, id);
  if (settings)
    g_object_ref (settings);

  /* Unlock settings list */
  G_UNLOCK (melo_settings_mutex);

  return settings;
}

/**
 * melo_settings_handle_request:
 * @msg: the #MeloMessage to handle
 * @cb: the function to call when a response is sent
 * @user_data: data to pass to @cb
 *
 * This function is called when a new settings request is received by the
 * application.
 *
 * If the request updates some settings values, the callback
 * #MeloSettingsUpdateCb registered for the destination group will be called.
 *
 * If the request is malformed or an internal error occurs, the function will
 * return %false, otherwise %true will be returned.
 *
 * Returns: %true if the message has been handled, %false otherwise.
 */
bool
melo_settings_handle_request (MeloMessage *msg, MeloAsyncCb cb, void *user_data)
{
  Settings__Request *req;
  MeloSettings *settings;
  bool ret = false;

  /* Unpack request */
  req = settings__request__unpack (
      NULL, melo_message_get_size (msg), melo_message_get_cdata (msg, NULL));
  if (!req) {
    MELO_LOGE ("failed to unpack request");
    return false;
  }

  /* Find settings from ID */
  settings = melo_settings_get_by_id (req->id);
  if (!settings) {
    settings__request__free_unpacked (req, NULL);
    return false;
  }

  /* Handle request */
  switch (req->req_case) {
  case SETTINGS__REQUEST__REQ_GET_GROUP_LIST:
    ret = melo_settings_get_group_list (
        settings, req->get_group_list, cb, user_data);
    break;
  case SETTINGS__REQUEST__REQ_SET_GROUP:
    ret = melo_settings_set_group (settings, req->set_group, cb, user_data);
    break;
  default:
    MELO_LOGE ("request %u not supported", req->req_case);
  }

  /* Free request */
  settings__request__free_unpacked (req, NULL);
  g_object_unref (settings);

  /* End of request */
  if (ret && cb)
    cb (NULL, user_data);

  return ret;
}

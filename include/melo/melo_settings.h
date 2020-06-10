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

#ifndef _MELO_SETTINGS_H_
#define _MELO_SETTINGS_H_

#include <stdbool.h>
#include <stdint.h>

#include <glib-object.h>

#include <melo/melo_async.h>

G_BEGIN_DECLS

/**
 * MeloSettingsGroup:
 *
 * An opaque structure to handle one or more settings values of type
 * #MeloSettingsEntry.
 */
/**
 * MeloSettingsEntry:
 *
 * An opaque structure to handle a settings within a #MeloSettingsGroup.
 */
typedef enum _MeloSettingsFlag MeloSettingsFlag;
typedef struct _MeloSettingsGroup MeloSettingsGroup;
typedef struct _MeloSettingsEntry MeloSettingsEntry;

/**
 * MeloSettingsFlag:
 * @MELO_SETTINGS_FLAG_NONE: no flag set
 * @MELO_SETTINGS_FLAG_READ_ONLY: the value is read-only and cannot be modified
 *     by the user interface (but modifiable internally)
 * @MELO_SETTINGS_FLAG_PASSWORD: the value is a password and should not be
 *     displayed in the user interface
 * @MELO_SETTINGS_FLAG_NO_EXPORT: the value should not be exported to the user
 *     interface (only for internal use)
 *
 * This flags can be used to add some specificity to a value. flags parameter of
 * melo_settings_group_add_ functions should be filled with a combination of
 * these flags (using OR bit-operator).
 */
enum _MeloSettingsFlag {
  MELO_SETTINGS_FLAG_NONE = 0,
  MELO_SETTINGS_FLAG_READ_ONLY = (1 << 0),
  MELO_SETTINGS_FLAG_PASSWORD = (1 << 1),
  MELO_SETTINGS_FLAG_NO_EXPORT = (1 << 2),
};

/**
 * MeloSettings:
 *
 * An opaque structure to handle an object settings, load and save values to
 * file system.
 */

#define MELO_TYPE_SETTINGS melo_settings_get_type ()
G_DECLARE_FINAL_TYPE (MeloSettings, melo_settings, MELO, SETTINGS, GObject)

/**
 * MeloSettingsUpdateCb:
 * @settings: the current #MeloSettings
 * @group: the current #MeloSettingsGroup
 * @error: (out) (transfer full) (optional): an error string
 * @user_data: user data passed to the callback
 *
 * This function is called when new settings values have been updated and should
 * be checked within a group. The function can access new value and curren
 * value with the melo_settings_entry_get_ functions.
 * If necessary, the value can be modified with one of the setter functions.
 *
 * If the function returns %false, the new settings will be invalidated and the
 * update will be aborted.
 *
 * The parameter @error can be used to set an error string.
 *
 * Returns: %true if the new values are valid, %false otherwise.
 */
typedef bool (*MeloSettingsUpdateCb) (MeloSettings *settings,
    MeloSettingsGroup *group, char **error, void *user_data);

MeloSettings *melo_settings_new (const char *id);

/* Populate functions */
MeloSettingsGroup *melo_settings_add_group (MeloSettings *settings,
    const char *id, const char *name, const char *description,
    MeloSettingsUpdateCb cb, void *user_data);

MeloSettingsEntry *melo_settings_group_add_boolean (MeloSettingsGroup *group,
    const char *id, const char *name, const char *description,
    bool default_value, const MeloSettingsEntry *depends, unsigned int flags);
MeloSettingsEntry *melo_settings_group_add_int32 (MeloSettingsGroup *group,
    const char *id, const char *name, const char *description,
    int32_t default_value, const MeloSettingsEntry *depends,
    unsigned int flags);
MeloSettingsEntry *melo_settings_group_add_uint32 (MeloSettingsGroup *group,
    const char *id, const char *name, const char *description,
    uint32_t default_value, const MeloSettingsEntry *depends,
    unsigned int flags);
MeloSettingsEntry *melo_settings_group_add_int64 (MeloSettingsGroup *group,
    const char *id, const char *name, const char *description,
    int64_t default_value, const MeloSettingsEntry *depends,
    unsigned int flags);
MeloSettingsEntry *melo_settings_group_add_uint64 (MeloSettingsGroup *group,
    const char *id, const char *name, const char *description,
    uint64_t default_value, const MeloSettingsEntry *depends,
    unsigned int flags);
MeloSettingsEntry *melo_settings_group_add_float (MeloSettingsGroup *group,
    const char *id, const char *name, const char *description,
    float default_value, const MeloSettingsEntry *depends, unsigned int flags);
MeloSettingsEntry *melo_settings_group_add_double (MeloSettingsGroup *group,
    const char *id, const char *name, const char *description,
    double default_value, const MeloSettingsEntry *depends, unsigned int flags);
MeloSettingsEntry *melo_settings_group_add_string (MeloSettingsGroup *group,
    const char *id, const char *name, const char *description,
    const char *default_value, const MeloSettingsEntry *depends,
    unsigned int flags);

/* Entry functions */
bool melo_settings_entry_get_boolean (
    MeloSettingsEntry *entry, bool *value, bool *old_value);
bool melo_settings_entry_get_int32 (
    MeloSettingsEntry *entry, int32_t *value, int32_t *old_value);
bool melo_settings_entry_get_uint32 (
    MeloSettingsEntry *entry, uint32_t *value, uint32_t *old_value);
bool melo_settings_entry_get_int64 (
    MeloSettingsEntry *entry, int64_t *value, int64_t *old_value);
bool melo_settings_entry_get_uint64 (
    MeloSettingsEntry *entry, uint64_t *value, uint64_t *old_value);
bool melo_settings_entry_get_float (
    MeloSettingsEntry *entry, float *value, float *old_value);
bool melo_settings_entry_get_double (
    MeloSettingsEntry *entry, double *value, double *old_value);
bool melo_settings_entry_get_string (
    MeloSettingsEntry *entry, const char **value, const char **old_value);

bool melo_settings_entry_set_boolean (MeloSettingsEntry *entry, bool value);
bool melo_settings_entry_set_int32 (MeloSettingsEntry *entry, int32_t value);
bool melo_settings_entry_set_uint32 (MeloSettingsEntry *entry, uint32_t value);
bool melo_settings_entry_set_int64 (MeloSettingsEntry *entry, int64_t value);
bool melo_settings_entry_set_uint64 (MeloSettingsEntry *entry, uint64_t value);
bool melo_settings_entry_set_float (MeloSettingsEntry *entry, float value);
bool melo_settings_entry_set_double (MeloSettingsEntry *entry, double value);
bool melo_settings_entry_set_string (
    MeloSettingsEntry *entry, const char *value);

/* Group functions */
MeloSettingsEntry *melo_settings_group_find_entry (
    MeloSettingsGroup *group, const char *id);

static inline bool
melo_settings_group_get_boolean (
    MeloSettingsGroup *group, const char *id, bool *value, bool *old_value)
{
  return melo_settings_entry_get_boolean (
      melo_settings_group_find_entry (group, id), value, old_value);
}
static inline bool
melo_settings_group_get_int32 (MeloSettingsGroup *group, const char *id,
    int32_t *value, int32_t *old_value)
{
  return melo_settings_entry_get_int32 (
      melo_settings_group_find_entry (group, id), value, old_value);
}
static inline bool
melo_settings_group_get_uint32 (MeloSettingsGroup *group, const char *id,
    uint32_t *value, uint32_t *old_value)
{
  return melo_settings_entry_get_uint32 (
      melo_settings_group_find_entry (group, id), value, old_value);
}
static inline bool
melo_settings_group_get_int64 (MeloSettingsGroup *group, const char *id,
    int64_t *value, int64_t *old_value)
{
  return melo_settings_entry_get_int64 (
      melo_settings_group_find_entry (group, id), value, old_value);
}
static inline bool
melo_settings_group_get_uint64 (MeloSettingsGroup *group, const char *id,
    uint64_t *value, uint64_t *old_value)
{
  return melo_settings_entry_get_uint64 (
      melo_settings_group_find_entry (group, id), value, old_value);
}
static inline bool
melo_settings_group_get_float (
    MeloSettingsGroup *group, const char *id, float *value, float *old_value)
{
  return melo_settings_entry_get_float (
      melo_settings_group_find_entry (group, id), value, old_value);
}
static inline bool
melo_settings_group_get_double (
    MeloSettingsGroup *group, const char *id, double *value, double *old_value)
{
  return melo_settings_entry_get_double (
      melo_settings_group_find_entry (group, id), value, old_value);
}
static inline bool
melo_settings_group_get_string (MeloSettingsGroup *group, const char *id,
    const char **value, const char **old_value)
{
  return melo_settings_entry_get_string (
      melo_settings_group_find_entry (group, id), value, old_value);
}

static inline bool
melo_settings_group_set_boolean (
    MeloSettingsGroup *group, const char *id, bool value)
{
  return melo_settings_entry_set_boolean (
      melo_settings_group_find_entry (group, id), value);
}
static inline bool
melo_settings_group_set_int32 (
    MeloSettingsGroup *group, const char *id, int32_t value)
{
  return melo_settings_entry_set_int32 (
      melo_settings_group_find_entry (group, id), value);
}
static inline bool
melo_settings_group_set_uint32 (
    MeloSettingsGroup *group, const char *id, uint32_t value)
{
  return melo_settings_entry_set_uint32 (
      melo_settings_group_find_entry (group, id), value);
}
static inline bool
melo_settings_group_set_int64 (
    MeloSettingsGroup *group, const char *id, int64_t value)
{
  return melo_settings_entry_set_int64 (
      melo_settings_group_find_entry (group, id), value);
}
static inline bool
melo_settings_group_set_uint64 (
    MeloSettingsGroup *group, const char *id, uint64_t value)
{
  return melo_settings_entry_set_uint64 (
      melo_settings_group_find_entry (group, id), value);
}
static inline bool
melo_settings_group_set_float (
    MeloSettingsGroup *group, const char *id, float value)
{
  return melo_settings_entry_set_float (
      melo_settings_group_find_entry (group, id), value);
}
static inline bool
melo_settings_group_set_double (
    MeloSettingsGroup *group, const char *id, double value)
{
  return melo_settings_entry_set_double (
      melo_settings_group_find_entry (group, id), value);
}
static inline bool
melo_settings_group_set_string (
    MeloSettingsGroup *group, const char *id, const char *value)
{
  return melo_settings_entry_set_string (
      melo_settings_group_find_entry (group, id), value);
}

/* Settings functions */
MeloSettingsGroup *melo_settings_find_group (
    MeloSettings *settings, const char *id);

void melo_settings_load (MeloSettings *settings);
void melo_settings_save (MeloSettings *settings);

/* Request */
bool melo_settings_handle_request (
    MeloMessage *msg, MeloAsyncCb cb, void *user_data);

G_END_DECLS

#endif /* !_MELO_SETTINGS_H_ */

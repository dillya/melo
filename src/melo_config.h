/*
 * melo_config.h: Configuration class
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

#ifndef __MELO_CONFIG_H__
#define __MELO_CONFIG_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define MELO_TYPE_CONFIG             (melo_config_get_type ())
#define MELO_CONFIG(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), MELO_TYPE_CONFIG, MeloConfig))
#define MELO_IS_CONFIG(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MELO_TYPE_CONFIG))
#define MELO_CONFIG_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), MELO_TYPE_CONFIG, MeloConfigClass))
#define MELO_IS_CONFIG_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), MELO_TYPE_CONFIG))
#define MELO_CONFIG_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), MELO_TYPE_CONFIG, MeloConfigClass))

typedef struct _MeloConfig MeloConfig;
typedef struct _MeloConfigClass MeloConfigClass;
typedef struct _MeloConfigPrivate MeloConfigPrivate;

typedef union _MeloConfigValue MeloConfigValue;
typedef struct _MeloConfigItem MeloConfigItem;
typedef struct _MeloConfigGroup MeloConfigGroup;
typedef struct _MeloConfigContext MeloConfigContext;

struct _MeloConfig {
  GObject parent_instance;

  /*< private >*/
  MeloConfigPrivate *priv;
};

struct _MeloConfigClass {
  GObjectClass parent_class;
};

typedef enum {
  MELO_CONFIG_TYPE_NONE = 0,
  MELO_CONFIG_TYPE_BOOLEAN,
  MELO_CONFIG_TYPE_INTEGER,
  MELO_CONFIG_TYPE_DOUBLE,
  MELO_CONFIG_TYPE_STRING,

  MELO_CONFIG_TYPE_COUNT
} MeloConfigType;

typedef enum {
  MELO_CONFIG_ELEMENT_NONE = 0,
  MELO_CONFIG_ELEMENT_CHECKBOX,
  MELO_CONFIG_ELEMENT_NUMBER,
  MELO_CONFIG_ELEMENT_TEXT,
  MELO_CONFIG_ELEMENT_PASSWORD,

  MELO_CONFIG_ELEMENT_COUNT
} MeloConfigElement;

typedef enum {
  MELO_CONFIG_FLAGS_NONE = 0,
  MELO_CONFIG_FLAGS_READ_ONLY = 1 << 0,
  MELO_CONFIG_FLAGS_DONT_SHOW = 1 << 1,
  MELO_CONFIG_FLAGS_DONT_SAVE = 1 << 2,
} MeloConfigFlags;

union _MeloConfigValue {
  gboolean _boolean;
  gint64 _integer;
  gdouble _double;
  gchar *_string;
};

struct _MeloConfigItem {
  gchar *id;
  gchar *name;
  MeloConfigType type;
  MeloConfigElement element;
  MeloConfigValue def;
  MeloConfigFlags flags;
};

struct _MeloConfigGroup {
  gchar *id;
  gchar *name;
  MeloConfigItem *items;
  gint items_count;
};

GType melo_config_get_type (void);

MeloConfig *melo_config_new (const gchar *id, const MeloConfigGroup *groups,
                             gint groups_count);
MeloConfig *melo_config_get_config_by_id (const gchar *id);

const gchar *melo_config_type_to_string (MeloConfigType type);
const gchar *melo_config_element_to_string (MeloConfigElement element);

const MeloConfigGroup *melo_config_get_groups (MeloConfig *config, gint *count);

void melo_config_load_default (MeloConfig *config);
gboolean melo_config_load_from_file (MeloConfig *config, const gchar *filename);
gboolean melo_config_save_to_file (MeloConfig *config, const gchar *filename);

gboolean melo_config_get_boolean (MeloConfig *config, const gchar *group,
                                 const gchar *id, gboolean *value);
gboolean melo_config_get_integer (MeloConfig *config, const gchar *group,
                                  const gchar *id, gint64 *value);
gboolean melo_config_get_double (MeloConfig *config, const gchar *group,
                                 const gchar *id, gdouble *value);
gboolean melo_config_get_string (MeloConfig *config, const gchar *group,
                                 const gchar *id, gchar **value);

gboolean melo_config_set_boolean (MeloConfig *config, const gchar *group,
                                  const gchar *id, gboolean value);
gboolean melo_config_set_integer (MeloConfig *config, const gchar *group,
                                  const gchar *id, gint64 value);
gboolean melo_config_set_double (MeloConfig *config, const gchar *group,
                                 const gchar *id, gdouble value);
gboolean melo_config_set_string (MeloConfig *config, const gchar *group,
                                 const gchar *id, const gchar *value);

typedef gboolean (*MeloConfigCheckFunc) (MeloConfigContext *context,
                                         gpointer user_data);
typedef void (*MeloConfigUpdateFunc) (MeloConfigContext *context,
                                      gpointer user_data);
void melo_config_set_check_callback (MeloConfig *config, const gchar *group,
                                     MeloConfigCheckFunc callback,
                                     gpointer user_data);
void melo_config_set_update_callback (MeloConfig *config, const gchar *group,
                                      MeloConfigUpdateFunc callback,
                                      gpointer user_data);

/* Advanced functions */
typedef gpointer (*MeloConfigFunc) (MeloConfigContext *context,
                                    gpointer user_data);
gpointer melo_config_parse (MeloConfig *config, MeloConfigFunc callback,
                            gpointer user_data);
gboolean melo_config_update (MeloConfig *config, MeloConfigFunc callback,
                             gpointer user_data);

gint melo_config_get_group_count (MeloConfigContext *context);
gboolean melo_config_next_group (MeloConfigContext *context,
                                 const MeloConfigGroup **group,
                                 gint *items_count);
gboolean melo_config_next_item (MeloConfigContext *context,
                                MeloConfigItem **item, MeloConfigValue *value);
gboolean melo_config_find_group (MeloConfigContext *context,
                                 const gchar *group_id,
                                 const MeloConfigGroup **group,
                                 gint *items_count);
gboolean melo_config_find_item (MeloConfigContext *context,
                                const gchar *item_id, MeloConfigItem **item,
                                MeloConfigValue *value);

void melo_config_update_boolean (MeloConfigContext *context, gboolean value);
void melo_config_update_integer (MeloConfigContext *context, gint64 value);
void melo_config_update_double (MeloConfigContext *context, gdouble value);
void melo_config_update_string (MeloConfigContext *context, const gchar *value);
void melo_config_remove_update (MeloConfigContext *context);

gboolean melo_config_get_updated_boolean (MeloConfigContext *context,
                                          const gchar *id,
                                          gboolean *value,
                                          gboolean *old_value);
gboolean melo_config_get_updated_integer (MeloConfigContext *context,
                                          const gchar *id,
                                          gint64 *value,
                                          gint64 *old_value);
gboolean melo_config_get_updated_double (MeloConfigContext *context,
                                         const gchar *id,
                                         gdouble *value,
                                         gdouble *old_value);
gboolean melo_config_get_updated_string (MeloConfigContext *context,
                                         const gchar *id,
                                         const gchar **value,
                                         const gchar **old_value);

G_END_DECLS

#endif /* __MELO_CONFIG_H__ */

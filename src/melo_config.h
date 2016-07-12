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

typedef struct _MeloConfigValues MeloConfigValues;

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

struct _MeloConfigValues {
  GHashTable *ids;
  MeloConfigValue *values;
  gsize size;
};

GType melo_config_get_type (void);

MeloConfig *melo_config_new (const gchar *id, const MeloConfigGroup *groups,
                             gint groups_count);
MeloConfig *melo_config_get_config_by_id (const gchar *id);

const gchar *melo_config_type_to_string (MeloConfigType type);
const gchar *melo_config_element_to_string (MeloConfigElement element);

const MeloConfigGroup *melo_config_get_groups (MeloConfig *config, gint *count);

void melo_config_load_default (MeloConfig *config);

gboolean melo_config_get_boolean (MeloConfig *config, const gchar *group,
                                 const gchar *id, gboolean *value);
gboolean melo_config_get_integer (MeloConfig *config, const gchar *group,
                                  const gchar *id, gint64 *value);
gboolean melo_config_get_double (MeloConfig *config, const gchar *group,
                                 const gchar *id, gdouble *value);
gboolean melo_config_get_string (MeloConfig *config, const gchar *group,
                                 const gchar *id, gchar **value);

/* Advanced functions */
typedef gpointer (*MeloConfigFunc) (const MeloConfigGroup *groups,
                                    gint groups_count,
                                    MeloConfigValues *values,
                                    gpointer user_data);
gpointer melo_config_read_all (MeloConfig *config, MeloConfigFunc callback,
                               gpointer user_data);

G_END_DECLS

#endif /* __MELO_CONFIG_H__ */

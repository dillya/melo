/*
 * melo_module.h: Module base class
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

#ifndef __MELO_MODULE_H__
#define __MELO_MODULE_H__

#include <glib-object.h>

#include "melo_browser.h"
#include "melo_player.h"

G_BEGIN_DECLS

#define MELO_TYPE_MODULE             (melo_module_get_type ())
#define MELO_MODULE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), MELO_TYPE_MODULE, MeloModule))
#define MELO_IS_MODULE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MELO_TYPE_MODULE))
#define MELO_MODULE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), MELO_TYPE_MODULE, MeloModuleClass))
#define MELO_IS_MODULE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), MELO_TYPE_MODULE))
#define MELO_MODULE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), MELO_TYPE_MODULE, MeloModuleClass))

typedef struct _MeloModule MeloModule;
typedef struct _MeloModuleClass MeloModuleClass;
typedef struct _MeloModulePrivate MeloModulePrivate;

typedef struct _MeloModuleInfo MeloModuleInfo;

struct _MeloModule {
  GObject parent_instance;

  /*< private >*/
  MeloModulePrivate *priv;
};

struct _MeloModuleClass {
  GObjectClass parent_class;

  const MeloModuleInfo *(*get_info) (MeloModule *module);
};

struct _MeloModuleInfo {
  const gchar *name;
  const gchar *description;
  const gchar *config_id;
};

GType melo_module_get_type (void);

const gchar *melo_module_get_id (MeloModule *module);
const MeloModuleInfo *melo_module_get_info (MeloModule *module);

/* Register a browser */
gboolean melo_module_register_browser (MeloModule *module,
                                       MeloBrowser *browser);
void melo_module_unregister_browser (MeloModule *module, const char *id);
GList *melo_module_get_browser_list (MeloModule *module);

/* Register a player */
gboolean melo_module_register_player (MeloModule *module, MeloPlayer *player);
void melo_module_unregister_player (MeloModule *module, const char *id);
GList *melo_module_get_player_list (MeloModule *module);

/* Register a new module */
gboolean melo_module_register (GType type, const gchar *id);
void melo_module_unregister (const gchar *id);

/* Get MeloModule list */
GList *melo_module_get_module_list (void);
/* Get MeloModule by id */
MeloModule *melo_module_get_module_by_id (const gchar *id);

/* Build a path for a file in module directory */
gchar *melo_module_build_path (MeloModule *module, const gchar *file);

G_END_DECLS

#endif /* __MELO_MODULE_H__ */

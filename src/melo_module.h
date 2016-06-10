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

G_BEGIN_DECLS

#define MELO_TYPE_MODULE             (melo_module_get_type ())
#define MELO_MODULE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), MELO_TYPE_MODULE, MeloModule))
#define MELO_IS_MODULE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MELO_TYPE_MODULE))
#define MELO_MODULE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), MELO_TYPE_MODULE, MeloModuleClass))
#define MELO_IS_MODULE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), MELO_TYPE_MODULE))
#define MELO_MODULE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), MELO_TYPE_MODULE, MeloModuleClass))

typedef struct _MeloModule MeloModule;
typedef struct _MeloModuleClass MeloModuleClass;

struct _MeloModule {
  GObject parent_instance;
};

struct _MeloModuleClass {
  GObjectClass parent_class;
};

GType melo_module_get_type (void);

/* Register a new module */
gboolean melo_module_register (GType type, const gchar *name);
void melo_module_unregister (const gchar *name);

G_END_DECLS

#endif /* __MELO_MODULE_H__ */

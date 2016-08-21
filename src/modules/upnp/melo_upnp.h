/*
 * melo_upnp.h: UPnP / DLNA renderer module
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

#ifndef __MELO_UPNP_H__
#define __MELO_UPNP_H__

#include "melo_module.h"

G_BEGIN_DECLS

#define MELO_TYPE_UPNP             (melo_upnp_get_type ())
#define MELO_UPNP(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), MELO_TYPE_UPNP, MeloUpnp))
#define MELO_IS_UPNP(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MELO_TYPE_UPNP))
#define MELO_UPNP_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), MELO_TYPE_UPNP, MeloUpnpClass))
#define MELO_IS_UPNP_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), MELO_TYPE_UPNP))
#define MELO_UPNP_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), MELO_TYPE_UPNP, MeloUpnpClass))

typedef struct _MeloUpnp MeloUpnp;
typedef struct _MeloUpnpClass MeloUpnpClass;
typedef struct _MeloUpnpPrivate MeloUpnpPrivate;

struct _MeloUpnp {
  MeloModule parent_instance;

  /*< private >*/
  MeloUpnpPrivate *priv;
};

struct _MeloUpnpClass {
  MeloModuleClass parent_class;
};

GType melo_upnp_get_type (void);

gboolean melo_upnp_set_name (MeloUpnp *up, const gchar *name);

G_END_DECLS

#endif /* __MELO_UPNP_H__ */

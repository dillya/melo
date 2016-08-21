/*
 * melo_player_upnp.h: UPnP / DLNA renderer using Rygel
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

#ifndef __MELO_PLAYER_UPNP_H__
#define __MELO_PLAYER_UPNP_H__

#include "melo_player.h"

G_BEGIN_DECLS

#define MELO_TYPE_PLAYER_UPNP             (melo_player_upnp_get_type ())
#define MELO_PLAYER_UPNP(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), MELO_TYPE_PLAYER_UPNP, MeloPlayerUpnp))
#define MELO_IS_PLAYER_UPNP(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MELO_TYPE_PLAYER_UPNP))
#define MELO_PLAYER_UPNP_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), MELO_TYPE_PLAYER_UPNP, MeloPlayerUpnpClass))
#define MELO_IS_PLAYER_UPNP_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), MELO_TYPE_PLAYER_UPNP))
#define MELO_PLAYER_UPNP_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), MELO_TYPE_PLAYER_UPNP, MeloPlayerUpnpClass))

typedef struct _MeloPlayerUpnp MeloPlayerUpnp;
typedef struct _MeloPlayerUpnpClass MeloPlayerUpnpClass;
typedef struct _MeloPlayerUpnpPrivate MeloPlayerUpnpPrivate;

struct _MeloPlayerUpnp {
  MeloPlayer parent_instance;

  /*< private >*/
  MeloPlayerUpnpPrivate *priv;
};

struct _MeloPlayerUpnpClass {
  MeloPlayerClass parent_class;
};

GType melo_player_upnp_get_type (void);

gboolean melo_player_upnp_start (MeloPlayerUpnp *up, const gchar *name);
void melo_player_upnp_stop (MeloPlayerUpnp *up);

G_END_DECLS

#endif /* __MELO_PLAYER_UPNP_H__ */

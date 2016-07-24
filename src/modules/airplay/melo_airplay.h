/*
 * melo_airplay.h: Airplay module for remote speakers
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

#ifndef __MELO_AIRPLAY_H__
#define __MELO_AIRPLAY_H__

#include "melo_module.h"

G_BEGIN_DECLS

#define MELO_TYPE_AIRPLAY             (melo_airplay_get_type ())
#define MELO_AIRPLAY(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), MELO_TYPE_AIRPLAY, MeloAirplay))
#define MELO_IS_AIRPLAY(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MELO_TYPE_AIRPLAY))
#define MELO_AIRPLAY_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), MELO_TYPE_AIRPLAY, MeloAirplayClass))
#define MELO_IS_AIRPLAY_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), MELO_TYPE_AIRPLAY))
#define MELO_AIRPLAY_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), MELO_TYPE_AIRPLAY, MeloAirplayClass))

typedef struct _MeloAirplay MeloAirplay;
typedef struct _MeloAirplayClass MeloAirplayClass;
typedef struct _MeloAirplayPrivate MeloAirplayPrivate;

struct _MeloAirplay {
  MeloModule parent_instance;

  /*< private >*/
  MeloAirplayPrivate *priv;
};

struct _MeloAirplayClass {
  MeloModuleClass parent_class;
};

GType melo_airplay_get_type (void);

gboolean melo_airplay_set_name (MeloAirplay *air, const gchar *name);
gboolean melo_airplay_set_port (MeloAirplay *air, int port);

G_END_DECLS

#endif /* __MELO_AIRPLAY_H__ */

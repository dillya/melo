/*
 * melo_discover.h: A Melo device discoverer
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

#ifndef __MELO_DISCOVER_H__
#define __MELO_DISCOVER_H__

G_BEGIN_DECLS

#define MELO_TYPE_DISCOVER \
    (melo_discover_get_type ())
#define MELO_DISCOVER(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), MELO_TYPE_DISCOVER, MeloDiscover))
#define MELO_IS_DISCOVER(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MELO_TYPE_DISCOVER))
#define MELO_DISCOVER_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), MELO_TYPE_DISCOVER, MeloDiscoverClass))
#define MELO_IS_DISCOVER_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), MELO_TYPE_DISCOVER))
#define MELO_DISCOVER_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), MELO_TYPE_DISCOVER, MeloDiscoverClass))

typedef struct _MeloDiscover MeloDiscover;
typedef struct _MeloDiscoverClass MeloDiscoverClass;
typedef struct _MeloDiscoverPrivate MeloDiscoverPrivate;

struct _MeloDiscover {
  GObject parent_instance;

  /*< private >*/
  MeloDiscoverPrivate *priv;
};

struct _MeloDiscoverClass {
  GObjectClass parent_class;
};

GType melo_discover_get_type (void);

MeloDiscover *melo_discover_new (void);

gboolean melo_discover_register_device (MeloDiscover *disco, const gchar *name,
                                        guint port);
gboolean melo_discover_unregister_device (MeloDiscover *disco);

G_END_DECLS

#endif /* __MELO_DISCOVER_H__ */

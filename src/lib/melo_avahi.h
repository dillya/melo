/*
 * melo_avahi.h: Avahi client to register services
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

#ifndef __MELO_AVAHI_H__
#define __MELO_AVAHI_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define MELO_TYPE_AVAHI             (melo_avahi_get_type ())
#define MELO_AVAHI(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), MELO_TYPE_AVAHI, MeloAvahi))
#define MELO_IS_AVAHI(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MELO_TYPE_AVAHI))
#define MELO_AVAHI_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), MELO_TYPE_AVAHI, MeloAvahiClass))
#define MELO_IS_AVAHI_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), MELO_TYPE_AVAHI))
#define MELO_AVAHI_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), MELO_TYPE_AVAHI, MeloAvahiClass))

typedef struct _MeloAvahi MeloAvahi;
typedef struct _MeloAvahiClass MeloAvahiClass;
typedef struct _MeloAvahiPrivate MeloAvahiPrivate;

typedef struct AvahiStringList AvahiStringList;
typedef struct _MeloAvahiService MeloAvahiService;

struct _MeloAvahi {
  GObject parent_instance;

  /*< private >*/
  MeloAvahiPrivate *priv;
};

struct _MeloAvahiClass {
  GObjectClass parent_class;
};

struct _MeloAvahiService {
  gchar *name;
  gchar *type;
  int port;
  AvahiStringList *txt;
  unsigned char ip[4];
  int iface;
};

GType melo_avahi_get_type (void);

MeloAvahi *melo_avahi_new (void);

/* Service publisher */
const MeloAvahiService *melo_avahi_add_service (MeloAvahi *avahi,
                                                const gchar *name,
                                                const gchar *type, gint port,
                                                ...);
gboolean melo_avahi_update_service (MeloAvahi *avahi,
                                    const MeloAvahiService *service,
                                    const gchar *name, const gchar *type,
                                    gint port, gboolean update_txt, ...);
void melo_avahi_remove_service (MeloAvahi *avahi,
                                const MeloAvahiService *service);

/* Service browser */
gboolean melo_avahi_add_browser (MeloAvahi *avahi, const gchar *type);
GList *melo_avahi_list_services (MeloAvahi *avahi);
void melo_avahi_remove_browser (MeloAvahi *avahi, const gchar *type);

gchar *melo_avahi_service_get_txt (const MeloAvahiService *s, const gchar *key);
MeloAvahiService *melo_avahi_service_copy (const MeloAvahiService *s);
void melo_avahi_service_free (MeloAvahiService *s);


G_END_DECLS

#endif /* __MELO_AVAHI_H__ */

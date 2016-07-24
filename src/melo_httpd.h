/*
 * melo_httpd.h: HTTP server for Melo remote control
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

#ifndef __MELO_HTTPD_H__
#define __MELO_HTTPD_H__

#include <libsoup/soup.h>

G_BEGIN_DECLS

#define MELO_TYPE_HTTPD             (melo_httpd_get_type ())
#define MELO_HTTPD(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), MELO_TYPE_HTTPD, MeloHTTPD))
#define MELO_IS_HTTPD(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MELO_TYPE_HTTPD))
#define MELO_HTTPD_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), MELO_TYPE_HTTPD, MeloHTTPDClass))
#define MELO_IS_HTTPD_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), MELO_TYPE_HTTPD))
#define MELO_HTTPD_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), MELO_TYPE_HTTPD, MeloHTTPDClass))

typedef struct _MeloHTTPD MeloHTTPD;
typedef struct _MeloHTTPDClass MeloHTTPDClass;
typedef struct _MeloHTTPDPrivate MeloHTTPDPrivate;

struct _MeloHTTPD {
  GObject parent_instance;

  /*< private >*/
  MeloHTTPDPrivate *priv;
};

struct _MeloHTTPDClass {
  GObjectClass parent_class;
};

MeloHTTPD *melo_httpd_new (void);

gboolean melo_httpd_start (MeloHTTPD *httpd, guint port, const gchar *name);
void melo_httpd_stop (MeloHTTPD *httpd);

void melo_httpd_auth_enable (MeloHTTPD *httpd);
void melo_httpd_auth_disable (MeloHTTPD *httpd);
void melo_httpd_auth_set_username (MeloHTTPD *httpd, const gchar *username);
void melo_httpd_auth_set_password (MeloHTTPD *httpd, const gchar *password);
gchar *melo_httpd_auth_get_username (MeloHTTPD *httpd);
gchar *melo_httpd_auth_get_password (MeloHTTPD *httpd);

G_END_DECLS

#endif /* __MELO_HTTPD_H__ */

/*
 * gstraopclientclock.h: clock that synchronizes with RAOP NTP clock
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

#ifndef __GST_RAOP_CLIENT_CLOCK_H__
#define __GST_RAOP_CLIENT_CLOCK_H__

#include <gst/gst.h>
#include <gst/gstsystemclock.h>

G_BEGIN_DECLS

#define GST_TYPE_RAOP_CLIENT_CLOCK \
  (gst_raop_client_clock_get_type())
#define GST_RAOP_CLIENT_CLOCK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_RAOP_CLIENT_CLOCK,GstRaopClientClock))
#define GST_RAOP_CLIENT_CLOCK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_RAOP_CLIENT_CLOCK,GstRaopClientClockClass))
#define GST_IS_RAOP_CLIENT_CLOCK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_RAOP_CLIENT_CLOCK))
#define GST_IS_RAOP_CLIENT_CLOCK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_RAOP_CLIENT_CLOCK))

typedef struct _GstRaopClientClock GstRaopClientClock;
typedef struct _GstRaopClientClockClass GstRaopClientClockClass;
typedef struct _GstRaopClientClockPrivate GstRaopClientClockPrivate;

struct _GstRaopClientClock {
  GstSystemClock clock;

  /*< private >*/
  GstRaopClientClockPrivate *priv;

  gpointer _gst_reserved[GST_PADDING];
};

struct _GstRaopClientClockClass {
  GstSystemClockClass parent_class;

  /*< private >*/
  gpointer _gst_reserved[GST_PADDING];
};

GType gst_raop_client_clock_get_type (void);

GstClock* gst_raop_client_clock_new (const gchar *name,
                                     const gchar *remote_address,
                                     gint remote_port, GstClockTime base_time);

#ifdef G_DEFINE_AUTOPTR_CLEANUP_FUNC
G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstRaopClientClock, gst_object_unref)
#endif

G_END_DECLS

#endif /* __GST_RAOP_CLIENT_CLOCK_H__ */

/*
 * gstrtpraop.h: RTP muxer for RAOP
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

#ifndef __GST_RTP_RAOP_H__
#define __GST_RTP_RAOP_H__

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_RTP_RAOP \
  (gst_rtp_raop_get_type())
#define GST_RTP_RAOP(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_RTP_RAOP, GstRtpRaop))
#define GST_RTP_RAOP_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_RTP_RAOP, GstRtpRaopClass))
#define GST_RTP_RAOP_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_RTP_RAOP, GstRtpRaopClass))
#define GST_IS_RTP_RAOP(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_RTP_RAOP))
#define GST_IS_RTP_RAOP_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_RTP_RAOP))

typedef struct _GstRtpRaop GstRtpRaop;
typedef struct _GstRtpRaopClass GstRtpRaopClass;
typedef struct _GstRtpRaopPrivate GstRtpRaopPrivate;

struct _GstRtpRaop
{
  GstElement element;

  /*< private >*/
  GstRtpRaopPrivate *priv;
};

struct _GstRtpRaopClass
{
  GstElementClass parent_class;
};

GType gst_rtp_raop_get_type (void);
gboolean gst_rtp_raop_plugin_init (GstPlugin * plugin);

G_END_DECLS

#endif /* __GST_RTP_RAOP_H__ */

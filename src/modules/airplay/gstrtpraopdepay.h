/*
 * gstrtpraopdepay.h: RTP RAOP Depayloader
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

#ifndef __GST_RTP_RAOP_DEPAY_H__
#define __GST_RTP_RAOP_DEPAY_H__

#include <gst/gst.h>
#include <gst/rtp/gstrtpbasedepayload.h>

G_BEGIN_DECLS

#define GST_TYPE_RTP_RAOP_DEPAY \
  (gst_rtp_raop_depay_get_type())
#define GST_RTP_RAOP_DEPAY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_RTP_RAOP_DEPAY,GstRtpRaopDepay))
#define GST_RTP_RAOP_DEPAY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_RTP_RAOP_DEPAY,GstRtpRaopDepayClass))
#define GST_IS_RTP_RAOP_DEPAY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_RTP_RAOP_DEPAY))
#define GST_IS_RTP_RAOP_DEPAY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_RTP_RAOP_DEPAY))

typedef struct _GstRtpRaopDepay GstRtpRaopDepay;
typedef struct _GstRtpRaopDepayClass GstRtpRaopDepayClass;
typedef struct _GstRtpRaopDepayPrivate GstRtpRaopDepayPrivate;

struct _GstRtpRaopDepay
{
  GstRTPBaseDepayload parent;

  /*< private >*/
  GstRtpRaopDepayPrivate *priv;
};

struct _GstRtpRaopDepayClass
{
  GstRTPBaseDepayloadClass parent_class;
};

GType gst_rtp_raop_depay_get_type (void);

gboolean gst_rtp_raop_depay_plugin_init (GstPlugin * plugin);

gboolean gst_rtp_raop_depay_set_key (GstRtpRaopDepay * rtpraopdepay,
    const guchar *key, gsize key_len, const guchar *iv, gsize iv_len);
gboolean gst_rtp_raop_depay_query_rtptime (GstRtpRaopDepay * rtpraopdepay,
    guint32 *rtptime);

G_END_DECLS

#endif /* __GST_RTP_RAOP_DEPAY_H__ */

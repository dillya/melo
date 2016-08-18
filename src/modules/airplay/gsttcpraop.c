/*
 * gsttcpraop.c: TCP depayloader for RAOP
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

#include <string.h>

#include <gio/gio.h>
#include <gst/gst.h>

#include "gsttcpraop.h"

#define DEFAULT_CLOCK_RATE      44100
#define DEFAULT_SAMPLES         4096

GST_DEBUG_CATEGORY_STATIC (gst_tcp_raop_debug);
#define GST_CAT_DEFAULT gst_tcp_raop_debug

static GstStaticPadTemplate gst_tcp_raop_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtp-stream")
    );

static GstStaticPadTemplate gst_tcp_raop_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtp")
    );

struct _GstTcpRaopPrivate {
  gint clock_rate;
  guint samples;
  guint32 rtptime;
  guint16 seq;
  gboolean first;
};

#define gst_tcp_raop_parent_class parent_class
G_DEFINE_TYPE_WITH_PRIVATE (GstTcpRaop, gst_tcp_raop, GST_TYPE_BASE_PARSE);

static gboolean gst_tcp_raop_set_sink_caps (GstBaseParse * parse,
    GstCaps * caps);
static GstCaps *gst_tcp_raop_get_sink_caps (GstBaseParse * parse,
    GstCaps * filter);
static GstFlowReturn gst_tcp_raop_handle_frame (GstBaseParse * parse,
    GstBaseParseFrame * frame, gint * skipsize);

static void
gst_tcp_raop_class_init (GstTcpRaopClass * klass)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstBaseParseClass *parse_class = GST_BASE_PARSE_CLASS (klass);

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_tcp_raop_src_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_tcp_raop_sink_template));

  gst_element_class_set_static_metadata (gstelement_class,
      "RAOP TCP Stream Depayloading", "Codec/Depayloader/Network",
      "Depayloads RTP packets from RAOP TCP stream",
      "Alexandre Dilly <alexandre.dilly@sparod.com>");

  parse_class->set_sink_caps =
      GST_DEBUG_FUNCPTR (gst_tcp_raop_set_sink_caps);
  parse_class->handle_frame =
      GST_DEBUG_FUNCPTR (gst_tcp_raop_handle_frame);
}

static void
gst_tcp_raop_init (GstTcpRaop * raop)
{
  GstTcpRaopPrivate *priv = gst_tcp_raop_get_instance_private (raop);

  raop->priv = priv;
  priv->clock_rate = DEFAULT_CLOCK_RATE;
  priv->samples = DEFAULT_SAMPLES;
  priv->rtptime = 0;
  priv->seq = 0;

  /* set minimal frame size to magic words + length +  RTP header */
  gst_base_parse_set_min_frame_size (GST_BASE_PARSE (raop), 16);
}

static gboolean
gst_tcp_raop_set_sink_caps (GstBaseParse * parse, GstCaps * caps)
{
  GstTcpRaop *raop;
  GstTcpRaopPrivate *priv;
  GstStructure *structure;
  GstCaps *src_caps;
  gboolean ret;
  const gchar *config;

  raop = GST_TCP_RAOP (parse);
  priv = raop->priv;

  /* copy caps from sink */
  src_caps = gst_caps_copy (caps);
  structure = gst_caps_get_structure (src_caps, 0);

  /* replace name */
  gst_structure_set_name (structure, "application/x-rtp");

  /* get clock rate */
  if (!gst_structure_get_int (structure, "clock-rate", &priv->clock_rate)) {
    GST_WARNING_OBJECT (raop, "No clock rate provided: set to default %d!",
                        DEFAULT_CLOCK_RATE);
    priv->clock_rate = DEFAULT_CLOCK_RATE;
  }

  /* extract sample size from config */
  config = gst_structure_get_string (structure, "config");
  if (config) {
    strtoul (config, &config, 10);
    priv->samples = strtoul (config, NULL, 10);
  } else
    priv->samples = DEFAULT_SAMPLES;

  ret = gst_pad_set_caps (GST_BASE_PARSE_SRC_PAD (parse), src_caps);
  gst_caps_unref (src_caps);

  return ret;
}

static GstFlowReturn
gst_tcp_raop_handle_frame (GstBaseParse * parse, GstBaseParseFrame * frame,
    gint * skipsize)
{
  GstTcpRaop *raop;
  GstTcpRaopPrivate *priv;
  guint8 header[16];
  gsize buf_size;
  guint16 size;

  raop = GST_TCP_RAOP (parse);
  priv = raop->priv;

  /* get header */
  if (gst_buffer_extract (frame->buffer, 0, &header, 16) != 16)
    return GST_FLOW_ERROR;

  /* check magic word */
  if (header[0] != 0x24)
    return GST_FLOW_ERROR;

  /* get RTP packet size */
  size = header[2] << 8 | header[3];
  buf_size = gst_buffer_get_size (frame->buffer);

  /* need more data */
  if (size + 4 > buf_size)
    return GST_FLOW_OK;

  /* fix RTP header (Pulseaudio send bad RTP header) */
  if (header[4] != 0x80) {
    GST_DEBUG_OBJECT (raop, "Bad RTP header: fix it");

    /* set RTP version and payload */
    header[4] = 0x80;
    header[5] = 0x60;
    if (!priv->first) {
      header[5] |= 0x80;
      priv->first = TRUE;
    }

    /* set sequence and timestamp */
    *((guint16 *) &header[6]) = g_htons (priv->seq);
    *((guint32 *) &header[8]) = g_htonl (priv->rtptime);
    priv->rtptime += priv->samples;
    priv->seq++;

    gst_buffer_fill (frame->buffer, 0, header, 16);
  }

  /* get only RTP buffer */
  frame->out_buffer =
      gst_buffer_copy_region (frame->buffer, GST_BUFFER_COPY_ALL, 4, size);

  return gst_base_parse_finish_frame (parse, frame, size + 4);
}

gboolean
gst_tcp_raop_plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (gst_tcp_raop_debug, "tcpraop", 0,
      "RAOP RTP stream depayloader");

  return gst_element_register (plugin, "tcpraop", GST_RANK_NONE,
      GST_TYPE_TCP_RAOP);
}

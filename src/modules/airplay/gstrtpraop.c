/*
 * gstrtpraop.c: RTP muxer for RAOP
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
#include <gst/rtp/gstrtpbuffer.h>

#include "gstrtpraop.h"

#define UDP_DEFAULT_HOST        "localhost"
#define UDP_DEFAULT_PORT        6001

GST_DEBUG_CATEGORY_STATIC (gst_rtp_raop_debug);
#define GST_CAT_DEFAULT gst_rtp_raop_debug

static GstStaticPadTemplate gst_rtp_raop_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtp,payload = (int) 96")
    );

static GstStaticPadTemplate gst_rtp_raop_sink_ctrl_template =
GST_STATIC_PAD_TEMPLATE ("sink_ctrl",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS ("application/x-rtp")
    );

static GstStaticPadTemplate gst_rtp_raop_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtp, payload = (int) 96")
    );

static GstStaticPadTemplate gst_rtp_raop_src_ctrl_template =
GST_STATIC_PAD_TEMPLATE ("src_ctrl",
    GST_PAD_SRC,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS ("application/x-rtp, payload = (int) 85")
    );

struct _GstRtpRaopPrivate {
  GstPad *sinkpad, *srcpad;
  GstPad *ctrl_sinkpad;
  GstPad *ctrl_srcpad;

  guint random_drop;
};

enum {
  PROP_0,
  PROP_RANDOM_DROP,
};

#define gst_rtp_raop_parent_class parent_class
G_DEFINE_TYPE_WITH_PRIVATE (GstRtpRaop, gst_rtp_raop, GST_TYPE_ELEMENT);

static void gst_rtp_raop_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_rtp_raop_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_rtp_raop_src_event (GstPad * pad,  GstObject * parent,
    GstEvent * event);

static gboolean gst_rtp_raop_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static GstFlowReturn gst_rtp_raop_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buf);

static GstPad *gst_rtp_raop_request_new_pad (GstElement * element,
    GstPadTemplate * template, const gchar * name, const GstCaps * filter);
static void gst_rtp_raop_release_pad (GstElement * element, GstPad * pad);

static void
gst_rtp_raop_class_init (GstRtpRaopClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_rtp_raop_set_property;
  gobject_class->get_property = gst_rtp_raop_get_property;

  g_object_class_install_property (gobject_class, PROP_RANDOM_DROP,
      g_param_spec_uint ("random-drop",
          "Drop buffers randomly in order to simulate packet lost",
          "Probability of drop (greater is less drop, 0 disable drop)", 0,
          G_MAXUINT, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_details_simple(gstelement_class,
      "RTP ROAP Muxer", "Filter/Network/RTP",
      "A multiple RTP stream muxer to handle sync packets and"
      " retransmit request/reply of RAOP protocol",
      "Alexandre Dilly <alexandre.dilly@sparod.com>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_rtp_raop_src_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_rtp_raop_src_ctrl_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_rtp_raop_sink_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_rtp_raop_sink_ctrl_template));

  gstelement_class->request_new_pad =
      GST_DEBUG_FUNCPTR (gst_rtp_raop_request_new_pad);
  gstelement_class->release_pad =
      GST_DEBUG_FUNCPTR (gst_rtp_raop_release_pad);
}

static void
gst_rtp_raop_init (GstRtpRaop * raop)
{
  GstRtpRaopPrivate *priv = gst_rtp_raop_get_instance_private (raop);

  raop->priv = priv;

  priv->sinkpad =
      gst_pad_new_from_static_template (&gst_rtp_raop_sink_template, "sink");

  gst_pad_set_event_function (priv->sinkpad,
      GST_DEBUG_FUNCPTR(gst_rtp_raop_sink_event));
  gst_pad_set_chain_function (priv->sinkpad,
      GST_DEBUG_FUNCPTR(gst_rtp_raop_chain));
  GST_PAD_SET_PROXY_CAPS (priv->sinkpad);

  priv->srcpad =
      gst_pad_new_from_static_template (&gst_rtp_raop_src_template, "src");

  gst_pad_set_event_function (priv->srcpad,
      GST_DEBUG_FUNCPTR(gst_rtp_raop_src_event));
  GST_PAD_SET_PROXY_CAPS (priv->srcpad);

  gst_element_add_pad (GST_ELEMENT (raop), priv->sinkpad);
  gst_element_add_pad (GST_ELEMENT (raop), priv->srcpad);
}

static void
gst_rtp_raop_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstRtpRaop *raop = GST_RTP_RAOP (object);
  GstRtpRaopPrivate *priv = raop->priv;

  switch (prop_id) {
    case PROP_RANDOM_DROP:
      priv->random_drop = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_rtp_raop_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstRtpRaop *raop = GST_RTP_RAOP (object);
  GstRtpRaopPrivate *priv = raop->priv;

  switch (prop_id) {
    case PROP_RANDOM_DROP:
      g_value_set_uint (value, priv->random_drop);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_rtp_raop_src_event (GstPad * pad,  GstObject * parent, GstEvent * event)
{
  GstRtpRaopPrivate *priv;
  GstRtpRaop *raop;
  gboolean ret = TRUE;

  raop = GST_RTP_RAOP (parent);
  priv = raop->priv;

  GST_DEBUG_OBJECT (raop, "received %s", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CUSTOM_UPSTREAM:
    {
      const GstStructure *req;
      GstMapInfo map;
      GstBuffer *buf;
      guint seq;

      /* only catch retransmission requests */
      if (!gst_event_has_name (event, "GstRTPRetransmissionRequest")) {
        ret = gst_pad_push_event (priv->sinkpad, event);
        break;
      }

      GST_OBJECT_LOCK (raop);
      if (priv->ctrl_srcpad) {
        /* get retransmission request */
        req = gst_event_get_structure (event);
        gst_structure_get_uint (req, "seqnum", &seq);

        GST_DEBUG_OBJECT (raop, "received GstRTPRetransmissionRequest event");

        /* generate retransmit request */
        buf = gst_buffer_new_allocate (NULL, 8, NULL);
        gst_buffer_map (buf, &map, GST_MAP_WRITE);
        map.data[0] = 0x80;
        map.data[1] = 0xD5;
        *((guint16 *) &map.data[2]) = g_ntohs (0x01);
        *((guint16 *) &map.data[4]) = g_ntohs (seq);
        *((guint16 *) &map.data[6]) = g_ntohs (1);
        gst_buffer_unmap (buf, &map);

        /* send retransmit request on control source pad */
        gst_pad_push (priv->ctrl_srcpad, buf);
      }
      GST_OBJECT_UNLOCK (raop);
      gst_event_unref (event);
      break;
    }
    default:
      ret = gst_pad_push_event (priv->sinkpad, event);
      break;
  }

  return ret;
}

static gboolean
gst_rtp_raop_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstRtpRaopPrivate *priv;
  GstRtpRaop *raop;
  gboolean ret;

  raop = GST_RTP_RAOP (parent);
  priv = raop->priv;

  GST_DEBUG_OBJECT (raop, "received %s", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    default:
      /* forward events */
      ret = gst_pad_event_default (pad, parent, event);
      break;
  }

  return ret;
}

static GstFlowReturn
gst_rtp_raop_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
  GstRtpRaopPrivate *priv;
  GstRtpRaop *raop;

  raop = GST_RTP_RAOP (parent);
  priv = raop->priv;

  /* drop randomly some packets (for test purpose) */
  if (priv->random_drop  && !g_random_int_range (0, priv->random_drop)) {
    gst_buffer_unref (buf);
    return GST_FLOW_OK;
  }

  /* simply forward buffer */
  return gst_pad_push (priv->srcpad, buf);
}

static gboolean
gst_rtp_raop_ctrl_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  gboolean ret = TRUE;
  GstRtpRaopPrivate *priv;
  GstRtpRaop *raop;

  raop = GST_RTP_RAOP (parent);
  priv = raop->priv;

  GST_DEBUG_OBJECT (raop, "received %s on ctrl", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    default:
      /* don't propagate event */
      gst_event_unref (event);
      break;
  }

  return ret;
}

static GstFlowReturn
gst_rtp_raop_ctrl_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;
  GstBuffer *out_buf = NULL;
  GstRtpRaopPrivate *priv;
  GstRtpRaop *raop;
  guint plen;
  guint8 pt;

  raop = GST_RTP_RAOP (parent);
  priv = raop->priv;

  /* map RTP buffer: not a fatal error */
  if (!gst_rtp_buffer_map (buf, GST_MAP_READ, &rtp)) {
    GST_WARNING_OBJECT (raop, "Invalid Control RTP buffer");
    gst_buffer_unref (buf);
    return GST_FLOW_OK;
  }

  /* get RTP payload */
  pt = gst_rtp_buffer_get_payload_type (&rtp);
  GST_DEBUG_OBJECT (raop, "received RTP packet %d on control", pt);

  /* get payload len */
  plen = gst_rtp_buffer_get_payload_len (&rtp);

  /* unamp buffer */
  gst_rtp_buffer_unmap (&rtp);

  switch (pt) {
    case 84:
      /* time sync packet */
      /* this packet should be send to GstClock based on RAOP timings packets */
      break;
    case 86:
      /* retransmit reply packet: get payload */
      plen = gst_buffer_get_size (buf);
      out_buf = gst_buffer_copy_region (buf, GST_BUFFER_COPY_ALL, 4, plen - 4);
      break;
    default:
      break;
  }

  gst_buffer_unref (buf);

  /* send payload from retransmit reply (an RTP packet) to source pad */
  if (out_buf)
    return gst_pad_push (priv->srcpad, out_buf);
  return GST_FLOW_OK;
}

static GstPad *
gst_rtp_raop_request_new_pad (GstElement * element, GstPadTemplate * template,
    const gchar * name, const GstCaps * filter)
{
  GstRtpRaopPrivate *priv;
  GstRtpRaop *raop;
  GstElementClass *klass;
  GstPad *pad = NULL;
  GstCaps *caps;

  g_return_val_if_fail (template != NULL, NULL);
  g_return_val_if_fail (GST_IS_RTP_RAOP (element), NULL);

  raop = GST_RTP_RAOP (element);
  priv = raop->priv;

  klass = GST_ELEMENT_GET_CLASS (element);

  GST_DEBUG_OBJECT (raop, "requesting pad %s", GST_STR_NULL (name));

  /* figure out the template */
  if (template == gst_element_class_get_pad_template (klass, "sink_ctrl")) {
    if (priv->ctrl_sinkpad) {
      GST_WARNING_OBJECT (raop, "pad already requested");
      return NULL;
    }

    /* create a new sink pad for control packets */
    pad = gst_pad_new_from_static_template (&gst_rtp_raop_sink_ctrl_template,
        "sink_ctrl");

    gst_pad_set_chain_function (pad, gst_rtp_raop_ctrl_chain);
    gst_pad_set_event_function (pad,
        (GstPadEventFunction) gst_rtp_raop_ctrl_sink_event);
    gst_pad_set_active (pad, TRUE);
    priv->ctrl_sinkpad = pad;

    /* add pad to element */
    gst_element_add_pad (element, pad);
  } else if (template == gst_element_class_get_pad_template (klass,
      "src_ctrl")) {
    if (priv->ctrl_srcpad) {
      GST_WARNING_OBJECT (raop, "pad already requested");
      return NULL;
    }

    /* create a new source pad for control packets */
    pad = gst_pad_new_from_static_template (&gst_rtp_raop_src_ctrl_template,
        "src_ctrl");

    gst_pad_use_fixed_caps (pad);
    gst_pad_set_active (pad, TRUE);

    /* add pad to element */
    gst_element_add_pad (element, pad);
    priv->ctrl_srcpad = pad;
  } else
    GST_WARNING_OBJECT (raop, "this is not our template");

  return pad;
}

static void
gst_rtp_raop_release_pad (GstElement * element, GstPad * pad)
{
  GstRtpRaopPrivate *priv;
  GstRtpRaop *raop;

  raop = GST_RTP_RAOP (element);
  priv = raop->priv;

  GST_DEBUG_OBJECT (raop, "releasing pad %s:%s", GST_DEBUG_PAD_NAME (pad));

  if (priv->ctrl_sinkpad != pad && priv->ctrl_srcpad != pad) {
    GST_WARNING_OBJECT (raop, "asked to release an unknown pad");
    return;
  }

  gst_pad_set_active (pad, FALSE);

  gst_element_remove_pad (element, pad);
  if (pad == priv->ctrl_sinkpad)
    priv->ctrl_sinkpad = NULL;
  else
    priv->ctrl_srcpad = NULL;
}

gboolean
gst_rtp_raop_plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (gst_rtp_raop_debug, "rtpraop", 0, "rtp RAOP muxer");

  return gst_element_register (plugin, "rtpraop", GST_RANK_NONE,
      GST_TYPE_RTP_RAOP);
}

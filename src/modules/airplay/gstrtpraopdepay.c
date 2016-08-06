/*
 * gstrtpraopdepay.c: RTP RAOP Depayloader
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

#include <openssl/aes.h>

#include <gst/rtp/gstrtpbuffer.h>

#include <string.h>
#include "gstrtpraopdepay.h"

GST_DEBUG_CATEGORY_STATIC (rtpraopdepay_debug);
#define GST_CAT_DEFAULT (rtpraopdepay_debug)

static GstStaticPadTemplate gst_rtp_raop_depay_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtp, "
        "clock-rate = (int) [1, MAX ]"
    )
    );

static GstStaticPadTemplate gst_rtp_raop_depay_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-alac")
    );

struct _GstRtpRaopDepayPrivate {
  gboolean has_key;
  AES_KEY key;
  guchar iv[16];
};

#define gst_rtp_raop_depay_parent_class parent_class
G_DEFINE_TYPE_WITH_PRIVATE (GstRtpRaopDepay, gst_rtp_raop_depay,
    GST_TYPE_RTP_BASE_DEPAYLOAD);

static gboolean gst_rtp_raop_depay_setcaps (GstRTPBaseDepayload * depayload,
    GstCaps * caps);
static GstBuffer *gst_rtp_raop_depay_process (GstRTPBaseDepayload * depayload,
    GstBuffer * buf);

static void gst_rtp_raop_depay_finalize (GObject * object);

static GstStateChangeReturn gst_rtp_raop_depay_change_state (GstElement *
    element, GstStateChange transition);

static void
gst_rtp_raop_depay_class_init (GstRtpRaopDepayClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstRTPBaseDepayloadClass *gstrtpbasedepayload_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstrtpbasedepayload_class = (GstRTPBaseDepayloadClass *) klass;

  gobject_class->finalize = gst_rtp_raop_depay_finalize;

  gstelement_class->change_state = gst_rtp_raop_depay_change_state;

  gstrtpbasedepayload_class->process = gst_rtp_raop_depay_process;
  gstrtpbasedepayload_class->set_caps = gst_rtp_raop_depay_setcaps;

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_rtp_raop_depay_sink_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_rtp_raop_depay_src_template));

  gst_element_class_set_static_metadata (gstelement_class,
      "RTP RAOP depayloader", "Codec/Depayloader/Network/RTP",
      "Extracts Audio from RAOP RTP packets",
      "Alexandre Dilly <dillya@sparod.com>");

  GST_DEBUG_CATEGORY_INIT (rtpraopdepay_debug, "rtpraopdepay", 0,
      "RAOP RTP Depayloader");
}

static void
gst_rtp_raop_depay_init (GstRtpRaopDepay * rtpraopdepay)
{
  GstRtpRaopDepayPrivate *priv =
      gst_rtp_raop_depay_get_instance_private (rtpraopdepay);

  rtpraopdepay->priv = priv;
}

static void
gst_rtp_raop_depay_finalize (GObject * object)
{
  GstRtpRaopDepay *rtpraopdepay = GST_RTP_RAOP_DEPAY (object);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
gst_rtp_raop_depay_parse_config (GstRtpRaopDepay * rtpraopdepay,
    const gchar *config, GstBuffer * buf)
{
  guint32 values[12];
  GstMapInfo info;
  guint8 *cfg;
  guint i;

  GST_DEBUG_OBJECT (rtpraopdepay, "parse config: %s", config);

  /* Get twelve values from config string */
  for (i = 0; i < 12; i++) {
    values[i] = strtoul (config, &config, 10);
    if (!config && i != 11)
      return FALSE;
  }

  /* Map buffer */
  if (!gst_buffer_map (buf, &info, 0))
    return FALSE;
  cfg = info.data;

  /* Fill buffer with configuration:
   *  - Atom size (4 bytes BE)
   *  - Tag name (4 bytes string)
   *  - Tag version (4 bytes BE)
   *  - Max samples per frame (4 bytes BE)
   *  - Compatible version (1 byte)
   *  - Sample size (1 bytes)
   *  - History mult (1 byte)
   *  - Initial history (1 byte)
   *  - Rice param limit (1 byte)
   *  - Channel count (1 byte)
   *  - Max run (2 bytes BE)
   *  - Max coded frame size (4 bytes BE)
   *  - Average bitrate (4 bytes BE)
   *  - Sample rate (4 bytes BE)
   */
  *((guint32*) cfg) = g_htonl (36);
  memcpy (&cfg[4], "alac", 4);
  memset (&cfg[8], 0, 4);
  *((guint32 *) &cfg[12]) = g_ntohl (values[1]);
  cfg[16] = values[2];
  cfg[17] = values[3];
  cfg[18] = values[4];
  cfg[19] = values[5];
  cfg[20] = values[6];
  cfg[21] = values[7];
  *((guint16 *) &cfg[22]) = g_ntohs (values[8]);
  *((guint32 *) &cfg[24]) = g_ntohl (values[9]);
  *((guint32 *) &cfg[28]) = g_ntohl (values[10]);
  *((guint32 *) &cfg[32]) = g_ntohl (values[11]);

  /* Unamp buffer */
  gst_buffer_unmap (buf, &info);

  return TRUE;
}

gboolean
gst_rtp_raop_depay_set_key (GstRtpRaopDepay * rtpraopdepay,
    const guchar *key, gsize key_len, const guchar *iv, gsize iv_len)
{
  GstRtpRaopDepayPrivate *priv = rtpraopdepay->priv;

  /* Check size */
  if (key_len < 128 || iv_len < 16) {
    GST_ERROR_OBJECT (rtpraopdepay, "bad keys provided");
    return FALSE;
  }

  /* Copy key data */
  AES_set_decrypt_key (key, 128, &priv->key);
  memcpy (priv->iv, iv, 16);
  priv->has_key = TRUE;

  return TRUE;
}

static gboolean
gst_rtp_raop_depay_setcaps (GstRTPBaseDepayload * depayload, GstCaps * caps)
{
  GstStructure *structure;
  GstRtpRaopDepay *rtpraopdepay;
  GstBuffer *config_buf;
  GstCaps *srccaps;
  const gchar *config;
  const gchar *b_key;
  gint clock_rate;
  gboolean res;

  rtpraopdepay = GST_RTP_RAOP_DEPAY (depayload);

  structure = gst_caps_get_structure (caps, 0);

  /* Get clock-rate */
  if (!gst_structure_get_int (structure, "clock-rate", &clock_rate))
    goto no_clock;

  /* Get AES keys */
  b_key = gst_structure_get_string (structure, "key");
  if (b_key) {
    const gchar *b_iv;
    guchar *key, *iv;
    gsize key_len, iv_len;

   /* Get AES IV */
    b_iv = gst_structure_get_string (structure, "iv");
    if (!b_iv) {
      GST_ERROR_OBJECT (rtpraopdepay, "no associated iv specified");
      return FALSE;
    }

    /* Decode AES keys from base 64 */
    key = g_base64_decode (b_key, &key_len);
    iv = g_base64_decode (b_iv, &iv_len);

    /* Set key data */
    res = gst_rtp_raop_depay_set_key (rtpraopdepay, key, key_len, iv, iv_len);
    g_free (key);
    g_free (iv);

    if (!res)
      return FALSE;
  }

  /* Get config string */
  config = gst_structure_get_string (structure, "config");
  if (!config)
    goto no_config;

  /* Allocate a new buffer for decoder configuration */
  config_buf = gst_buffer_new_allocate (NULL, 36, NULL);
  if (!config_buf)
    goto failed_config_buf;

  /* Parse configuration */
  if (!gst_rtp_raop_depay_parse_config (rtpraopdepay, config, config_buf))
    goto bad_config;

  /* Configure element */
  depayload->clock_rate = clock_rate;

  /* set caps on pad and on header */
  srccaps = gst_caps_new_simple ("audio/x-alac",
                                 "codec_data", GST_TYPE_BUFFER, config_buf,
                                 "rate", G_TYPE_INT, clock_rate,
                                 NULL);
  res = gst_pad_set_caps (depayload->srcpad, srccaps);
  gst_caps_unref (srccaps);
  gst_buffer_unref (config_buf);

  return res;

bad_config:
  GST_ERROR_OBJECT (rtpraopdepay, "bad decoder configuration");
  gst_buffer_unref (config_buf);
  return FALSE;
failed_config_buf:
  GST_ERROR_OBJECT (rtpraopdepay, "failed to allocate buffer for config");
  return FALSE;
no_config:
  GST_ERROR_OBJECT (rtpraopdepay, "no config specified");
  return FALSE;
no_clock:
  GST_ERROR_OBJECT (rtpraopdepay, "no clock-rate specified");
  return FALSE;
}

static GstBuffer *
gst_rtp_raop_depay_process (GstRTPBaseDepayload * depayload, GstBuffer * buf)
{
  GstRtpRaopDepay *rtpraopdepay;
  GstRtpRaopDepayPrivate *priv;
  GstRTPBuffer rtp = { NULL };
  GstBuffer *in_buf, *out_buf;
  gint payload_len;
  guint8 *payload;

  rtpraopdepay = GST_RTP_RAOP_DEPAY (depayload);
  priv = rtpraopdepay->priv;

  /* Map RTP buffer to read header */
  gst_rtp_buffer_map (buf, GST_MAP_READ, &rtp);

  /* Get packet len */
  payload_len = gst_rtp_buffer_get_payload_len (&rtp);
  GST_DEBUG_OBJECT (depayload, "got RTP packet of size %d", payload_len);

  /* Get payload buffer */
  in_buf = gst_rtp_buffer_get_payload_buffer (&rtp);

  /* Descrypt when a key is available */
  if (priv->has_key) {
    GstMapInfo in, out;
    guint8 *in_data, *out_data;
    gsize aes_len;
    guchar iv[16];

    /* Map packet buffer */
    gst_buffer_map (in_buf, &in, 0);
    in_data = in.data;

    /* Allocate a new buffer */
    out_buf = gst_buffer_new_allocate (NULL, payload_len, NULL);
    gst_buffer_map (out_buf, &out, GST_MAP_WRITE);
    out_data = out.data;

    /* Decrypt RTP packet with AES */
    aes_len = payload_len & ~0xf;
    memcpy (iv, priv->iv, sizeof (iv));
    AES_cbc_encrypt (in_data, out_data, aes_len, &priv->key, iv, AES_DECRYPT);
    memcpy (out_data + aes_len, in_data + aes_len, payload_len - aes_len);

    /* Unmap buffers */
    gst_buffer_unmap (in_buf, &in);
    gst_buffer_unmap (out_buf, &out);
  } else
    out_buf = in_buf;

  /* Unmap RTP buffer */
  gst_rtp_buffer_unmap (&rtp);

  return out_buf;
}

static GstStateChangeReturn
gst_rtp_raop_depay_change_state (GstElement * element,
    GstStateChange transition)
{
  GstRtpRaopDepay *rtpraopdepay;
  GstStateChangeReturn ret;

  rtpraopdepay = GST_RTP_RAOP_DEPAY (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      break;
    default:
      break;
  }
  return ret;
}

gboolean
gst_rtp_raop_depay_plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "rtpraopdepay",
      GST_RANK_SECONDARY, GST_TYPE_RTP_RAOP_DEPAY);
}

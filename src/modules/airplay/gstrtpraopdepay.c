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

#include <stdio.h>
#include <openssl/aes.h>

#include <gst/rtp/gstrtpbuffer.h>

#include <string.h>
#include "gstrtpraopdepay.h"

/* HACK: force to decode PCM as ALAC stream */
#define DECODE_PCM_AS_ALAC

GST_DEBUG_CATEGORY_STATIC (rtpraopdepay_debug);
#define GST_CAT_DEFAULT (rtpraopdepay_debug)

static GstStaticPadTemplate gst_rtp_raop_depay_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtp, "
        "clock-rate = (int) [1, MAX ], "
        "encoding-name = (string) { L16, ALAC, AAC }"
    )
    );

static GstStaticPadTemplate gst_rtp_raop_depay_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw;audio/x-alac;audio/mpeg")
    );

struct _GstRtpRaopDepayPrivate {
  gboolean has_key;
  AES_KEY key;
  guchar iv[16];
  guint32 last_rtptime;
  gint sample_size;
};

enum {
  CODEC_PCM = 0,
  CODEC_ALAC,
  CODEC_AAC,
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
gst_rtp_raop_depay_parse_pcm_config (GstRtpRaopDepay * rtpraopdepay,
    const gchar *config, guint *channels)
{
  GST_DEBUG_OBJECT (rtpraopdepay, "parse config: %s", config);

  if (channels)
    return sscanf (config, "%*d L%*d/%*d/%d", channels) == 1 ? TRUE : FALSE;
  return TRUE;
}

static gboolean
gst_rtp_raop_depay_parse_alac_config (GstRtpRaopDepay * rtpraopdepay,
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

  /* Keep sample size */
  rtpraopdepay->priv->sample_size = cfg[17];

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
  const gchar *encoding;
  const gchar *config;
  const gchar *b_key;
  guint codec = CODEC_ALAC;
  guint channels = 2;
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

  /* Get encoding-name to get codec */
  encoding = gst_structure_get_string (structure, "encoding-name");
  if (encoding) {
    if (!g_strcmp0 (encoding, "L16"))
      codec = CODEC_PCM;
    else if (!g_strcmp0 (encoding, "ALAC"))
      codec = CODEC_ALAC;
    else if (!g_strcmp0 (encoding, "AAC"))
      codec = CODEC_AAC;
  } else
    GST_WARNING_OBJECT (rtpraopdepay, "No encoding-name provided!");

  /* Get config string */
  config = gst_structure_get_string (structure, "config");
  if (!config)
    goto no_config;

  switch (codec) {
    case CODEC_PCM:
#ifndef DECODE_PCM_AS_ALAC
      /* Parse configuration */
      gst_rtp_raop_depay_parse_pcm_config (rtpraopdepay, config, &channels);

      /* Set caps on src pad */
      srccaps = gst_caps_new_simple ("audio/x-raw",
                                     "format", G_TYPE_STRING, "S16BE",
                                     "layout", G_TYPE_STRING, "interleaved",
                                     "rate", G_TYPE_INT, clock_rate,
                                     "channels", G_TYPE_INT, channels,
                                     NULL);
      res = gst_pad_set_caps (depayload->srcpad, srccaps);
      gst_caps_unref (srccaps);

      break;
#else
      /* On an iPod Touch with iOS 4, when a PCM stream is announced by the
       * iPod, a standard ALAC stream is send to AirTunes server. However, we
       * don't have configuration string, so we use a static configuration which
       * is the standard configuration used for ALAC streams.
       */
      config = "96 352 0 16 40 10 14 2 255 0 0 44100";
#endif
    case CODEC_ALAC:
      /* Allocate a new buffer for decoder configuration */
      config_buf = gst_buffer_new_allocate (NULL, 36, NULL);
      if (!config_buf)
        goto failed_config_buf;

      /* Parse configuration */
      if (!gst_rtp_raop_depay_parse_alac_config (rtpraopdepay, config,
              config_buf))
        goto bad_config;

      /* Set caps on src pad */
      srccaps = gst_caps_new_simple ("audio/x-alac",
                                     "codec_data", GST_TYPE_BUFFER, config_buf,
                                     "rate", G_TYPE_INT, clock_rate,
                                     NULL);
      res = gst_pad_set_caps (depayload->srcpad, srccaps);
      gst_caps_unref (srccaps);
      gst_buffer_unref (config_buf);

      break;
    case CODEC_AAC:
      /* Set caps on src pad */
      srccaps = gst_caps_new_simple ("audio/mpeg",
                                     "mpegversion", G_TYPE_STRING, "4",
                                     "stream-format", G_TYPE_STRING, "raw",
                                     "rate", G_TYPE_INT, clock_rate,
                                     NULL);
      res = gst_pad_set_caps (depayload->srcpad, srccaps);
      gst_caps_unref (srccaps);

      break;
  }

  /* Configure element */
  depayload->clock_rate = clock_rate;

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

static gboolean
gst_rtp_raop_depay_fix_frame (GstRtpRaopDepay * rtpraopdepay, guint8 * data,
    gsize len)
{
  guint32 size;

  /* This code is to check and fix stream from Pulseaudio RAOP module which
   * doesn't send the ALAC end tag waited by some ALAC decoders.
   */

  /* Can only chech when size and uncompressed are set */
  if (len < 7  || (data[2] & 0x12) != 0x12)
    return FALSE;

  /* Get samples count */
  size = data[3] << 23 | data[4] << 15 | data[5] << 7 | data[6] >> 1;

  /* Get bytes count:
   *  size * channel
   *  size * sample size
   */
  size *= (data[0] & 0xE0) == 0x20 ? 2 : 1;
  size *= rtpraopdepay->priv->sample_size / 8;

  /* Not enough size in buffer */
  if (size + 7 > len) {
    GST_ERROR_OBJECT (rtpraopdepay, "Cannot fix bad ALAC frame...");
    return FALSE;
   }

  /* Fix frame (we allocate 1 byte more than len) */
  data[size + 6] |= 0x01;
  data[size + 7] = 0xC0;

  return TRUE;
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

  /* Get RTP time */
  priv->last_rtptime = gst_rtp_buffer_get_timestamp (&rtp);

  /* Get packet len */
  payload_len = gst_rtp_buffer_get_payload_len (&rtp);
  GST_DEBUG_OBJECT (depayload, "got RTP packet of size %d", payload_len);

  /* Get payload buffer */
  in_buf = gst_rtp_buffer_get_payload_buffer (&rtp);

  /* Decrypt when a key is available */
  if (priv->has_key) {
    GstMapInfo in, out;
    guint8 *in_data, *out_data;
    gsize aes_len;
    guchar iv[16];

    /* Map packet buffer */
    gst_buffer_map (in_buf, &in, 0);
    in_data = in.data;

    /* Allocate a new buffer */
    out_buf = gst_buffer_new_allocate (NULL, payload_len + 1, NULL);
    gst_buffer_map (out_buf, &out, GST_MAP_WRITE);
    out_data = out.data;

    /* Decrypt RTP packet with AES */
    aes_len = payload_len & ~0xf;
    memcpy (iv, priv->iv, sizeof (iv));
    AES_cbc_encrypt (in_data, out_data, aes_len, &priv->key, iv, AES_DECRYPT);
    memcpy (out_data + aes_len, in_data + aes_len, payload_len - aes_len);

    /* Check and fix ALAC frame (Pulseaudio HACK) */
    if (!gst_rtp_raop_depay_fix_frame (rtpraopdepay, out_data, payload_len))
      gst_buffer_set_size (out_buf, payload_len);

    /* Unmap buffers */
    gst_buffer_unmap (in_buf, &in);
    gst_buffer_unmap (out_buf, &out);
  } else
    out_buf = gst_buffer_ref (in_buf);

  /* Unmap RTP buffer */
  gst_rtp_buffer_unmap (&rtp);

  /* Free RTP buffer */
  gst_buffer_unref (in_buf);

  return out_buf;
}

gboolean
gst_rtp_raop_depay_query_rtptime (GstRtpRaopDepay * rtpraopdepay,
    guint32 *rtptime)
{
  if (!rtptime || !rtpraopdepay->priv->last_rtptime)
   return FALSE;

  *rtptime = rtpraopdepay->priv->last_rtptime;
  return TRUE;
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

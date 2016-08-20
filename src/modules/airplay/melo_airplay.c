/*
 * melo_airplay.c: Airplay module for remote speakers
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
#include <netpacket/packet.h>
#include <ifaddrs.h>
#include <net/if.h>

#include <gst/sdp/sdp.h>

#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "melo_avahi.h"
#include "melo_rtsp.h"
#include "melo_config_airplay.h"

#include "melo_airplay.h"
#include "melo_airplay_pkey.h"

#include "melo_player_airplay.h"

/* Module airplay info */
static MeloModuleInfo melo_airplay_info = {
  .name = "Airplay",
  .description = "Play any media wireless on Melo",
  .config_id = "airplay",
};

/* Default Hardware address */
static guchar melo_default_hw_addr[6] = {0x00, 0x51, 0x52, 0x53, 0x54, 0x55};

static const MeloModuleInfo *melo_airplay_get_info (MeloModule *module);

static guchar *melo_airplay_base64_decode (const gchar *text, gsize *out_len);

static void melo_airplay_request_handler (MeloRTSPClient *client,
                                          MeloRTSPMethod method,
                                          const gchar *url,
                                          gpointer user_data,
                                          gpointer *data);
static void melo_airplay_read_handler (MeloRTSPClient *client, guchar *buffer,
                                       gsize size, gboolean last,
                                       gpointer user_data, gpointer *data);
static void melo_airplay_close_handler (MeloRTSPClient *client,
                                        gpointer user_data, gpointer *data);

typedef struct {
  /* Authentication */
  gboolean is_auth;
  /* Content type */
  gchar *type;
  /* Cover art */
  guchar *img;
  gsize img_size;
  gsize img_len;
  /* Format */
  MeloAirplayCodec codec;
  gchar *format;
  /* AES key and IV */
  guchar *key;
  gsize key_len;
  guchar *iv;
  gsize iv_len;
  /* RAOP configuration */
  MeloAirplayTransport transport;
  guint port;
  guint control_port;
  guint timing_port;
  gchar *client_ip;
  guint client_control_port;
  guint client_timing_port;
  /* Airplay player */
  MeloPlayer *player;
} MeloAirplayClient;

struct _MeloAirplayPrivate {
  GMutex mutex;
  MeloConfig *config;
  MeloRTSP *rtsp;
  RSA *pkey;
  gchar *password;
  MeloAvahi *avahi;
  const MeloAvahiService *service;
  guchar hw_addr[6];
  gchar *name;
  int port;

  /* Player tunning */
  guint latency;
  gint rtx_delay;
};

G_DEFINE_TYPE_WITH_PRIVATE (MeloAirplay, melo_airplay, MELO_TYPE_MODULE)

static void
melo_airplay_finalize (GObject *gobject)
{
  MeloAirplayPrivate *priv =
                     melo_airplay_get_instance_private (MELO_AIRPLAY (gobject));

  /* Free avahi client */
  if (priv->avahi)
    g_object_unref (priv->avahi);

  /* Stop and free RTSP server */
  melo_rtsp_stop (priv->rtsp);
  g_object_unref (priv->rtsp);

  /* Free private key */
  if (priv->pkey)
    RSA_free (priv->pkey);

  /* Free password */
  g_free (priv->password);

  /* Free name */
  g_free (priv->name);

  /* Clear mutex */
  g_mutex_clear (&priv->mutex);

  /* Save and free configuration */
  melo_config_save_to_def_file (priv->config);
  g_object_unref (priv->config);

  /* Chain up to the parent class */
  G_OBJECT_CLASS (melo_airplay_parent_class)->finalize (gobject);
}

static void
melo_airplay_class_init (MeloAirplayClass *klass)
{
  MeloModuleClass *mclass = MELO_MODULE_CLASS (klass);
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  mclass->get_info = melo_airplay_get_info;

  /* Add custom finalize() function */
  oclass->finalize = melo_airplay_finalize;
}

static gboolean
melo_airplay_set_hardware_address (MeloAirplayPrivate *priv)
{
  struct ifaddrs *ifap, *i;

  /* Get network interfaces */
  if (getifaddrs (&ifap))
    return FALSE;

  /* Find first MAC */
  for (i = ifap; i != NULL; i = i->ifa_next) {
    if (i && i->ifa_addr->sa_family == AF_PACKET &&
        !(i->ifa_flags & IFF_LOOPBACK)) {
      struct sockaddr_ll *s = (struct sockaddr_ll*) i->ifa_addr;
      memcpy (priv->hw_addr, s->sll_addr, 6);
      break;
    }
  }

  /* Free intarfaces list */
  freeifaddrs (ifap);

  return i ? TRUE : FALSE;
}

#define RAOP_SERVICE_TXT "tp=TCP,UDP", "sm=false", "sv=false", \
    "ek=1", "et=0,1", "cn=0,1", "ch=2", "ss=16", "sr=44100", password, "vn=3", \
    "md=0,1,2", "txtvers=1", NULL

static void
melo_airplay_update_service (MeloAirplayPrivate *priv)
{
  gchar *password;
  gchar *sname;

  /* Generate service name */
  sname = g_strdup_printf ("%02x%02x%02x%02x%02x%02x@%s", priv->hw_addr[0],
                           priv->hw_addr[1], priv->hw_addr[2], priv->hw_addr[3],
                           priv->hw_addr[4], priv->hw_addr[5], priv->name);

  /* Set password */
  password = priv->password && *priv->password != '\0' ? "pw=true" : "pw=false";

  /* Add service */
  if (!priv->service)
    priv->service = melo_avahi_add (priv->avahi, sname, "_raop._tcp",
                                    priv->port, RAOP_SERVICE_TXT);
  else
    melo_avahi_update (priv->avahi, priv->service, sname, NULL, priv->port,
                       TRUE, RAOP_SERVICE_TXT);

  /* Free service name */
  g_free (sname);
}

static void
melo_airplay_init (MeloAirplay *self)
{
  MeloAirplayPrivate *priv = melo_airplay_get_instance_private (self);
  gint64 port = 5000;
  gint64 val;
  BIO *temp_bio;

  self->priv = priv;

  /* Init mutex */
  g_mutex_init (&priv->mutex);

  /* Load configuration */
  priv->config = melo_config_airplay_new ();
  if (!melo_config_load_from_def_file (priv->config))
    melo_config_load_default (priv->config);

  /* Get name and port from configuration */
  if (!melo_config_get_string (priv->config, "general", "name", &priv->name))
    priv->name = g_strdup ("Melo");
  melo_config_get_integer (priv->config, "general", "port", &port);
  priv->port = port;
  melo_config_get_string (priv->config, "general", "password", &priv->password);

  /* Load advanced settings */
  if (melo_config_get_integer (priv->config, "advanced", "latency", &val))
    priv->latency = val;
  if (melo_config_get_integer (priv->config, "advanced", "rtx_delay", &val))
    priv->rtx_delay = val;

  /* Load RSA private key */
  temp_bio = BIO_new_mem_buf (AIRPORT_PRIVATE_KEY, -1);
  priv->pkey = PEM_read_bio_RSAPrivateKey (temp_bio, NULL, NULL, NULL);
  BIO_free (temp_bio);

  /* Set hardware address */
  if (!melo_airplay_set_hardware_address (priv))
    memcpy (priv->hw_addr, melo_default_hw_addr, 6);

  /* Create RTSP server */
  priv->rtsp = melo_rtsp_new ();
  melo_rtsp_set_request_callback (priv->rtsp, melo_airplay_request_handler,
                                  self);
  melo_rtsp_set_read_callback (priv->rtsp, melo_airplay_read_handler, self);
  melo_rtsp_set_close_callback (priv->rtsp, melo_airplay_close_handler, self);

  /* Start RTSP server */
  melo_rtsp_start (priv->rtsp, port);
  melo_rtsp_attach (priv->rtsp, g_main_context_default ());

  /* Create avahi client */
  priv->avahi = melo_avahi_new ();
  if (priv->avahi)
    melo_airplay_update_service (priv);

  /* Add config handler for update */
  melo_config_set_update_callback (priv->config, "general",
                                   melo_config_airplay_update, self);
  melo_config_set_update_callback (priv->config, "advanced",
                                   melo_config_airplay_update_advanced, self);
}

static const MeloModuleInfo *
melo_airplay_get_info (MeloModule *module)
{
  return &melo_airplay_info;
}

gboolean
melo_airplay_set_name (MeloAirplay *air, const gchar *name)
{
  MeloAirplayPrivate *priv = air->priv;

  /* Replace name */
  g_free (priv->name);
  priv->name = g_strdup (name);

  /* Update service */
  if (priv->avahi)
    melo_airplay_update_service (priv);

  return TRUE;
}

gboolean
melo_airplay_set_port (MeloAirplay *air, int port)
{
  MeloAirplayPrivate *priv = air->priv;

  /* Replace port */
  priv->port = port;

  /* Update service */
  if (priv->avahi)
    melo_airplay_update_service (priv);

  return TRUE;
}

void
melo_airplay_set_password (MeloAirplay *air, const gchar *password)
{
  MeloAirplayPrivate *priv = air->priv;

  /* Lock mutex */
  g_mutex_lock (&priv->mutex);

  /* Update password */
  g_free (priv->password);
  priv->password = g_strdup (password);

  /* Unlock mutex */
  g_mutex_unlock (&priv->mutex);

  /* Update service */
  if (priv->avahi)
    melo_airplay_update_service (priv);
}

void
melo_airplay_set_latency (MeloAirplay *air, guint latency)
{
  air->priv->latency = latency;
}

void
melo_airplay_set_rtx (MeloAirplay *air, gint rtx_delay)
{
  air->priv->rtx_delay = rtx_delay;
}

static gboolean
melo_airplay_init_apple_response (MeloRTSPClient *client, guchar *hw_addr,
                                  RSA *pkey)
{
  const gchar *challenge;
  gchar *rsa_response;
  gchar *response;
  gchar *decoded;
  guchar tmp[32];
  gsize len;

  /* Get Apple challenge */
  challenge = melo_rtsp_get_header (client, "Apple-Challenge");
  if (!challenge)
    return FALSE;

  /* Copy string and padd with '=' if missing */
  strcpy (tmp, challenge);
  if (tmp[22] == '\0') {
    tmp[22] = '=';
    tmp[23] = '=';
  } else if (tmp[23] == '\0')
    tmp[23] = '=';

  /* Decode base64 string */
  g_base64_decode_inplace (tmp, &len);
  if (len < 16)
    return FALSE;

  /* Make the response */
  memcpy (tmp + 16, melo_rtsp_get_server_ip (client), 4);
  memcpy (tmp + 20, hw_addr, 6);
  memset (tmp + 26, 0, 6);

  /* Sign response with private key */
  len = RSA_size (pkey);
  rsa_response = g_slice_alloc (len);
  RSA_private_encrypt (32, tmp, (unsigned char *) rsa_response, pkey,
                       RSA_PKCS1_PADDING);

  /* Encode response in base64 */
  response = g_base64_encode (rsa_response, len);
  g_slice_free1 (len, rsa_response);
  len = strlen (response);

  /* Remove '=' at end */
  if (response[len-2] == '=')
    response[len-2] = '\0';
  else if (response[len-1] == '=')
    response[len-1] = '\0';

  /* Add Apple-response to RTSP response */
  melo_rtsp_add_header (client, "Apple-Response", response);
  g_free (response);

  return TRUE;
}

static gboolean
melo_airplay_request_setup (MeloRTSPClient *client, MeloAirplayClient *aclient,
                            MeloAirplay *air)
{
  MeloAirplayPrivate *priv = air->priv;
  gboolean hack_sync;
  const gchar *header, *h;
  gchar *transport;
  gchar *id;

  /* Get Transport header */
  header = melo_rtsp_get_header (client, "Transport");
  if (!header)
    return FALSE;

  /* Get transport type */
  if (strstr (header, "TCP"))
    aclient->transport = MELO_AIRPLAY_TRANSPORT_TCP;
  else
    aclient->transport = MELO_AIRPLAY_TRANSPORT_UDP;

  /* Get control port */
  h = strstr (header, "control_port=");
  if (h)
    aclient->control_port = strtoul (h + 13, NULL, 10);

  /* Get timing port */
  h = strstr (header, "timing_port=");
  if (h)
    aclient->timing_port = strtoul (h + 12, NULL, 10);

  /* Set client IP and ports */
  g_free (aclient->client_ip);
  aclient->client_ip = g_strdup (melo_rtsp_get_ip_string (client));
  aclient->client_control_port = aclient->control_port;
  aclient->client_timing_port = aclient->timing_port;

  /* Generate a unique ID for its player */
  id = g_strdup_printf ("airplay_%s",
                        melo_rtsp_get_header (client, "Client-Instance"));
  if (!id)
    id = g_strdup_printf ("airplay_%s",
                          melo_rtsp_get_header (client, "DACP-ID"));

  /* Create a new player */
  aclient->player = melo_player_new (MELO_TYPE_PLAYER_AIRPLAY, id);
  melo_module_register_player (MELO_MODULE (air), aclient->player);
  g_free (id);

  /* Set latency */
  if (priv->latency)
    melo_player_airplay_set_latency (MELO_PLAYER_AIRPLAY (aclient->player),
                                     priv->latency);

  /* Set retransmit delay */
  if (priv->rtx_delay > 0)
    melo_player_airplay_set_rtx (MELO_PLAYER_AIRPLAY (aclient->player),
                                 priv->rtx_delay);

  /* Set disable sync hack */
  if (melo_config_get_boolean (priv->config, "advanced", "hack_sync",
                               &hack_sync))
    melo_player_airplay_disable_sync (MELO_PLAYER_AIRPLAY (aclient->player),
                                      hack_sync);

  /* Setup player */
  aclient->port = 6000;
  if (!melo_player_airplay_setup (MELO_PLAYER_AIRPLAY (aclient->player),
                                  aclient->transport, aclient->client_ip,
                                  &aclient->port,
                                  &aclient->control_port,
                                  &aclient->timing_port,
                                  aclient->codec, aclient->format,
                                  aclient->key, aclient->key_len,
                                  aclient->iv, aclient->iv_len)) {
    melo_rtsp_init_response (client, 500, "Internal error");
    return FALSE;
  }

  /* Prepare response */
  melo_rtsp_add_header (client, "Audio-Jack-Status", "connected; type=analog");
  if (aclient->transport == MELO_AIRPLAY_TRANSPORT_TCP)
    transport = g_strdup_printf ("RTP/AVP/TCP;unicast;interleaved=0-1;"
                                 "mode=record;server_port=%d;", aclient->port);
  else
    transport = g_strdup_printf ("RTP/AVP/UDP;unicast;interleaved=0-1;"
                                 "mode=record;control_port=%d;timing_port=%d;"
                                 "server_port=%d;", aclient->control_port,
                                 aclient->timing_port, aclient->port);
  melo_rtsp_add_header (client, "Transport", transport);
  melo_rtsp_add_header (client, "Session", "1");
  g_free (transport);

  return TRUE;
}

static void
melo_airplay_get_rtp_info (MeloRTSPClient *client, guint *seq, guint *timestamp)
{
  const gchar *header, *h;

  header = melo_rtsp_get_header (client, "RTP-Info");
  if (!header)
    return;

  /* Get next sequence number */
  h = strstr (header, "seq=");
  if (h && seq)
    *seq = strtoul (h + 4, NULL, 10);

  /* Get next timestamp */
  h = strstr(header, "rtptime=");
  if (h && timestamp)
    *timestamp = strtoul (h + 8, NULL, 10);
}

static void
melo_airplay_request_handler (MeloRTSPClient *client, MeloRTSPMethod method,
                              const gchar *url, gpointer user_data,
                              gpointer *data)
{
  MeloAirplay *air = (MeloAirplay *) user_data;
  MeloAirplayPrivate *priv = air->priv;
  MeloAirplayClient *aclient = (MeloAirplayClient *) *data;
  guint seq;

  /* Create new client */
  if (!*data) {
    aclient = g_slice_new0 (MeloAirplayClient);
    *data = aclient;
  }

  /* Lock mutex */
  g_mutex_lock (&priv->mutex);

  /* Prepare response */
  if (!aclient->is_auth && priv->password && *priv->password != '\0' &&
      !melo_rtsp_digest_auth_check (client, NULL, priv->password, priv->name)) {
    melo_rtsp_digest_auth_response (client, priv->name, NULL, 0);
    method = -1;
  } else {
    aclient->is_auth = TRUE;
    melo_rtsp_init_response (client, 200, "OK");
  }

  /* Unlock mutex */
  g_mutex_unlock (&priv->mutex);

  /* Prepare Apple response */
  melo_airplay_init_apple_response (client, priv->hw_addr, priv->pkey);

  /* Set common headers */
  melo_rtsp_add_header (client, "Server", "Melo/1.0");
  melo_rtsp_add_header (client, "CSeq", melo_rtsp_get_header (client, "CSeq"));

  /* Parse method */
  switch (method) {
    case MELO_RTSP_METHOD_OPTIONS:
      /* Set available methods */
      melo_rtsp_add_header (client, "Public", "ANNOUNCE, SETUP, RECORD, PAUSE,"
                                              "FLUSH, TEARDOWN, OPTIONS, "
                                              "GET_PARAMETER, SET_PARAMETER");
      break;
    case MELO_RTSP_METHOD_SETUP:
      /* Setup client and player */
      melo_airplay_request_setup (client, aclient, air);
      break;
    case MELO_RTSP_METHOD_RECORD:
      /* Get first RTP sequence number */
      melo_airplay_get_rtp_info (client, &seq, NULL);

      /* Start player */
      melo_player_airplay_record (MELO_PLAYER_AIRPLAY (aclient->player), seq);
      break;
    case MELO_RTSP_METHOD_TEARDOWN:
      if (aclient->player) {
        const gchar *id = melo_player_get_id (MELO_PLAYER (aclient->player));
        melo_module_unregister_player (MELO_MODULE (air), id);
        g_object_unref (aclient->player);
        aclient->player = NULL;
      }
      break;
    case MELO_RTSP_METHOD_UNKNOWN:
      if (!g_strcmp0 (melo_rtsp_get_method_name (client), "FLUSH")) {
        /* Get RTP flush sequence number */
        melo_airplay_get_rtp_info (client, &seq, NULL);

        /* Pause player */
        melo_player_airplay_flush (MELO_PLAYER_AIRPLAY (aclient->player), seq);
      }
      break;
    case MELO_RTSP_METHOD_SET_PARAMETER:
    case MELO_RTSP_METHOD_GET_PARAMETER:
      /* Save content type */
      g_free (aclient->type);
      aclient->type = g_strdup (melo_rtsp_get_header (client, "Content-Type"));
      break;
    default:
     ;
  }
}

static gboolean
melo_airplay_read_announce (guchar *buffer, gsize size,
                            MeloAirplayPrivate *priv,
                            MeloAirplayClient *aclient)
{
  const GstSDPMedia *media = NULL;
  GstSDPMessage *sdp;
  const char *rtpmap = NULL;
  const gchar *val;
  gboolean ret = FALSE;
  guint i, count;
  gsize len;

  /* Init SDP message */
  gst_sdp_message_new (&sdp);
  gst_sdp_message_init (sdp);

  /* Parse SDP packet */
  if (gst_sdp_message_parse_buffer (buffer, size, sdp) != GST_SDP_OK)
    goto end;

  /* Get audio media */
  count = gst_sdp_message_medias_len (sdp);
  for (i = 0; i < count; i++) {
    const GstSDPMedia *m = gst_sdp_message_get_media (sdp, i);
    if (!g_strcmp0 (gst_sdp_media_get_media (m), "audio")) {
      media = m;
      break;
    }
  }
  if (!media)
    goto end;

  /* Parse all attributes */
  count = gst_sdp_media_attributes_len (media);
  for (i = 0; i < count; i++) {
    const GstSDPAttribute *attr = gst_sdp_media_get_attribute (media, i);

    /* Find rtpmap, ftmp, rsaaeskey and aesiv */
    if (!g_strcmp0 (attr->key, "rtpmap")) {
      /* Get codec */
      rtpmap = attr->value;
      const gchar *codec = attr->value + 3;

      /* Find codec */
      if (!strncmp (codec, "L16", 3))
        aclient->codec = MELO_AIRPLAY_CODEC_PCM;
      else if (!strncmp (codec, "AppleLossless", 13))
        aclient->codec = MELO_AIRPLAY_CODEC_ALAC;
      else if (!strncmp (codec, "mpeg4-generic", 13))
        aclient->codec = MELO_AIRPLAY_CODEC_AAC;
      else
        goto end;
    } else if (!g_strcmp0 (attr->key, "fmtp")) {
      /* Get format string */
      g_free (aclient->format);
      aclient->format = g_strdup (attr->value);
    } else if (!g_strcmp0 (attr->key, "rsaaeskey")) {
      gchar *key;

      /* Decode AES key from base64 */
      key = melo_airplay_base64_decode (attr->value, &len);

      /* Allocate new AES key */
      aclient->key_len = RSA_size (priv->pkey);
      if (!aclient->key)
        aclient->key = g_slice_alloc (aclient->key_len);
      if (!aclient->key) {
        g_free (key);
        goto end;
      }

      /* Decrypt AES key */
      if (!RSA_private_decrypt (len, (unsigned char *) key,
                                (unsigned char *) aclient->key, priv->pkey,
                                RSA_PKCS1_OAEP_PADDING)) {
        g_free (key);
        goto end;
      }
      g_free (key);
    } else if (!g_strcmp0 (attr->key, "aesiv")) {
      /* Get AES IV */
      g_free (aclient->iv);
      aclient->iv = melo_airplay_base64_decode (attr->value, &aclient->iv_len);
    }
  }

  /* Add a pseudo format for PCM */
  if (aclient->codec == MELO_AIRPLAY_CODEC_PCM && !aclient->format)
    aclient->format = g_strdup (rtpmap);

  /* A format and a key has been found */
  if (aclient->format && aclient->key)
    ret = TRUE;
end:
  /* Free SDP message */
  gst_sdp_message_free (sdp);

  return ret;
}

static gboolean
melo_rtsp_read_params (MeloAirplayClient *aclient, guchar *buffer, gsize size)
{
  MeloPlayerAirplay *pair = MELO_PLAYER_AIRPLAY (aclient->player);
  gchar *value;

  /* Check buffer content */
  if (size > 8 && !strncmp (buffer, "volume: ", 8)) {
    gdouble volume;

    /* Get volume from buffer */
    value = g_strndup (buffer + 8, size - 8);
    volume = g_strtod (value, NULL);
    g_free (value);

    /* Set volume */
    melo_player_airplay_set_volume (pair, volume);
  } else if (size > 10 && !strncmp (buffer, "progress: ", 10)) {
    guint start, cur, end;
    gchar *v;

    /* Get RTP time values */
    value = g_strndup (buffer + 10, size - 10);
    start = strtoul (value, &v, 10);
    cur = strtoul (v + 1, &v, 10);
    end = strtoul (v + 1, NULL, 10);
    g_free (value);

    /* Set position and duration */
    melo_player_airplay_set_progress (pair, start, cur, end);
  } else
    return FALSE;

  return TRUE;
}

static gboolean
melo_rtsp_read_tags (MeloAirplayClient *aclient, guchar *buffer, gsize size)
{
  MeloTags *tags;
  gsize len;

  /* Skip first header */
  if (size > 8 && !memcmp (buffer, "mlit", 4)) {
    buffer += 8;
    size -= 8;
  }

  /* Create a new tags */
  tags = melo_tags_new ();
  if (!tags)
    return FALSE;

  /* Parse all buffer */
  while (size > 8) {
    /* Get tag length */
    len = buffer[4] << 24 | buffer[5] << 16 | buffer[6] << 8 | buffer[7];

    /* Get values */
    if (!memcmp (buffer, "minm", 4)) {
      g_free (tags->title);
      tags->title = g_strndup (buffer + 8, len);
    } else if (!memcmp (buffer, "asar", 4)) {
      g_free (tags->artist);
      tags->artist = g_strndup (buffer + 8, len);
    } else if (!memcmp (buffer, "asal", 4)) {
      g_free (tags->album);
      tags->album = g_strndup (buffer + 8, len);
    }

    /* Go to next block */
    buffer += len + 8;
    size -= len + 8;
  }

  /* Update tags in player */
  melo_player_play (aclient->player, NULL, NULL, tags, TRUE);

  return TRUE;
}

static gboolean
melo_rtsp_read_image (MeloRTSPClient *client, MeloAirplayClient *aclient,
                      guchar *buffer, gsize size, gboolean last)
{
  GBytes *cover;

  /* First packet */
  if (!aclient->img) {
    aclient->img_len = 0;
    aclient->img_size = melo_rtsp_get_content_length (client);
    aclient->img = g_malloc (aclient->img_size);
    if (!aclient->img)
      return FALSE;
  }

  /* Copy data */
  memcpy (aclient->img + aclient->img_len, buffer, size);
  aclient->img_len += size;

  /* Last packet */
  if (last) {
    /* Create cover GBytes */
    cover = g_bytes_new_take (aclient->img, aclient->img_size);
    aclient->img_size = 0;
    aclient->img = NULL;

    /* Send final image to player */
    melo_player_airplay_set_cover (MELO_PLAYER_AIRPLAY (aclient->player),
                                   cover, aclient->type);
  }

  return TRUE;
}

static gboolean
melo_rtsp_write_params (MeloRTSPClient *client, MeloAirplayClient *aclient,
                        guchar *buffer, gsize size)
{
  MeloPlayerAirplay *pair = MELO_PLAYER_AIRPLAY (aclient->player);
  gchar *value;

  /* Check buffer content */
  if (size > 6 && !strncmp (buffer, "volume", 6)) {
    gdouble volume;
    gchar *packet;
    gsize len;

    /* Get volume */
    volume = melo_player_airplay_get_volume (pair);

    /* Add headers for content type and length */
    melo_rtsp_add_header (client, "Content-Type", "text/parameters");

    /* Create and add response body */
    packet = g_strdup_printf ("volume: %.6f\r\n", volume);
    len = strlen (packet);
    melo_rtsp_set_packet (client, packet, len, (GDestroyNotify) g_free);
  } else
    return FALSE;

  return TRUE;
}

static void
melo_airplay_read_handler (MeloRTSPClient *client, guchar *buffer, gsize size,
                           gboolean last, gpointer user_data, gpointer *data)
{
  MeloAirplay *air = (MeloAirplay *) user_data;
  MeloAirplayPrivate *priv = air->priv;
  MeloAirplayClient *aclient = (MeloAirplayClient *) *data;
  const gchar *type;

  /* Parse method */
  switch (melo_rtsp_get_method (client)) {
    case MELO_RTSP_METHOD_ANNOUNCE:
      melo_airplay_read_announce (buffer, size, priv, aclient);
      break;
    case MELO_RTSP_METHOD_SET_PARAMETER:
      /* Get content type */
      if (!aclient->type)
        break;

      /* Parse content type */
      if (!g_strcmp0 (aclient->type, "text/parameters"))
        /* Get parameters (volume or progress) */
        melo_rtsp_read_params (aclient, buffer, size);
      else if (!g_strcmp0 (aclient->type, "application/x-dmap-tagged"))
        /* Get media tags */
        melo_rtsp_read_tags (aclient, buffer, size);
      else if (g_str_has_prefix (aclient->type, "image/"))
        /* Get cover art */
        melo_rtsp_read_image (client, aclient, buffer, size, last);

      break;
    case MELO_RTSP_METHOD_GET_PARAMETER:
      /* Get content type */
      if (!aclient->type)
        break;

      /* Get volume */
      if (!g_strcmp0 (aclient->type, "text/parameters"))
        melo_rtsp_write_params (client, aclient, buffer, size);

      break;
    default:
     ;
  }
}

static void
melo_airplay_close_handler (MeloRTSPClient *client, gpointer user_data,
                            gpointer *data)
{
  MeloAirplay *air = (MeloAirplay *) user_data;
  MeloAirplayPrivate *priv = air->priv;
  MeloAirplayClient *aclient = (MeloAirplayClient *) *data;

  if (!aclient)
    return;

  /* Remove player */
  if (aclient->player) {
    const gchar *id = melo_player_get_id (MELO_PLAYER (aclient->player));
    melo_module_unregister_player (MELO_MODULE (air), id);
    g_object_unref (aclient->player);
  }

  /* Free AES key */
  if (aclient->key)
    g_slice_free1 (aclient->key_len, aclient->key);
  g_free (aclient->iv);

  /* Free client IP */
  g_free (aclient->client_ip);

  /* Free format */
  g_free (aclient->format);

  /* Free type */
  g_free (aclient->type);

  /* Free image */
  g_free (aclient->img);

  /* Free stream */
  g_slice_free (MeloAirplayClient, aclient);
}

static guchar *
melo_airplay_base64_decode (const gchar *text, gsize *out_len)
{
  gint state = 0;
  guint save = 0;
  guchar *out;
  gsize len;

  /* Allocate output buffer */
  len = strlen (text);
  out = g_malloc ((len * 3 / 4) + 3);

  /* Decode string */
  len = g_base64_decode_step (text, len, out, &state, &save);
  while (state)
    len += g_base64_decode_step ("=", 1, out + len, &state, &save);

  /* Return values */
  *out_len = len;
  return out;
}

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

#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "melo_avahi.h"
#include "melo_rtsp.h"
#include "melo_config_airplay.h"

#include "melo_airplay.h"
#include "melo_airplay_pkey.h"

/* Module airplay info */
static MeloModuleInfo melo_airplay_info = {
  .name = "Airplay",
  .description = "Play any media wireless on Melo",
  .config_id = "airplay",
};

/* Default Hardware address */
static guchar melo_default_hw_addr[6] = {0x00, 0x51, 0x52, 0x53, 0x54, 0x55};

static const MeloModuleInfo *melo_airplay_get_info (MeloModule *module);
static void melo_airplay_request_handler (MeloRTSPClient *client,
                                          MeloRTSPMethod method,
                                          const gchar *url,
                                          gpointer user_data,
                                          gpointer *data);

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

static void
melo_airplay_update_service (MeloAirplayPrivate *priv)
{
  gchar *sname;

  /* Generate service name */
  sname = g_strdup_printf ("%02x%02x%02x%02x%02x%02x@%s", priv->hw_addr[0],
                           priv->hw_addr[1], priv->hw_addr[2], priv->hw_addr[3],
                           priv->hw_addr[4], priv->hw_addr[5], priv->name);

  /* Add service */
  if (!priv->service)
    priv->service = melo_avahi_add (priv->avahi, sname, "_raop._tcp",
                                    priv->port,
                                    "tp=TCP,UDP", "sm=false", "sv=false",
                                    "ek=1", "et=0,1", "cn=0,1", "ch=2", "ss=16",
                                    "sr=44100", "pw=false", "vn=3", "md=0,1,2",
                                    "txtvers=1", NULL);
  else
    melo_avahi_update (priv->avahi, priv->service, sname, NULL, priv->port,
                       NULL);

  /* Free service name */
  g_free (sname);
}

static void
melo_airplay_init (MeloAirplay *self)
{
  MeloAirplayPrivate *priv = melo_airplay_get_instance_private (self);
  gint64 port = 5000;
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
                                  priv);

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

static void
melo_airplay_request_handler (MeloRTSPClient *client, MeloRTSPMethod method,
                              const gchar *url, gpointer user_data,
                              gpointer *data)
{
  MeloAirplayPrivate *priv = (MeloAirplayPrivate *) user_data;

  /* Lock mutex */
  g_mutex_lock (&priv->mutex);

  /* Prepare response */
  if (priv->password && *priv->password != '\0' &&
      !melo_rtsp_digest_auth_check (client, NULL, priv->password, priv->name)) {
    melo_rtsp_digest_auth_response (client, priv->name, "", 0);
    method = -1;
  } else
    melo_rtsp_init_response (client, 200, "OK");

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
    default:
     ;
  }
}

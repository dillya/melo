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

#include "melo_avahi.h"

#include "melo_airplay.h"

/* Module airplay info */
static MeloModuleInfo melo_airplay_info = {
  .name = "Airplay",
  .description = "Play any media wireless on Melo",
  .config_id = "airplay",
};

/* Default Hardware address */
static guchar melo_default_hw_addr[6] = {0x00, 0x51, 0x52, 0x53, 0x54, 0x55};

static const MeloModuleInfo *melo_airplay_get_info (MeloModule *module);

struct _MeloAirplayPrivate {
  MeloAvahi *avahi;
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

  /* Free name */
  g_free (priv->name);

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
melo_airplay_init (MeloAirplay *self)
{
  MeloAirplayPrivate *priv = melo_airplay_get_instance_private (self);

  self->priv = priv;
  priv->name = g_strdup ("Melo");
  priv->port = 5000;

  /* Set hardware address */
  if (!melo_airplay_set_hardware_address (priv))
    memcpy (priv->hw_addr, melo_default_hw_addr, 6);

  /* Create avahi client */
  priv->avahi = melo_avahi_new ();
  if (priv->avahi) {
    gchar *sname;

    /* Generate service name */
    sname = g_strdup_printf ("%02x%02x%02x%02x%02x%02x@%s",
                                    priv->hw_addr[0], priv->hw_addr[1],
                                    priv->hw_addr[2], priv->hw_addr[3],
                                    priv->hw_addr[4], priv->hw_addr[5],
                                    priv->name);

    /* Add service */
    melo_avahi_add (priv->avahi, sname, "_raop._tcp", priv->port,
                    "tp=TCP,UDP", "sm=false", "sv=false", "ek=1", "et=0,1",
                    "cn=0,1", "ch=2", "ss=16", "sr=44100", "pw=false", "vn=3",
                    "md=0,1,2", "txtvers=1", NULL);
    g_free (sname);
  }
}

static const MeloModuleInfo *
melo_airplay_get_info (MeloModule *module)
{
  return &melo_airplay_info;
}

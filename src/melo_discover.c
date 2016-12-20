/*
 * melo_discover.c: A Melo device discoverer
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

#include <linux/if_packet.h>
#include <sys/types.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <arpa/inet.h>

#include <libsoup/soup.h>

#include "melo_discover.h"

#define MELO_DISCOVER_URL "http://www.sparod.com/melo/discover.php"

struct _MeloDiscoverPrivate {
  SoupSession *session;
};

typedef struct {
  const gchar *name;
  gchar *hw_address;
  gchar *address;
} MeloDiscoverInterface;

G_DEFINE_TYPE_WITH_PRIVATE (MeloDiscover, melo_discover, G_TYPE_OBJECT)

static void
melo_discover_finalize (GObject *gobject)
{
  MeloDiscover *disco = MELO_DISCOVER (gobject);
  MeloDiscoverPrivate *priv = melo_discover_get_instance_private (disco);

  /* Free Soup session */
  g_object_unref (priv->session);

  /* Chain up to the parent class */
  G_OBJECT_CLASS (melo_discover_parent_class)->finalize (gobject);
}

static void
melo_discover_class_init (MeloDiscoverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  /* Add custom finalize() function */
  object_class->finalize = melo_discover_finalize;
}

static void
melo_discover_init (MeloDiscover *self)
{
  MeloDiscoverPrivate *priv = melo_discover_get_instance_private (self);

  self->priv = priv;

  /* Create a new Soup session */
  priv->session = soup_session_new_with_options (
                                SOUP_SESSION_USER_AGENT, "Melo",
                                NULL);
}

MeloDiscover *
melo_discover_new ()
{
  return g_object_new (MELO_TYPE_DISCOVER, NULL);
}

static gchar *
melo_discover_get_hw_address (struct sockaddr_ll *s)
{
  return g_strdup_printf ("%02x:%02x:%02x:%02x:%02x:%02x", s->sll_addr[0],
                          s->sll_addr[1], s->sll_addr[2], s->sll_addr[3],
                          s->sll_addr[4], s->sll_addr[5]);
}

static gchar *
melo_discover_get_address (struct sockaddr_in *s)
{
  gchar *address;

  /* Get address string */
  address = g_malloc (INET_ADDRSTRLEN);
  if (address)
    inet_ntop(AF_INET, &s->sin_addr, address, INET_ADDRSTRLEN);

  return address;
}

static gchar *
melo_discover_get_serial (struct ifaddrs *ifap)
{
  struct ifaddrs *i;

  /* Get first hardware address for serial */
  for (i = ifap; i != NULL; i = i->ifa_next) {
    if (i && i->ifa_addr->sa_family == AF_PACKET &&
        !(i->ifa_flags & IFF_LOOPBACK)) {
      struct sockaddr_ll *s = (struct sockaddr_ll*) i->ifa_addr;
      return melo_discover_get_hw_address (s);
    }
  }

  return NULL;
}

static MeloDiscoverInterface *
melo_discover_interface_get (GList **ifaces, const gchar *name)
{
  MeloDiscoverInterface *iface;
  GList *l;

  /* Find interface in list */
  for (l = *ifaces; l != NULL; l = l->next) {
    iface = l->data;
    if (!g_strcmp0 (iface->name, name))
      return iface;
  }

  /* Create a new item */
  iface = g_slice_new0 (MeloDiscoverInterface);
  if (iface) {
    iface->name = name;
    *ifaces = g_list_prepend (*ifaces, iface);
  }

  return iface;
}

gboolean
melo_discover_register_device (MeloDiscover *disco, const gchar *name,
                               guint port)
{
  MeloDiscoverPrivate *priv = disco->priv;
  MeloDiscoverInterface *iface;
  GList *l, *ifaces = NULL;
  struct ifaddrs *ifap, *i;
  gchar *serial, *req;
  const gchar *host;
  SoupMessage *msg;

  /* Get network interfaces */
  if (getifaddrs (&ifap))
    return FALSE;

  /* Get serial */
  serial = melo_discover_get_serial (ifap);

  /* Get hostname */
  host = g_get_host_name ();

  /* Prepare request for device registration */
  req = g_strdup_printf (MELO_DISCOVER_URL "?action=add_device&"
                         "serial=%s&name=%s&hostname=%s&port=%u",
                         serial, name, host, port);

  /* Register device on Melo website */
  msg = soup_message_new ("GET", req);
  soup_session_send_message (priv->session, msg);
  g_object_unref (msg);
  g_free (req);

  /* List all interfaces */
  for (i = ifap; i != NULL; i = i->ifa_next) {
    /* Skip loopback interface */
    if (i->ifa_flags & IFF_LOOPBACK || !i->ifa_addr)
      continue;

    /* Get addresses */
    if (i->ifa_addr->sa_family == AF_PACKET) {
      struct sockaddr_ll *s = (struct sockaddr_ll *) i->ifa_addr;

      /* Find interface in list */
      iface = melo_discover_interface_get (&ifaces, i->ifa_name);
      if (!iface)
        continue;

      /* Get hardware address */
      iface->hw_address = melo_discover_get_hw_address (s);
    } else if (i->ifa_addr->sa_family == AF_INET) {
      struct sockaddr_in *s = (struct sockaddr_in *) i->ifa_addr;

      /* Find interface in list */
      iface = melo_discover_interface_get (&ifaces, i->ifa_name);
      if (!iface)
        continue;

      /* Get hardware address */
      iface->address = melo_discover_get_address (s);
    }
  }

  /* Add device addresses on Sparod */
  for (l = ifaces; l != NULL; l = l->next) {
    /* Get interface */
    iface = l->data;

    /* Prepare request for address registration */
    req = g_strdup_printf (MELO_DISCOVER_URL "?action=add_address&"
                           "serial=%s&hw_address=%s&address=%s",
                           serial, iface->hw_address, iface->address);

    /* Add device address on Melo website */
    msg = soup_message_new ("GET", req);
    soup_session_send_message (priv->session, msg);
    g_object_unref (msg);
    g_free (iface->hw_address);
    g_free (iface->address);
    g_slice_free (MeloDiscoverInterface, iface);
    g_free (req);
  }

  /* Free intarfaces list */
  g_list_free (ifaces);
  freeifaddrs (ifap);
  g_free (serial);

  return TRUE;
}

gboolean
melo_discover_unregister_device (MeloDiscover *disco)
{
  MeloDiscoverPrivate *priv = disco->priv;
  struct ifaddrs *ifap;
  gchar *serial, *req;
  SoupMessage *msg;

  /* Get network interfaces */
  if (getifaddrs (&ifap))
    return FALSE;

  /* Get serial */
  serial = melo_discover_get_serial (ifap);

  /* Prepare request for device removal */
  req = g_strdup_printf (MELO_DISCOVER_URL "?action=remove_device&serial=%s",
                         serial);

  /* Unregister device from Melo website */
  msg = soup_message_new ("GET", req);
  soup_session_send_message (priv->session, msg);
  g_object_unref (msg);
  g_free (serial);
  g_free (req);

  /* Free intarfaces list */
  freeifaddrs (ifap);

  return TRUE;
}

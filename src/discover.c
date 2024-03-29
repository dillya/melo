/*
 * Copyright (C) 2020 Alexandre Dilly <dillya@sparod.com>
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA.
 */

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <linux/if_packet.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <sys/types.h>

#include <glib-unix.h>

#include <melo/melo_http_client.h>
#include <melo/melo_mdns.h>

#define MELO_LOG_TAG "melo_discover"
#include <melo/melo_log.h>

#include "discover.h"

#define DISCOVER_BUFFER_SIZE 4096
#define DISCOVER_URL "https://www.sparod.com/melo/discover.php"

typedef struct {
  char *name;
  char *hw_address;
  char *address;
} DiscoverInterface;

static MeloHttpClient *discover_client;
static MeloMdns *discover_mdns;
static const MeloMdnsService *discover_http_service;
static const MeloMdnsService *discover_https_service;
static bool discover_registered;
static char *discover_serial;
static char *discover_device_name;
static unsigned int discover_http_port;
static unsigned int discover_https_port;
static int discover_ntlk_fd = -1;
static gulong discover_ntlk_id = 0;
static GList *discover_ifaces;

static char *discover_get_hw_address (unsigned char *addr);

static void discover_interface_free (DiscoverInterface *iface);

static gboolean netlink_cb (
    gint fd, GIOCondition condition, gpointer user_data);

/**
 * discover_init:
 *
 * Initialize the discover module.
 * It will generate a serial number from the hardware address of the first
 * network interface found.
 * It will also open a netlink socket to monitor network interfaces and to
 * add / remove network interfaces on runtime.
 */
void
discover_init (void)
{
  struct ifaddrs *ifap;

  /* Get network interfaces list */
  if (!getifaddrs (&ifap)) {
    struct ifaddrs *i;

    /* Get first hardware address for serial */
    for (i = ifap; i != NULL; i = i->ifa_next) {
      if (i && i->ifa_addr->sa_family == AF_PACKET &&
          !(i->ifa_flags & IFF_LOOPBACK)) {
        struct sockaddr_ll *s = (struct sockaddr_ll *) i->ifa_addr;
        discover_serial = discover_get_hw_address (s->sll_addr);
        break;
      }
    }

    /* Free interfaces list */
    freeifaddrs (ifap);
  }

  /* Create a new HTTP client */
  discover_client = melo_http_client_new (NULL);
  if (discover_client) {
    /* Limit to one connection at once for atomicity */
    melo_http_client_set_max_connections (discover_client, 1);
  } else
    MELO_LOGE ("failed to create HTTP client");

  /* Open netlink socket to monitor interfaces */
  discover_ntlk_fd = socket (AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
  if (discover_ntlk_fd > 0) {
    struct sockaddr_nl sa;

    /* Set netlink socket to monitor interfaces and their addresses */
    memset (&sa, 0, sizeof (sa));
    sa.nl_family = AF_NETLINK;
    sa.nl_groups = RTMGRP_LINK | RTMGRP_IPV4_IFADDR;
    if (bind (discover_ntlk_fd, (struct sockaddr *) &sa, sizeof (sa)))
      MELO_LOGW ("failed to bind netlink socket");

    /* Add netlink socket source event */
    discover_ntlk_id =
        g_unix_fd_add (discover_ntlk_fd, G_IO_IN, netlink_cb, NULL);
  }

  /* Create mdns client */
  discover_mdns = melo_mdns_new ();
  if (!discover_mdns)
    MELO_LOGW ("failed to create mdns client");
}

/**
 * discover_exit:
 *
 * Release and cleanup all resources allocated and used by discover module.
 */
void
discover_exit (void)
{
  /* Close netlink socket */
  if (discover_ntlk_fd != -1) {
    g_source_remove (discover_ntlk_id);
    close (discover_ntlk_fd);
  }
  discover_ntlk_fd = -1;

  /* Free interfaces list */
  g_list_free_full (discover_ifaces, (GDestroyNotify) discover_interface_free);

  /* Destroy mdns client */
  if (discover_mdns)
    g_object_unref (discover_mdns);
  discover_mdns = NULL;

  /* Destroy HTTP client */
  if (discover_client)
    g_object_unref (discover_client);
  discover_client = NULL;

  /* Free name */
  g_free (discover_device_name);

  /* Free serial */
  g_free (discover_serial);
  discover_serial = NULL;
}

static char *
discover_get_hw_address (unsigned char *addr)
{
  return g_strdup_printf ("%02x:%02x:%02x:%02x:%02x:%02x", addr[0], addr[1],
      addr[2], addr[3], addr[4], addr[5]);
}

static gchar *
discover_get_address (struct in_addr *addr)
{
  gchar *address;

  /* Get address string */
  address = g_malloc (INET_ADDRSTRLEN);
  if (address)
    inet_ntop (AF_INET, addr, address, INET_ADDRSTRLEN);

  return address;
}

static DiscoverInterface *
discover_interface_get (const char *name)
{
  DiscoverInterface *iface;
  GList *l;

  /* Find interface in list */
  for (l = discover_ifaces; l != NULL; l = l->next) {
    iface = l->data;
    if (!g_strcmp0 (iface->name, name))
      return iface;
  }

  /* Create a new item */
  iface = g_slice_new0 (DiscoverInterface);
  if (iface) {
    iface->name = g_strdup (name);
    discover_ifaces = g_list_prepend (discover_ifaces, iface);
  }

  return iface;
}

static void
discover_interface_free (DiscoverInterface *iface)
{
  g_free (iface->name);
  g_free (iface->hw_address);
  g_free (iface->address);
  g_slice_free (DiscoverInterface, iface);
}

static void
discover_address_cb (MeloHttpClient *client, unsigned int code,
    const char *data, size_t size, void *user_data)
{
  if (code != 200)
    discover_registered = false;
}

static bool
discover_add_address (DiscoverInterface *iface)
{
  char *url;

  /* Prepare request for address registration */
  url = g_strdup_printf (DISCOVER_URL
      "?action=add_address&serial=%s&hw_address=%s&address=%s",
      discover_serial, iface->hw_address, iface->address);

  /* Send request */
  melo_http_client_get (discover_client, url, discover_address_cb, NULL);
  g_free (url);

  return true;
}

static bool
discover_remove_address (DiscoverInterface *iface)
{
  char *url;

  /* Prepare request for address removal */
  url = g_strdup_printf (DISCOVER_URL
      "?action=remove_address&serial=%s&hw_address=%s",
      discover_serial, iface->hw_address);

  /* Send request */
  melo_http_client_get (discover_client, url, discover_address_cb, NULL);
  g_free (url);

  return true;
}

static gboolean
netlink_cb (gint fd, GIOCondition condition, gpointer user_data)
{
  char buffer[DISCOVER_BUFFER_SIZE];
  struct nlmsghdr *nh;
  ssize_t len;

  /* Get next message from netlink socket */
  len = recv (fd, buffer, DISCOVER_BUFFER_SIZE, 0);
  if (len <= 0)
    return FALSE;

  /* Device not registered */
  if (!discover_registered)
    discover_register_device (
        discover_device_name, discover_http_port, discover_https_port);

  /* Process messages */
  for (nh = (struct nlmsghdr *) buffer; NLMSG_OK (nh, len);
       nh = NLMSG_NEXT (nh, len)) {
    DiscoverInterface *iface;
    char name[IF_NAMESIZE];
    struct rtattr *ra;
    int rlen;

    /* Process message */
    switch (nh->nlmsg_type) {
    case RTM_NEWLINK: {
      struct ifinfomsg *msg = (struct ifinfomsg *) NLMSG_DATA (nh);

      /* Get interface */
      if_indextoname (msg->ifi_index, name);
      iface = discover_interface_get (name);

      /* Update interface */
      if (iface) {
        /* Extract hardware address */
        ra = IFLA_RTA (msg);
        rlen = IFLA_PAYLOAD (nh);
        for (; rlen && RTA_OK (ra, rlen); ra = RTA_NEXT (ra, rlen)) {
          if (ra->rta_type != IFLA_ADDRESS)
            continue;

          /* Set hardware address */
          g_free (iface->hw_address);
          iface->hw_address =
              discover_get_hw_address ((unsigned char *) RTA_DATA (ra));
        }
      }
      break;
    }
    case RTM_DELLINK:
      break;
    case RTM_NEWADDR: {
      struct ifaddrmsg *msg = (struct ifaddrmsg *) NLMSG_DATA (nh);
      struct in_addr addr;

      /* Get interface */
      if_indextoname (msg->ifa_index, name);
      iface = discover_interface_get (name);

      /* Update IP address */
      if (iface) {
        /* Extract local address */
        ra = IFA_RTA (msg);
        rlen = IFA_PAYLOAD (nh);
        for (; rlen && RTA_OK (ra, rlen); ra = RTA_NEXT (ra, rlen)) {
          if (ra->rta_type != IFA_LOCAL)
            continue;

          /* Get address */
          addr.s_addr = *((uint32_t *) RTA_DATA (ra));

          /* Set address */
          g_free (iface->address);
          iface->address = discover_get_address (&addr);
          if (iface->hw_address)
            discover_add_address (iface);
        }
      }
      break;
    }
    case RTM_DELADDR: {
      struct ifaddrmsg *msg = (struct ifaddrmsg *) NLMSG_DATA (nh);

      /* Get interface */
      if_indextoname (msg->ifa_index, name);
      iface = discover_interface_get (name);

      /* Remove IP address */
      if (iface) {
        g_free (iface->address);
        iface->address = NULL;
        if (iface->hw_address)
          discover_remove_address (iface);
      }
      break;
    }
    case NLMSG_DONE:
    case NLMSG_ERROR:
      goto end;
    default:
      break;
    }
  }

end:
  return TRUE;
}

static void
discover_register_cb (MeloHttpClient *client, unsigned int code,
    const char *data, size_t size, void *user_data)
{
  struct ifaddrs *ifap, *i;
  DiscoverInterface *iface;
  GList *l;

  /* Failed to register device */
  if (code != 200)
    return;

  /* Device registered */
  discover_registered = true;

  /* Get network interfaces list */
  if (getifaddrs (&ifap))
    return;

  /* List all interfaces */
  for (i = ifap; i != NULL; i = i->ifa_next) {
    /* Skip loopback interface */
    if (i->ifa_flags & IFF_LOOPBACK || !i->ifa_addr)
      continue;

    /* Get addresses */
    if (i->ifa_addr->sa_family == AF_PACKET) {
      struct sockaddr_ll *s = (struct sockaddr_ll *) i->ifa_addr;

      /* Find interface in list */
      iface = discover_interface_get (i->ifa_name);
      if (!iface)
        continue;

      /* Get hardware address */
      g_free (iface->hw_address);
      iface->hw_address = discover_get_hw_address (s->sll_addr);
    } else if (i->ifa_addr->sa_family == AF_INET) {
      struct sockaddr_in *s = (struct sockaddr_in *) i->ifa_addr;

      /* Find interface in list */
      iface = discover_interface_get (i->ifa_name);
      if (!iface)
        continue;

      /* Get address */
      g_free (iface->address);
      iface->address = discover_get_address (&s->sin_addr);
    }
  }

  /* Add device addresses on Sparod */
  for (l = discover_ifaces; l != NULL; l = l->next) {
    /* Get interface */
    iface = l->data;

    /* Add or remove device address on Sparod */
    if (iface->hw_address) {
      if (iface->address)
        discover_add_address (iface);
      else
        discover_remove_address (iface);
    }
  }

  /* Free interfaces list */
  freeifaddrs (ifap);
}

/**
 * discover_register_device:
 * @name: the device name
 * @http_port: the port of the HTTP server
 * @https_port: the port of the HTTPs server
 *
 * This function will send a request to Sparod server to register the device
 * with its name and its serial. The HTTP server ports are also sent.
 * Then, the network interfaces are listed and each of them (except loop back)
 * are notified to the Sparod server.
 *
 * Returns: %true if the device has been registered, %false otherwise.
 */
bool
discover_register_device (
    const char *name, unsigned int http_port, unsigned https_port)
{
  const gchar *host;
  char *url;

  /* No serial found */
  if (!discover_serial) {
    MELO_LOGE ("no serial found");
    return false;
  }

  /* Get host name */
  host = g_get_host_name ();
  if (name != discover_device_name) {
    g_free (discover_device_name);
    discover_device_name = g_strdup (name);
  }
  discover_http_port = http_port;
  discover_https_port = https_port;

  /* Prepare device registration request */
  url = g_strdup_printf (DISCOVER_URL
      "?action=add_device&serial=%s&name=%s&hostname=%s&port=%u&sport=%u",
      discover_serial, name, host, http_port, https_port);

  /* Send request */
  melo_http_client_get (discover_client, url, discover_register_cb, NULL);
  g_free (url);

  return true;
}

/**
 * discover_unregister_device:
 *
 * This function is used to unregister the device from Sparod server. After its
 * call, the device won't be visible from Sparod server.
 *
 * Returns: %true if the device has been unregistered, %false otherwise.
 */
bool
discover_unregister_device (void)
{
  char *url;

  /* No serial found */
  if (!discover_serial)
    return false;

  /* Prepare request for device removal */
  url = g_strconcat (
      DISCOVER_URL, "?action=remove_device&serial=", discover_serial, NULL);

  /* Unregister device from Sparod */
  melo_http_client_get (discover_client, url, NULL, NULL);
  g_free (url);

  /* Device not registered */
  discover_registered = false;

  return true;
}

/**
 * discover_register_service:
 * @name: the device name
 * @http_port: the port of the HTTP server
 * @https_port: the port of the HTTPs server
 *
 * This function will publish a new service for HTTP and HTTPs ports to the mdns
 * client. After the call to this function, the device will be discoverable from
 * local network.
 *
 * Returns: %true if the service has been registered, %false otherwise.
 */
bool
discover_register_service (
    const char *name, unsigned int http_port, unsigned https_port)
{
  /* Register HTTP service */
  if (!discover_http_service) {
    discover_http_service = melo_mdns_add_service (
        discover_mdns, name, "_http._tcp", http_port, NULL);
    if (!discover_http_service)
      MELO_LOGW ("failed to register HTTP service");
  } else if (!melo_mdns_update_service (discover_mdns, discover_http_service,
                 name, "_http._tcp", http_port, false, NULL))
    MELO_LOGW ("failed to update HTTP service");

  /* Register HTTPs service */
  if (discover_https_service) {
    /* Update service */
    if (https_port) {
      if (!melo_mdns_update_service (discover_mdns, discover_https_service,
              name, "_https._tcp", https_port, false, NULL))
        MELO_LOGW ("failed to update HTTPs service");
    } else {
      melo_mdns_remove_service (discover_mdns, discover_https_service);
      discover_https_service = NULL;
    }
  } else if (https_port) {
    discover_https_service = melo_mdns_add_service (
        discover_mdns, name, "_https._tcp", https_port, NULL);
    if (!discover_https_service)
      MELO_LOGW ("failed to register HTTPs service");
  }

  return true;
}

/**
 * discover_unregister_service:
 *
 * This function is used to unregister the service from mdns client. After its
 * call, the service won't be visible from local network.
 *
 * Returns: %true if the service has been unregistered, %false otherwise.
 */
bool
discover_unregister_service (void)
{
  /* Unregister HTTP service */
  if (discover_http_service)
    melo_mdns_remove_service (discover_mdns, discover_http_service);
  discover_http_service = NULL;

  /* Unregister HTTPs service */
  if (discover_https_service)
    melo_mdns_remove_service (discover_mdns, discover_https_service);
  discover_https_service = NULL;

  return true;
}

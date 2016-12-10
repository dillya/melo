/*
 * melo_network.c: A network control based on NetworkManager
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

#include <nm-utils.h>
#include <nm-client.h>
#include <nm-device-ethernet.h>
#include <nm-device-wifi.h>

#include "melo_network.h"

struct _MeloNetworkPrivate {
  NMClient *client;
};

G_DEFINE_TYPE_WITH_PRIVATE (MeloNetwork, melo_network, G_TYPE_OBJECT)

static void
melo_network_finalize (GObject *gobject)
{
  MeloNetwork *net = MELO_NETWORK (gobject);
  MeloNetworkPrivate *priv = melo_network_get_instance_private (net);

  /* Free Network Manager client */
  if (priv->client)
    g_object_unref (priv->client);

  /* Chain up to the parent class */
  G_OBJECT_CLASS (melo_network_parent_class)->finalize (gobject);
}

static void
melo_network_class_init (MeloNetworkClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  /* Add custom finalize() function */
  object_class->finalize = melo_network_finalize;
}

static void
melo_network_init (MeloNetwork *self)
{
  MeloNetworkPrivate *priv = melo_network_get_instance_private (self);
  GError *err = NULL;

  self->priv = priv;

  /* Create a new Network Manager client */
  priv->client = nm_client_new ();
}

MeloNetwork *
melo_network_new ()
{
  return g_object_new (MELO_TYPE_NETWORK, NULL);
}

GList *
melo_network_get_device_list (MeloNetwork *net)
{
  MeloNetworkPrivate *priv = net->priv;
  const GPtrArray *devs;
  GList *list = NULL;
  guint i;

  g_return_val_if_fail (priv->client, NULL);

  /* Get all devices */
  devs = nm_client_get_devices (priv->client);
  if (!devs)
    return NULL;

  /* Generate new list */
  for (i = 0; i < devs->len; i++) {
    NMDevice *dev = g_ptr_array_index(devs, i);
    MeloNetworkDevice *item;

    /* Skip device when not managed */
    if (!nm_device_get_managed (dev))
      continue;

    /* Create new item */
    item = melo_network_device_new (nm_device_get_iface (dev));
    if (!item)
      continue;

    /* Fill item */
    switch (nm_device_get_device_type (dev)) {
      case NM_DEVICE_TYPE_ETHERNET: {
        NMDeviceEthernet *eth = NM_DEVICE_ETHERNET (dev);

        /* Set as ethernet */
        item->type = MELO_NETWORK_DEVICE_TYPE_ETHERNET;

        break;
      }
      case NM_DEVICE_TYPE_WIFI: {
        NMDeviceWifi *wifi = NM_DEVICE_WIFI (dev);

        /* Set as wifi */
        item->type = MELO_NETWORK_DEVICE_TYPE_WIFI;

        break;
      }
      default:
        /* Not supported */
        item->type = MELO_NETWORK_DEVICE_TYPE_UNKNOWN;
    }

    /* Add to list */
    list = g_list_prepend (list, item);
  }

  return list;
}

static MeloNetworkAP *
melo_network_nm_ap_to_ap_item (NMAccessPoint *ap)
{
  NM80211ApSecurityFlags wpa_flags, rsn_flags;
  NM80211ApFlags flags;
  NM80211Mode mode;
  const GByteArray *array;
  MeloNetworkAP *item;

  /* Create a new AP item */
  item = melo_network_ap_new (nm_access_point_get_bssid (ap));
  if (!item)
    return NULL;

  /* Set SSID */
  array = nm_access_point_get_ssid (ap);
  if (array)
    item->ssid = nm_utils_ssid_to_utf8 (array);

  /* Set security */
  flags = nm_access_point_get_flags (ap);
  wpa_flags = nm_access_point_get_wpa_flags (ap);
  rsn_flags = nm_access_point_get_rsn_flags (ap);
  if (rsn_flags != NM_802_11_AP_SEC_NONE) {
    if (rsn_flags & NM_802_11_AP_SEC_KEY_MGMT_802_1X)
      item->security = MELO_NETWORK_AP_SECURITY_WPA2_ENTERPRISE;
    else
      item->security = MELO_NETWORK_AP_SECURITY_WPA2;
  } else if (wpa_flags != NM_802_11_AP_SEC_NONE) {
    if (wpa_flags & NM_802_11_AP_SEC_KEY_MGMT_802_1X)
      item->security = MELO_NETWORK_AP_SECURITY_WPA_ENTERPRISE;
    else
      item->security = MELO_NETWORK_AP_SECURITY_WPA;
  } else if (flags & NM_802_11_AP_FLAGS_PRIVACY)
    item->security = MELO_NETWORK_AP_SECURITY_WEP;
  else
    item->security = MELO_NETWORK_AP_SECURITY_NONE;

  /* Set mode */
  mode = nm_access_point_get_mode (ap);
  switch (mode) {
    case NM_802_11_MODE_ADHOC:
      item->mode = MELO_NETWORK_AP_MODE_ADHOC;
      break;
    case NM_802_11_MODE_INFRA:
      item->mode = MELO_NETWORK_AP_MODE_INFRA;
      break;
    default:
      item->mode = MELO_NETWORK_AP_MODE_UNKNOWN;
  }

  /* Set signal details */
  item->frequency = nm_access_point_get_frequency (ap);
  item->max_bitrate = nm_access_point_get_max_bitrate (ap);
  item->signal_strength = nm_access_point_get_strength (ap);

  return item;
}

GList *
melo_network_wifi_scan (MeloNetwork *net, const gchar *name)
{
  MeloNetworkPrivate *priv = net->priv;
  NMAccessPoint *cur_ap;
  NMDeviceWifi *wifi;
  NMDevice *dev;
  const GPtrArray *aps;
  GList *list = NULL;
  guint i;

  g_return_val_if_fail (priv->client, NULL);

  /* Get device */
  dev = nm_client_get_device_by_iface (priv->client, name);
  if (!dev)
    return NULL;
  wifi = NM_DEVICE_WIFI (dev);

  /* Get active access point */
  cur_ap = nm_device_wifi_get_active_access_point (wifi);

  /* Get Access Points list */
  aps = nm_device_wifi_get_access_points (wifi);
  if (!aps)
    return NULL;

  /* Generate new list */
  for (i = 0; i < aps->len; i++) {
    NMAccessPoint *ap = g_ptr_array_index(aps, i);
    MeloNetworkAP *item;

    /* Create a new AP item */
    item = melo_network_nm_ap_to_ap_item (ap);
    if (!item)
      continue;

    /* Set status */
    if (cur_ap == ap)
      item->status = MELO_NETWORK_AP_STATUS_CONNECTED;

    /* Add to AP list */
    list = g_list_prepend (list, item);
  }

  return list;
}

MeloNetworkDevice *
melo_network_device_new (const gchar *iface)
{
  MeloNetworkDevice *dev;

  /* Allocate new device */
  dev = g_slice_new0 (MeloNetworkDevice);
  if (!dev)
    return NULL;

  /* Set iface */
  dev->iface = g_strdup (iface);

  return dev;
}

void
melo_network_device_free (MeloNetworkDevice *dev)
{
  g_free (dev->iface);
  g_free (dev->name);
  g_slice_free (MeloNetworkDevice, dev);
}

MeloNetworkAP *
melo_network_ap_new (const gchar *bssid)
{
  MeloNetworkAP *ap;

  /* Allocate new device */
  ap = g_slice_new0 (MeloNetworkAP);
  if (!ap)
    return NULL;

  /* Set name */
  ap->bssid = g_strdup (bssid);

  return ap;
}

void
melo_network_ap_free (MeloNetworkAP *ap)
{
  g_free (ap->bssid);
  g_free (ap->ssid);
  g_slice_free (MeloNetworkAP, ap);
}

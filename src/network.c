/*
 * Copyright (C) 2020 Alexandre Dilly <dillya@sparod.com>
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA.
 */

#include <NetworkManager.h>

#define MELO_LOG_TAG "melo_network"
#include <melo/melo_log.h>

#include "proto/network.pb-c.h"

#include "network.h"

static NMClient *network_client;
static GList *network_requests;

typedef struct {
  NMDevice *device;
  MeloAsyncData async;
  GCancellable *cancellable;
} NetworkRequest;

static void
client_new_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GError *error = NULL;

  /* Get client instance */
  network_client = nm_client_new_finish (res, &error);
  if (!network_client) {
    MELO_LOGE ("failed to create nm client: %s", error->message);
    g_error_free (error);
  }
}

/**
 * network_init:
 *
 * Initialize network client to monitor and control settings.
 */
void
network_init (void)
{
  /* Create client */
  nm_client_new_async (NULL, client_new_cb, NULL);
}

/**
 * network_deinit:
 *
 * Clean and release network client and its resources.
 */
void
network_deinit (void)
{
  /* Release client */
  if (network_client)
    g_object_unref (network_client);
}

static bool
network_get_device_list (MeloAsyncCb cb, void *user_data)
{
  Network__Response resp = NETWORK__RESPONSE__INIT;
  Network__Response__DeviceList list = NETWORK__RESPONSE__DEVICE_LIST__INIT;
  Network__Response__DeviceList__Device **devices_ptr;
  Network__Response__DeviceList__Device *devices;
  const GPtrArray *array;
  MeloMessage *msg;
  unsigned int i;

  /* Get device list */
  array = nm_client_get_devices (network_client);
  if (!array) {
    MELO_LOGE ("failed to get device list");
    return false;
  }

  /* Set response */
  resp.resp_case = NETWORK__RESPONSE__RESP_DEVICE_LIST;
  resp.device_list = &list;

  /* Allocate pointers */
  devices_ptr = malloc (sizeof (*devices_ptr) * array->len);
  devices = malloc (sizeof (*devices) * array->len);

  /* Set device list */
  list.n_devices = 0;
  list.devices = devices_ptr;

  /* Fill device list */
  for (i = 0; i < array->len; i++) {
    Network__Response__DeviceList__Device__Type type;
    Network__Response__DeviceList__Device *device;
    NMDevice *dev = g_ptr_array_index (array, i);

    /* List only ethernet and wifi devices */
    switch (nm_device_get_device_type (dev)) {
    case NM_DEVICE_TYPE_ETHERNET:
      type = NETWORK__RESPONSE__DEVICE_LIST__DEVICE__TYPE__ETHERNET;
      break;
    case NM_DEVICE_TYPE_WIFI:
      type = NETWORK__RESPONSE__DEVICE_LIST__DEVICE__TYPE__WIFI;
      break;
    default:
      /* Skip device */
      continue;
    }

    /* Add device */
    device = &devices[list.n_devices];
    devices_ptr[list.n_devices++] = device;

    /* Set device */
    network__response__device_list__device__init (device);
    device->iface = (char *) nm_device_get_iface (dev);
    device->type = type;
  }

  /* Pack message */
  msg = melo_message_new (network__response__get_packed_size (&resp));
  melo_message_set_size (
      msg, network__response__pack (&resp, melo_message_get_data (msg)));

  /* Free response */
  free (devices_ptr);
  free (devices);

  /* Send response */
  if (cb)
    cb (msg, user_data);
  melo_message_unref (msg);

  return true;
}

static NetworkRequest *
network_request_new (NMDevice *device, MeloAsyncCb cb, void *user_data)
{
  NetworkRequest *req;

  /* Allocate new request */
  req = g_slice_new0 (NetworkRequest);
  if (req) {
    /* Set request */
    req->device = device;
    req->async.cb = cb;
    req->async.user_data = user_data;
    req->cancellable = g_cancellable_new ();

    /* Add to request list */
    network_requests = g_list_prepend (network_requests, req);
  }

  return req;
}

static void
network_request_free (NetworkRequest *req)
{
  /* Close request */
  if (req->async.cb)
    req->async.cb (NULL, req->async.user_data);

  /* Remove from request list */
  network_requests = g_list_remove (network_requests, req);

  /* Free request */
  g_object_unref (req->cancellable);
  g_slice_free (NetworkRequest, req);
}

static void
network_fill_ip_settings (
    NMDevice *dev, NMConnection *conn, Network__IPSettings *settings, bool v6)
{
  NMSettingIPConfig *sconfig;
  NMIPConfig *config;

  /* Get setting IP config */
  sconfig = (NMSettingIPConfig *) nm_connection_get_setting (
      conn, v6 ? NM_TYPE_SETTING_IP6_CONFIG : NM_TYPE_SETTING_IP4_CONFIG);
  if (sconfig) {
    const char *method;

    /* Set mode */
    method = nm_setting_ip_config_get_method (sconfig);
    if (method) {
      if (!strcmp (method, NM_SETTING_IP6_CONFIG_METHOD_IGNORE) ||
          !strcmp (method, NM_SETTING_IP4_CONFIG_METHOD_DISABLED))
        settings->mode = NETWORK__IPSETTINGS__MODE__DISABLED;
      else if (!strcmp (method, NM_SETTING_IP4_CONFIG_METHOD_MANUAL))
        settings->mode = NETWORK__IPSETTINGS__MODE__MANUAL;
    }
  }

  /* Get IP config */
  config = v6 ? nm_device_get_ip6_config (dev) : nm_device_get_ip4_config (dev);
  if (config) {
    const char *const *dns;
    GPtrArray *array;

    /* Get first address */
    array = nm_ip_config_get_addresses (config);
    if (array && array->len) {
      NMIPAddress *address = g_ptr_array_index (array, 0);

      /* Set address */
      settings->address = (char *) nm_ip_address_get_address (address);
      settings->prefix = nm_ip_address_get_prefix (address);
    }

    /* Set gateway */
    settings->gateway = (char *) nm_ip_config_get_gateway (config);

    /* Get DNS servers */
    dns = nm_ip_config_get_nameservers (config);
    if (dns)
      settings->dns = (char *) dns[0];
  }
}

static void
ethernet_device_cb (
    GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  NMDevice *device = NM_DEVICE (source_object);
  NetworkRequest *req = user_data;
  GError *error = NULL;
  guint64 version_id;
  NMConnection *conn;

  /* Get active connection */
  conn = nm_device_get_applied_connection_finish (
      device, res, &version_id, &error);
  if (conn) {
    Network__Response resp = NETWORK__RESPONSE__INIT;
    Network__Response__EthernetDevice dev =
        NETWORK__RESPONSE__ETHERNET_DEVICE__INIT;
    Network__IPSettings ipv4 = NETWORK__IPSETTINGS__INIT;
    Network__IPSettings ipv6 = NETWORK__IPSETTINGS__INIT;
    MeloMessage *msg;

    /* Set response */
    resp.resp_case = NETWORK__RESPONSE__RESP_ETHERNET_DEVICE;
    resp.ethernet_device = &dev;

    /* Set state */
    dev.connected = nm_device_get_state (device) == NM_DEVICE_STATE_ACTIVATED;

    /* Set IPv4 / IPv6 settings */
    dev.ipv4 = &ipv4;
    dev.ipv6 = &ipv6;
    network_fill_ip_settings (device, conn, &ipv4, false);
    network_fill_ip_settings (device, conn, &ipv6, true);

    /* Pack message */
    msg = melo_message_new (network__response__get_packed_size (&resp));
    melo_message_set_size (
        msg, network__response__pack (&resp, melo_message_get_data (msg)));

    /* Send response */
    if (req->async.cb)
      req->async.cb (msg, req->async.user_data);
    melo_message_unref (msg);

    /* Free connection */
    g_object_unref (conn);
  } else {
    MELO_LOGE ("failed to get ethernet connection: %s", error->message);
    g_error_free (error);
  }

  /* Free request */
  network_request_free (req);
}

static bool
network_get_ethernet_device (const char *iface, MeloAsyncCb cb, void *user_data)
{
  NetworkRequest *req;
  NMDevice *device;

  /* Get device */
  device = nm_client_get_device_by_iface (network_client, iface);
  if (!device || !NM_IS_DEVICE_ETHERNET (device)) {
    MELO_LOGE ("invalid device name");
    return false;
  }

  /* Create request */
  req = network_request_new (device, cb, user_data);
  if (!req)
    return false;

  /* Get current device connection */
  nm_device_get_applied_connection_async (
      device, 0, req->cancellable, ethernet_device_cb, req);

  return true;
}

static void
wifi_device_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  NMDevice *device = NM_DEVICE (source_object);
  NetworkRequest *req = user_data;
  GError *error = NULL;
  guint64 version_id;
  NMConnection *conn;

  /* Get active connection */
  conn = nm_device_get_applied_connection_finish (
      device, res, &version_id, &error);
  if (conn) {
    Network__Response resp = NETWORK__RESPONSE__INIT;
    Network__Response__WifiDevice dev = NETWORK__RESPONSE__WIFI_DEVICE__INIT;
    Network__IPSettings ipv4 = NETWORK__IPSETTINGS__INIT;
    Network__IPSettings ipv6 = NETWORK__IPSETTINGS__INIT;
    MeloMessage *msg;

    /* Set response */
    resp.resp_case = NETWORK__RESPONSE__RESP_WIFI_DEVICE;
    resp.wifi_device = &dev;

    /* Set state */
    dev.connected = nm_device_get_state (device) == NM_DEVICE_STATE_ACTIVATED;

    /* Set IPv4 / IPv6 settings */
    dev.ipv4 = &ipv4;
    dev.ipv6 = &ipv6;
    network_fill_ip_settings (device, conn, &ipv4, false);
    network_fill_ip_settings (device, conn, &ipv6, true);

    /* Pack message */
    msg = melo_message_new (network__response__get_packed_size (&resp));
    melo_message_set_size (
        msg, network__response__pack (&resp, melo_message_get_data (msg)));

    /* Send response */
    if (req->async.cb)
      req->async.cb (msg, req->async.user_data);
    melo_message_unref (msg);

    /* Free connection */
    g_object_unref (conn);
  } else {
    MELO_LOGE ("failed to get wifi connection: %s", error->message);
    g_error_free (error);
  }

  /* Free request */
  network_request_free (req);
}

static bool
network_get_wifi_device (const char *iface, MeloAsyncCb cb, void *user_data)
{
  NetworkRequest *req;
  NMDevice *device;

  /* Get device */
  device = nm_client_get_device_by_iface (network_client, iface);
  if (!device || !NM_IS_DEVICE_WIFI (device)) {
    MELO_LOGE ("invalid device name");
    return false;
  }

  /* Create request */
  req = network_request_new (device, cb, user_data);
  if (!req)
    return false;

  /* Get current device connection */
  nm_device_get_applied_connection_async (
      device, 0, req->cancellable, wifi_device_cb, req);

  return true;
}

static bool
network_scan_wifi (const char *iface, MeloAsyncCb cb, void *user_data)
{
  NMDevice *device;

  /* Get device */
  device = nm_client_get_device_by_iface (network_client, iface);
  if (!device || !NM_IS_DEVICE_WIFI (device)) {
    MELO_LOGE ("invalid device name");
    return false;
  }

  /* Last scan request is aged of 30s or less */
  if (nm_utils_get_timestamp_msec () -
          nm_device_wifi_get_last_scan (NM_DEVICE_WIFI (device)) <
      30000)
    return true;

  /* Request new scan */
  nm_device_wifi_request_scan_async (NM_DEVICE_WIFI (device), NULL, NULL, NULL);

  return true;
}

static bool
network_get_access_point_list (
    const char *iface, MeloAsyncCb cb, void *user_data)
{
  Network__Response resp = NETWORK__RESPONSE__INIT;
  Network__Response__AccessPointList list =
      NETWORK__RESPONSE__ACCESS_POINT_LIST__INIT;
  Network__Response__AccessPointList__AccessPoint **aps_ptr;
  Network__Response__AccessPointList__AccessPoint *aps;
  const GPtrArray *array;
  NMAccessPoint *ap;
  NMDevice *device;
  MeloMessage *msg;
  unsigned int i;

  /* Get device */
  device = nm_client_get_device_by_iface (network_client, iface);
  if (!device || !NM_IS_DEVICE_WIFI (device)) {
    MELO_LOGE ("invalid device name");
    return false;
  }

  /* Get access point list */
  array = nm_device_wifi_get_access_points (NM_DEVICE_WIFI (device));
  if (!array) {
    MELO_LOGE ("failed to get access point list");
    return false;
  }

  /* Set response */
  resp.resp_case = NETWORK__RESPONSE__RESP_AP_LIST;
  resp.ap_list = &list;

  /* Allocate pointers */
  aps_ptr = malloc (sizeof (*aps_ptr) * array->len);
  aps = malloc (sizeof (*aps) * array->len);

  /* Set device list */
  list.n_access_points = array->len;
  list.access_points = aps_ptr;

  /* Fill device list */
  for (i = 0; i < array->len; i++) {
    NM80211ApSecurityFlags wpa_flags, rsn_flags;
    NM80211ApFlags flags;
    GBytes *ssid;

    /* Get access point */
    ap = g_ptr_array_index (array, i);

    /* Get flags */
    flags = nm_access_point_get_flags (ap);
    wpa_flags = nm_access_point_get_wpa_flags (ap);
    rsn_flags = nm_access_point_get_rsn_flags (ap);

    /* Add access point */
    network__response__access_point_list__access_point__init (&aps[i]);
    aps_ptr[i] = &aps[i];

    /* Set access point */
    ssid = nm_access_point_get_ssid (ap);
    if (ssid)
      aps[i].ssid = nm_utils_ssid_to_utf8 (
          g_bytes_get_data (ssid, NULL), g_bytes_get_size (ssid));
    aps[i].bssid = (char *) nm_access_point_get_bssid (ap);
    aps[i].strength = nm_access_point_get_strength (ap);
    aps[i].private_ = flags & NM_802_11_AP_FLAGS_PRIVACY;

    /* Set mode */
    switch (nm_access_point_get_mode (ap)) {
    case NM_802_11_MODE_ADHOC:
      aps[i].mode = NETWORK__WIFI_MODE__AD_HOC;
      break;
    case NM_802_11_MODE_INFRA:
    default:
      aps[i].mode = NETWORK__WIFI_MODE__INFRASTRUCTURE;
    }

    /* Set security */
    if (flags & NM_802_11_AP_FLAGS_PRIVACY) {
      if ((wpa_flags == NM_802_11_AP_SEC_NONE) &&
          (rsn_flags == NM_802_11_AP_SEC_NONE))
        aps[i].security = NETWORK__WIFI_SECURITY__WEP_PASSPHRASE;
      else if (wpa_flags != NM_802_11_AP_SEC_NONE)
        aps[i].security = NETWORK__WIFI_SECURITY__WPA;
      else if ((rsn_flags & NM_802_11_AP_SEC_KEY_MGMT_PSK) ||
               (rsn_flags & NM_802_11_AP_SEC_KEY_MGMT_802_1X))
        aps[i].security = NETWORK__WIFI_SECURITY__WPA2;
    }
  }

  /* Set active access point */
  ap = nm_device_wifi_get_active_access_point (NM_DEVICE_WIFI (device));
  if (ap)
    list.active_bssid = (char *) nm_access_point_get_bssid (ap);

  /* Pack message */
  msg = melo_message_new (network__response__get_packed_size (&resp));
  melo_message_set_size (
      msg, network__response__pack (&resp, melo_message_get_data (msg)));

  /* Free response */
  for (i = 0; i < array->len; i++)
    if (aps[i].ssid != protobuf_c_empty_string)
      g_free (aps[i].ssid);
  free (aps_ptr);
  free (aps);

  /* Send response */
  if (cb)
    cb (msg, user_data);
  melo_message_unref (msg);

  return true;
}

static void
apply_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  NetworkRequest *req = user_data;
  NMActiveConnection *conn;
  GError *error = NULL;

  /* Get result */
  conn = nm_client_activate_connection_finish (
      NM_CLIENT (source_object), res, &error);
  if (!conn) {
    MELO_LOGE ("failed to activate connection: %s", error->message);
    g_error_free (error);
  } else
    g_object_unref (conn);

  /* Free request */
  network_request_free (req);
}

static void
commit_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  NMRemoteConnection *conn = NM_REMOTE_CONNECTION (source_object);
  NetworkRequest *req = user_data;
  GError *error = NULL;

  /* Get result */
  if (!nm_remote_connection_commit_changes_finish (conn, res, &error)) {
    MELO_LOGE ("failed to update connection: %s", error->message);
    g_error_free (error);
  }

  /* Apply connection */
  nm_client_activate_connection_async (network_client, NM_CONNECTION (conn),
      req->device, NULL, req->cancellable, apply_cb, req);
}

static void
add_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  NetworkRequest *req = user_data;
  NMActiveConnection *conn;
  GError *error = NULL;

  /* Get new connection */
  conn = nm_client_add_and_activate_connection_finish (
      NM_CLIENT (source_object), res, &error);
  if (!conn) {
    MELO_LOGE ("failed to add and activate connection: %s", error->message);
    g_error_free (error);
  } else
    g_object_unref (conn);

  /* Free request */
  network_request_free (req);
}

static bool
network_set_ip_settings (Network__Request__SetIPSettings *req, bool v6,
    MeloAsyncCb cb, void *user_data)
{
  Network__IPSettings *settings = req->settings;
  NMRemoteConnection *remote = NULL;
  NMActiveConnection *active;
  NMConnection *conn = NULL;
  NMDevice *device;
  NMSettingIPConfig *ip;
  NetworkRequest *request;
  const char *method;

  /* No settings */
  if (!settings)
    return false;

  /* Get device */
  device = nm_client_get_device_by_iface (network_client, req->iface);
  if (!device) {
    MELO_LOGE ("invalid device name");
    return false;
  }

  /* Get connection */
  if (NM_IS_DEVICE_WIFI (device)) {
    /* Get active connection */
    active = nm_device_get_active_connection (device);
    if (!active) {
      MELO_LOGE ("no active connection on Wifi device");
      return false;
    }

    /* Get remote connection */
    remote = nm_active_connection_get_connection (active);
    conn = (NMConnection *) remote;
  } else {
    char *id;

    /* Generate connection ID */
    id = g_strconcat ("melo_", req->iface, NULL);

    /* Find connection */
    remote = nm_client_get_connection_by_id (network_client, id);
    if (remote)
      conn = (NMConnection *) remote;

    /* Create a new wired connection */
    if (!conn) {
      NMSettingConnection *s_con;
      NMSettingWired *s_wired;
      char *uuid;

      /* Create simple connection */
      conn = nm_simple_connection_new ();
      if (!conn) {
        MELO_LOGE ("failed to create new connection");
        g_free (id);
        return false;
      }

      /* Generate random UUID */
      uuid = nm_utils_uuid_generate ();

      /* Add connection setting */
      s_con = (NMSettingConnection *) nm_setting_connection_new ();
      g_object_set (s_con, NM_SETTING_CONNECTION_UUID, uuid,
          NM_SETTING_CONNECTION_ID, id, NM_SETTING_CONNECTION_TYPE,
          "802-3-ethernet", NULL);
      nm_connection_add_setting (conn, NM_SETTING (s_con));
      g_free (uuid);

      /* Add wired setting */
      s_wired = (NMSettingWired *) nm_setting_wired_new ();
      nm_connection_add_setting (conn, NM_SETTING (s_wired));

      /* Add IP setting */
      if (v6) {
        NMSettingIP6Config *s_ip6;

        /* Add IPv6 setting */
        s_ip6 = (NMSettingIP6Config *) nm_setting_ip6_config_new ();
        g_object_set (G_OBJECT (s_ip6), NM_SETTING_IP_CONFIG_METHOD,
            NM_SETTING_IP6_CONFIG_METHOD_AUTO, NULL);
        nm_connection_add_setting (conn, NM_SETTING (s_ip6));
      } else {
        NMSettingIP4Config *s_ip4;

        /* Add IPv4 setting */
        s_ip4 = (NMSettingIP4Config *) nm_setting_ip4_config_new ();
        g_object_set (G_OBJECT (s_ip4), NM_SETTING_IP_CONFIG_METHOD,
            NM_SETTING_IP4_CONFIG_METHOD_AUTO, NULL);
        nm_connection_add_setting (conn, NM_SETTING (s_ip4));
      }
    }

    /* Free connection ID */
    g_free (id);
  }

  /* Find method */
  switch (settings->mode) {
  case NETWORK__IPSETTINGS__MODE__MANUAL:
    method = v6 ? NM_SETTING_IP6_CONFIG_METHOD_MANUAL
                : NM_SETTING_IP4_CONFIG_METHOD_MANUAL;
    break;
  case NETWORK__IPSETTINGS__MODE__DISABLED:
    method = v6 ? NM_SETTING_IP6_CONFIG_METHOD_IGNORE
                : NM_SETTING_IP4_CONFIG_METHOD_DISABLED;
    break;
  case NETWORK__IPSETTINGS__MODE__AUTOMATIC:
  default:
    method = v6 ? NM_SETTING_IP6_CONFIG_METHOD_AUTO
                : NM_SETTING_IP4_CONFIG_METHOD_AUTO;
  }

  /* Apply method and reset settings */
  ip = v6 ? nm_connection_get_setting_ip6_config (conn)
          : nm_connection_get_setting_ip4_config (conn);
  g_object_set (ip, NM_SETTING_IP_CONFIG_METHOD, method,
      NM_SETTING_IP_CONFIG_GATEWAY, NULL, NULL);
  nm_setting_ip_config_clear_addresses (ip);
  nm_setting_ip_config_clear_dns (ip);

  /* Set manual settings */
  if (settings->mode == NETWORK__IPSETTINGS__MODE__MANUAL) {
    NMIPAddress *address;

    /* Set IP address */
    address = nm_ip_address_new (
        v6 ? AF_INET6 : AF_INET, settings->address, settings->prefix, NULL);
    if (!nm_setting_ip_config_add_address (ip, address)) {
      MELO_LOGE ("failed to set address");
      goto failed;
    }
    nm_ip_address_unref (address);

    /* Set gateway */
    g_object_set (ip, NM_SETTING_IP_CONFIG_GATEWAY, settings->gateway, NULL);

    /* Set DNS */
    if (!nm_setting_ip_config_add_dns (ip, settings->dns)) {
      MELO_LOGE ("failed to set DNS");
      goto failed;
    }
  }

  /* Create request */
  request = network_request_new (device, cb, user_data);
  if (!request)
    goto failed;

  /* Apply new configuration */
  if (remote) {
    nm_remote_connection_commit_changes_async (
        remote, TRUE, request->cancellable, commit_cb, request);
  } else {
    nm_client_add_and_activate_connection_async (network_client, conn, device,
        NULL, request->cancellable, add_cb, request);
    g_object_unref (conn);
  }

  return true;

failed:
  if (!remote)
    g_object_unref (conn);
  return false;
}

static bool
network_set_wifi_settings (
    Network__Request__SetWifiSettings *req, MeloAsyncCb cb, void *user_data)
{
  Network__WifiSettings *settings = req->settings;
  NMSettingWireless *s_wifi;
  NMSettingWirelessSecurity *s_wifi_sec;
  NMRemoteConnection *remote;
  NMConnection *conn;
  NMDevice *device;
  NetworkRequest *request;
  const char *key_setting = NM_SETTING_WIRELESS_SECURITY_WEP_KEY0;
  const char *mode, *key_mgmt;
  guint key_type = NM_WEP_KEY_TYPE_KEY;
  GBytes *ssid;
  char *id;

  /* No settings */
  if (!settings)
    return false;

  /* Get device */
  device = nm_client_get_device_by_iface (network_client, req->iface);
  if (!device || !NM_IS_DEVICE_WIFI (device)) {
    MELO_LOGE ("invalid device name");
    return false;
  }

  /* Create SSID */
  ssid = g_bytes_new_static (settings->ssid, strlen (settings->ssid));
  if (!ssid) {
    MELO_LOGE ("failed to allocate SSID");
    return false;
  }

  /* Create connection ID */
  id = g_strdup_printf ("%s%s",
      settings->mode == NETWORK__WIFI_MODE__ACCESS_POINT ? "AP_" : "",
      settings->ssid);
  if (!id) {
    MELO_LOGE ("failed to allocate wifi connection ID");
    g_bytes_unref (ssid);
    return false;
  }

  /* Select wifi mode */
  switch (settings->mode) {
  case NETWORK__WIFI_MODE__ACCESS_POINT:
    mode = "ap";
    break;
  case NETWORK__WIFI_MODE__AD_HOC:
    mode = "ad-hoc";
    break;
  case NETWORK__WIFI_MODE__INFRASTRUCTURE:
  default:
    mode = "infrastructure";
  }

  /* Select security */
  switch (settings->security) {
  case NETWORK__WIFI_SECURITY__WPA2:
  case NETWORK__WIFI_SECURITY__WPA:
    key_mgmt = "wpa-psk";
    key_setting = NM_SETTING_WIRELESS_SECURITY_PSK;
    break;
  case NETWORK__WIFI_SECURITY__WEP_PASSPHRASE:
    key_type = NM_WEP_KEY_TYPE_PASSPHRASE;
  case NETWORK__WIFI_SECURITY__WEP_KEY:
    key_mgmt = "none";
    key_setting = NM_SETTING_WIRELESS_SECURITY_WEP_KEY0;
    break;
  case NETWORK__WIFI_SECURITY__NONE:
  default:
    key_mgmt = NULL;
  }

  /* Find connection from SSID */
  remote = nm_client_get_connection_by_id (network_client, id);
  if (remote) {
    /* Get connection */
    conn = (NMConnection *) remote;

    /* Update wifi SSID */
    s_wifi = nm_connection_get_setting_wireless (conn);
    g_object_set (s_wifi, NM_SETTING_WIRELESS_SSID, ssid, NULL);

    /* Update wifi security */
    s_wifi_sec = nm_connection_get_setting_wireless_security (conn);
    g_object_set (s_wifi_sec, NM_SETTING_WIRELESS_SECURITY_KEY_MGMT, key_mgmt,
        key_setting, settings->key, NM_SETTING_WIRELESS_SECURITY_WEP_KEY_TYPE,
        key_type, NULL);
  } else {
    NMSettingConnection *s_con;
    NMSettingIP4Config *s_ip4;
    char *uuid;

    /* Create new connection */
    conn = nm_simple_connection_new ();
    if (!conn) {
      MELO_LOGE ("failed to create Wifi connection");
      g_bytes_unref (ssid);
      g_free (id);
      return false;
    }

    /* Generate random UUID */
    uuid = nm_utils_uuid_generate ();

    /* Add connection setting */
    s_con = (NMSettingConnection *) nm_setting_connection_new ();
    g_object_set (s_con, NM_SETTING_CONNECTION_UUID, uuid,
        NM_SETTING_CONNECTION_ID, id, NM_SETTING_CONNECTION_TYPE,
        "802-11-wireless", NULL);
    g_free (uuid);
    nm_connection_add_setting (conn, NM_SETTING (s_con));

    /* Add wifi setting */
    s_wifi = (NMSettingWireless *) nm_setting_wireless_new ();
    g_object_set (s_wifi, NM_SETTING_WIRELESS_SSID, ssid,
        NM_SETTING_WIRELESS_MODE, mode, NULL);
    nm_connection_add_setting (conn, NM_SETTING (s_wifi));

    /* Add wifi security setting */
    s_wifi_sec =
        (NMSettingWirelessSecurity *) nm_setting_wireless_security_new ();
    g_object_set (s_wifi_sec, NM_SETTING_WIRELESS_SECURITY_KEY_MGMT, key_mgmt,
        key_setting, settings->key, NM_SETTING_WIRELESS_SECURITY_WEP_KEY_TYPE,
        key_type, NULL);
    nm_connection_add_setting (conn, NM_SETTING (s_wifi_sec));

    /* Add IPv4 setting */
    s_ip4 = (NMSettingIP4Config *) nm_setting_ip4_config_new ();
    g_object_set (G_OBJECT (s_ip4), NM_SETTING_IP_CONFIG_METHOD,
        NM_SETTING_IP4_CONFIG_METHOD_AUTO, NULL);
    nm_connection_add_setting (conn, NM_SETTING (s_ip4));
  }

  /* Free SSID */
  g_bytes_unref (ssid);
  g_free (id);

  /* Create request */
  request = network_request_new (device, cb, user_data);
  if (!request) {
    if (!remote)
      g_object_unref (conn);
    return false;
  }

  /* Apply new configuration */
  if (remote)
    nm_remote_connection_commit_changes_async (
        remote, TRUE, request->cancellable, commit_cb, request);
  else {
    nm_client_add_and_activate_connection_async (network_client, conn, device,
        NULL, request->cancellable, add_cb, request);
    g_object_unref (conn);
  }

  return true;
}

/**
 * network_handle_request:
 * @msg: the #MeloMessage to handle
 * @cb: the function to call when a response is sent
 * @user_data: data to pass to @cb
 *
 * This function is called when a new network request is received by the
 * application.
 *
 * If the request is malformed or an internal error occurs, the function will
 * return %false, otherwise %true will be returned.
 *
 * After returning, many asynchronous tasks related to the request can still be
 * pending, so @cb and @user_data should not be destroyed. If the request need
 * to stopped / cancelled, network_cancel_request() is intended for.
 *
 * Returns: %true if the message has been handled, %false otherwise.
 */
bool
network_handle_request (MeloMessage *msg, MeloAsyncCb cb, void *user_data)
{
  Network__Request *request;
  bool ret = false;

  if (!msg)
    return false;

  /* Unpack request */
  request = network__request__unpack (
      NULL, melo_message_get_size (msg), melo_message_get_cdata (msg, NULL));
  if (!request) {
    MELO_LOGE ("failed to unpack request");
    return false;
  }

  /* Handle request */
  switch (request->req_case) {
  case NETWORK__REQUEST__REQ_GET_DEVICE_LIST:
    ret = network_get_device_list (cb, user_data);
    break;
  case NETWORK__REQUEST__REQ_GET_ETHERNET_DEVICE:
    ret = network_get_ethernet_device (
        request->get_ethernet_device, cb, user_data);
    cb = NULL;
    break;
  case NETWORK__REQUEST__REQ_GET_WIFI_DEVICE:
    ret = network_get_wifi_device (request->get_wifi_device, cb, user_data);
    cb = NULL;
    break;
  case NETWORK__REQUEST__REQ_SCAN_WIFI:
    ret = network_scan_wifi (request->scan_wifi, cb, user_data);
    break;
  case NETWORK__REQUEST__REQ_GET_AP_LIST:
    ret = network_get_access_point_list (request->get_ap_list, cb, user_data);
    break;
  case NETWORK__REQUEST__REQ_SET_IPV4_SETTINGS:
    ret = network_set_ip_settings (
        request->set_ipv4_settings, false, cb, user_data);
    cb = NULL;
    break;
  case NETWORK__REQUEST__REQ_SET_IPV6_SETTINGS:
    ret = network_set_ip_settings (
        request->set_ipv6_settings, false, cb, user_data);
    cb = NULL;
    break;
  case NETWORK__REQUEST__REQ_SET_WIFI_SETTINGS:
    ret = network_set_wifi_settings (request->set_wifi_settings, cb, user_data);
    break;
  default:
    MELO_LOGE ("request %u not supported", request->req_case);
  }

  /* Free request */
  network__request__free_unpacked (request, NULL);

  /* End of request */
  if (ret && cb)
    cb (NULL, user_data);

  return ret;
}

/**
 * network_cancel_request:
 * @cb: the function used during call to network_handle_request()
 * @user_data: data passed with @cb
 *
 * This function can be called to cancel a running or a pending request. If the
 * request exists, the asynchronous tasks will be cancelled and @cb will be
 * called with a NULL-message to signal end of request. If the request is
 * already finished or a cancellation is already pending, this function will do
 * nothing.
 */
void
network_cancel_request (MeloAsyncCb cb, void *user_data)
{
  GList *l;

  /* Find request */
  for (l = network_requests; l != NULL; l = l->next) {
    NetworkRequest *req = l->data;

    /* Cancel request */
    if (req->async.user_data == user_data) {
      if (!g_cancellable_is_cancelled (req->cancellable))
        g_cancellable_cancel (req->cancellable);
      break;
    }
  }
}

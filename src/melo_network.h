/*
 * melo_network.h: A network control based on NetworkManager
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

#ifndef __MELO_NETWORK_H__
#define __MELO_NETWORK_H__

G_BEGIN_DECLS

#define MELO_TYPE_NETWORK \
    (melo_network_get_type ())
#define MELO_NETWORK(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), MELO_TYPE_NETWORK, MeloNetwork))
#define MELO_IS_NETWORK(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MELO_TYPE_NETWORK))
#define MELO_NETWORK_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), MELO_TYPE_NETWORK, MeloNetworkClass))
#define MELO_IS_NETWORK_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), MELO_TYPE_NETWORK))
#define MELO_NETWORK_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), MELO_TYPE_NETWORK, MeloNetworkClass))

typedef struct _MeloNetwork MeloNetwork;
typedef struct _MeloNetworkClass MeloNetworkClass;
typedef struct _MeloNetworkPrivate MeloNetworkPrivate;

typedef enum _MeloNetworkDeviceType MeloNetworkDeviceType;
typedef struct _MeloNetworkDevice MeloNetworkDevice;

typedef enum _MeloNetworkAPMode MeloNetworkAPMode;
typedef enum _MeloNetworkAPStatus MeloNetworkAPStatus;
typedef enum _MeloNetworkAPSecurity MeloNetworkAPSecurity;
typedef struct _MeloNetworkAP MeloNetworkAP;

struct _MeloNetwork {
  GObject parent_instance;

  /*< private >*/
  MeloNetworkPrivate *priv;
};

struct _MeloNetworkClass {
  GObjectClass parent_class;
};

enum _MeloNetworkDeviceType {
  MELO_NETWORK_DEVICE_TYPE_UNKNOWN = 0,
  MELO_NETWORK_DEVICE_TYPE_ETHERNET,
  MELO_NETWORK_DEVICE_TYPE_WIFI,
};

struct _MeloNetworkDevice {
  gchar *iface;
  gchar *name;
  MeloNetworkDeviceType type;
};

enum _MeloNetworkAPMode {
 MELO_NETWORK_AP_MODE_UNKNOWN = 0,
 MELO_NETWORK_AP_MODE_ADHOC,
 MELO_NETWORK_AP_MODE_INFRA,
};

enum _MeloNetworkAPSecurity {
  MELO_NETWORK_AP_SECURITY_NONE = 0,
  MELO_NETWORK_AP_SECURITY_WEP,
  MELO_NETWORK_AP_SECURITY_WPA,
  MELO_NETWORK_AP_SECURITY_WPA2,
  MELO_NETWORK_AP_SECURITY_WPA_ENTERPRISE,
  MELO_NETWORK_AP_SECURITY_WPA2_ENTERPRISE,
};

enum _MeloNetworkAPStatus {
  MELO_NETWORK_AP_STATUS_DISCONNECTED = 0,
  MELO_NETWORK_AP_STATUS_CONNECTED,
};

struct _MeloNetworkAP {
  gchar *bssid;
  gchar *ssid;
  MeloNetworkAPMode mode;
  MeloNetworkAPSecurity security;
  guint32 frequency;
  guint32 max_bitrate;
  guint8 signal_strength;
  MeloNetworkAPStatus status;
};

MeloNetwork *melo_network_new (void);

GList *melo_network_get_device_list (MeloNetwork *net);
GList *melo_network_wifi_scan (MeloNetwork *net, const gchar *name);

MeloNetworkDevice *melo_network_device_new (const gchar *iface);
void melo_network_device_free (MeloNetworkDevice *dev);

MeloNetworkAP *melo_network_ap_new (const gchar *bssid);
void melo_network_ap_free (MeloNetworkAP *ap);

G_END_DECLS

#endif /* __MELO_NETWORK_H__ */

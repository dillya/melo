/*
 * melo_network_jsonrpc.c: Network controler JSON-RPC interface
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

#include "melo_jsonrpc.h"

#include "melo_network_jsonrpc.h"

typedef enum {
  MELO_NETWORK_JSONRPC_DEVICE_LIST_FIELDS_NONE = 0,
  MELO_NETWORK_JSONRPC_DEVICE_LIST_FIELDS_IFACE = 1,
  MELO_NETWORK_JSONRPC_DEVICE_LIST_FIELDS_NAME = 2,
  MELO_NETWORK_JSONRPC_DEVICE_LIST_FIELDS_TYPE = 4,

  MELO_NETWORK_JSONRPC_DEVICE_LIST_FIELDS_FULL = ~0,
} MeloNetworkJSONRPCDeviceListFields;

typedef enum {
  MELO_NETWORK_JSONRPC_AP_LIST_FIELDS_NONE = 0,
  MELO_NETWORK_JSONRPC_AP_LIST_FIELDS_BSSID = 1,
  MELO_NETWORK_JSONRPC_AP_LIST_FIELDS_SSID = 2,
  MELO_NETWORK_JSONRPC_AP_LIST_FIELDS_MODE = 4,
  MELO_NETWORK_JSONRPC_AP_LIST_FIELDS_SECURITY = 8,
  MELO_NETWORK_JSONRPC_AP_LIST_FIELDS_FREQUENCY = 16,
  MELO_NETWORK_JSONRPC_AP_LIST_FIELDS_BITRATE = 32,
  MELO_NETWORK_JSONRPC_AP_LIST_FIELDS_STRENGTH = 64,
  MELO_NETWORK_JSONRPC_AP_LIST_FIELDS_STATUS = 128,

  MELO_NETWORK_JSONRPC_AP_LIST_FIELDS_FULL = ~0,
} MeloNetworkJSONRPCAPListFields;

static MeloNetworkJSONRPCDeviceListFields
melo_network_jsonrpc_get_device_list_fields (JsonObject *obj)
{
  MeloNetworkJSONRPCDeviceListFields fields =
                                   MELO_NETWORK_JSONRPC_DEVICE_LIST_FIELDS_NONE;
  const gchar *field;
  JsonArray *array;
  guint count, i;

  /* Check if fields is available */
  if (!json_object_has_member (obj, "fields"))
    return MELO_NETWORK_JSONRPC_DEVICE_LIST_FIELDS_FULL;

  /* Get fields array */
  array = json_object_get_array_member (obj, "fields");
  if (!array)
    return fields;

  /* Parse array */
  count = json_array_get_length (array);
  for (i = 0; i < count; i++) {
    field = json_array_get_string_element (array, i);
    if (!field)
      break;
    if (!g_strcmp0 (field, "none")) {
      fields = MELO_NETWORK_JSONRPC_DEVICE_LIST_FIELDS_NONE;
      break;
    } else if (!g_strcmp0 (field, "full")) {
      fields = MELO_NETWORK_JSONRPC_DEVICE_LIST_FIELDS_FULL;
      break;
    } else if (!g_strcmp0 (field, "iface"))
      fields |= MELO_NETWORK_JSONRPC_DEVICE_LIST_FIELDS_IFACE;
    else if (!g_strcmp0 (field, "name"))
      fields |= MELO_NETWORK_JSONRPC_DEVICE_LIST_FIELDS_NAME;
    else if (!g_strcmp0 (field, "type"))
      fields |= MELO_NETWORK_JSONRPC_DEVICE_LIST_FIELDS_TYPE;
  }

  return fields;
}

static MeloNetworkJSONRPCAPListFields
melo_network_jsonrpc_get_ap_list_fields (JsonObject *obj)
{
  MeloNetworkJSONRPCAPListFields fields =
                                       MELO_NETWORK_JSONRPC_AP_LIST_FIELDS_NONE;
  const gchar *field;
  JsonArray *array;
  guint count, i;

  /* Check if fields is available */
  if (!json_object_has_member (obj, "fields"))
    return MELO_NETWORK_JSONRPC_AP_LIST_FIELDS_FULL;

  /* Get fields array */
  array = json_object_get_array_member (obj, "fields");
  if (!array)
    return fields;

  /* Parse array */
  count = json_array_get_length (array);
  for (i = 0; i < count; i++) {
    field = json_array_get_string_element (array, i);
    if (!field)
      break;
    if (!g_strcmp0 (field, "none")) {
      fields = MELO_NETWORK_JSONRPC_AP_LIST_FIELDS_NONE;
      break;
    } else if (!g_strcmp0 (field, "full")) {
      fields = MELO_NETWORK_JSONRPC_AP_LIST_FIELDS_FULL;
      break;
    } else if (!g_strcmp0 (field, "bssid"))
      fields |= MELO_NETWORK_JSONRPC_AP_LIST_FIELDS_BSSID;
    else if (!g_strcmp0 (field, "ssid"))
      fields |= MELO_NETWORK_JSONRPC_AP_LIST_FIELDS_SSID;
    else if (!g_strcmp0 (field, "mode"))
      fields |= MELO_NETWORK_JSONRPC_AP_LIST_FIELDS_MODE;
    else if (!g_strcmp0 (field, "security"))
      fields |= MELO_NETWORK_JSONRPC_AP_LIST_FIELDS_SECURITY;
    else if (!g_strcmp0 (field, "frequency"))
      fields |= MELO_NETWORK_JSONRPC_AP_LIST_FIELDS_FREQUENCY;
    else if (!g_strcmp0 (field, "bitrate"))
      fields |= MELO_NETWORK_JSONRPC_AP_LIST_FIELDS_BITRATE;
    else if (!g_strcmp0 (field, "strength"))
      fields |= MELO_NETWORK_JSONRPC_AP_LIST_FIELDS_STRENGTH;
    else if (!g_strcmp0 (field, "status"))
      fields |= MELO_NETWORK_JSONRPC_AP_LIST_FIELDS_STATUS;
  }

  return fields;
}

static JsonArray *
melo_network_jsonrpc_device_list_to_array (GList *list,
                                      MeloNetworkJSONRPCDeviceListFields fields)
{
  JsonArray *array;
  const GList *l;

  /* Parse list and create array */
  array = json_array_new ();
  for (l = list; l != NULL; l = l->next) {
    MeloNetworkDevice *dev = (MeloNetworkDevice *) l->data;
    JsonObject *o = json_object_new ();

    /* Fill object */
    if (fields & MELO_NETWORK_JSONRPC_DEVICE_LIST_FIELDS_IFACE)
      json_object_set_string_member (o, "iface", dev->iface);
    if (fields & MELO_NETWORK_JSONRPC_DEVICE_LIST_FIELDS_NAME)
      json_object_set_string_member (o, "name", dev->name);
    if (fields & MELO_NETWORK_JSONRPC_DEVICE_LIST_FIELDS_TYPE) {
      const gchar *type;
      switch (dev->type) {
        case MELO_NETWORK_DEVICE_TYPE_ETHERNET:
          type = "ethernet";
          break;
        case MELO_NETWORK_DEVICE_TYPE_WIFI:
          type = "wifi";
          break;
        case MELO_NETWORK_DEVICE_TYPE_UNKNOWN:
        default:
          type = "unknown";
      }
      json_object_set_string_member (o, "type", type);
    }

    /* Add object to array */
    json_array_add_object_element (array, o);
  }

  return array;
}

static JsonArray *
melo_network_jsonrpc_ap_list_to_array (GList *list,
                                       MeloNetworkJSONRPCAPListFields fields)
{
  JsonArray *array;
  const GList *l;
  const gchar *str;

  /* Parse list and create array */
  array = json_array_new ();
  for (l = list; l != NULL; l = l->next) {
    MeloNetworkAP *ap = (MeloNetworkAP *) l->data;
    JsonObject *o = json_object_new ();

    /* Fill object */
    if (fields & MELO_NETWORK_JSONRPC_AP_LIST_FIELDS_BSSID)
      json_object_set_string_member (o, "bssid", ap->bssid);
    if (fields & MELO_NETWORK_JSONRPC_AP_LIST_FIELDS_SSID)
      json_object_set_string_member (o, "ssid", ap->ssid);
    if (fields & MELO_NETWORK_JSONRPC_AP_LIST_FIELDS_MODE) {
      switch (ap->mode) {
        case MELO_NETWORK_AP_MODE_ADHOC:
          str = "ad-hoc";
          break;
        case MELO_NETWORK_AP_MODE_INFRA:
          str = "infrastructure";
          break;
        case MELO_NETWORK_AP_MODE_UNKNOWN:
        default:
          str = "unknown";
      }
      json_object_set_string_member (o, "mode", str);
    }
    if (fields & MELO_NETWORK_JSONRPC_AP_LIST_FIELDS_SECURITY) {
      switch (ap->security) {
        case MELO_NETWORK_AP_SECURITY_WEP:
          str = "WEP";
          break;
        case MELO_NETWORK_AP_SECURITY_WPA:
          str = "WPA";
          break;
        case MELO_NETWORK_AP_SECURITY_WPA2:
          str = "WPA2";
          break;
        case MELO_NETWORK_AP_SECURITY_WPA_ENTERPRISE:
          str = "WPA Enterprise";
          break;
        case MELO_NETWORK_AP_SECURITY_WPA2_ENTERPRISE:
          str = "WPA2 Enterprise";
          break;
        case MELO_NETWORK_AP_SECURITY_NONE:
        default:
          str = "none";
      }
      json_object_set_string_member (o, "security", str);
    }
    if (fields & MELO_NETWORK_JSONRPC_AP_LIST_FIELDS_FREQUENCY)
      json_object_set_int_member (o, "frequency", ap->frequency);
    if (fields & MELO_NETWORK_JSONRPC_AP_LIST_FIELDS_BITRATE)
      json_object_set_int_member (o, "bitrate", ap->max_bitrate);
    if (fields & MELO_NETWORK_JSONRPC_AP_LIST_FIELDS_STRENGTH)
      json_object_set_int_member (o, "strength", ap->signal_strength);
    if (fields & MELO_NETWORK_JSONRPC_AP_LIST_FIELDS_STATUS) {
      switch (ap->status) {
        case MELO_NETWORK_AP_STATUS_CONNECTED:
          str = "connected";
          break;
        case MELO_NETWORK_AP_STATUS_DISCONNECTED:
        default:
          str = "disconnected";
      }
      json_object_set_string_member (o, "status", str);
    }

    /* Add object to array */
    json_array_add_object_element (array, o);
  }

  return array;
}

/* Method callbacks */
static void
melo_network_jsonrpc_get_device_list (const gchar *method,
                                     JsonArray *s_params, JsonNode *params,
                                     JsonNode **result, JsonNode **error,
                                     gpointer user_data)
{
  MeloNetwork *net = MELO_NETWORK (user_data);
  MeloNetworkJSONRPCDeviceListFields fields;
  JsonArray *array;
  JsonObject *obj;
  GList *list;

  /* Get parameters */
  obj = melo_jsonrpc_get_object (s_params, params, error);
  if (!obj)
    return;

  /* Get fields */
  fields = melo_network_jsonrpc_get_device_list_fields (obj);
  json_object_unref (obj);

  /* Get device list */
  list = melo_network_get_device_list (net);

  /* Create response with device list */
  array = melo_network_jsonrpc_device_list_to_array (list, fields);

  /* Free device list */
  g_list_free_full (list, (GDestroyNotify) melo_network_device_free);

  /* Return object */
  *result = json_node_new (JSON_NODE_ARRAY);
  json_node_take_array (*result, array);
}

static void
melo_network_jsonrpc_scan_wifi (const gchar *method,
                                JsonArray *s_params, JsonNode *params,
                                JsonNode **result, JsonNode **error,
                                gpointer user_data)
{
  MeloNetwork *net = MELO_NETWORK (user_data);
  MeloNetworkJSONRPCAPListFields fields;
  JsonArray *array;
  JsonObject *obj;
  const gchar *iface;
  GList *list;

  /* Get parameters */
  obj = melo_jsonrpc_get_object (s_params, params, error);
  if (!obj)
    return;

  /* Get network interface */
  iface = json_object_get_string_member (obj, "iface");

  /* Get fields */
  fields = melo_network_jsonrpc_get_ap_list_fields (obj);

  /* Get Wifi AP list */
  list = melo_network_wifi_scan (net, iface);
  json_object_unref (obj);

  /* Create response with Wifi AP list */
  array = melo_network_jsonrpc_ap_list_to_array (list, fields);

  /* Free device list */
  g_list_free_full (list, (GDestroyNotify) melo_network_ap_free);

  /* Return object */
  *result = json_node_new (JSON_NODE_ARRAY);
  json_node_take_array (*result, array);
}

/* List of methods */
static MeloJSONRPCMethod melo_network_jsonrpc_methods[] = {
  {
    .method = "get_device_list",
    .params = "["
              "  {"
              "    \"name\": \"fields\", \"type\": \"array\","
              "    \"required\": false"
              "  }"
              "]",
    .result = "{\"type\":\"array\"}",
    .callback = melo_network_jsonrpc_get_device_list,
    .user_data = NULL,
  },
  {
    .method = "scan_wifi",
    .params = "["
              "  {\"name\": \"iface\", \"type\": \"string\"},"
              "  {"
              "    \"name\": \"fields\", \"type\": \"array\","
              "    \"required\": false"
              "  }"
              "]",
    .result = "{\"type\":\"array\"}",
    .callback = melo_network_jsonrpc_scan_wifi,
    .user_data = NULL,
  },
};

/* Register / Unregister methods */
void
melo_network_jsonrpc_register_methods (MeloNetwork *net)
{
  guint i;

  /* Add network to methods array */
  g_object_ref (net);
  for (i = 0; i < G_N_ELEMENTS (melo_network_jsonrpc_methods); i++) {
    melo_network_jsonrpc_methods[i].user_data = net;
  }

  /* Register new methods */
  melo_jsonrpc_register_methods ("network", melo_network_jsonrpc_methods,
                                 G_N_ELEMENTS (melo_network_jsonrpc_methods));
}

void
melo_network_jsonrpc_unregister_methods (void)
{
  /* Unregister new methods */
  melo_jsonrpc_unregister_methods ("network", melo_network_jsonrpc_methods,
                                   G_N_ELEMENTS (melo_network_jsonrpc_methods));

  /* Unref network object */
  g_object_unref (MELO_NETWORK (melo_network_jsonrpc_methods[0].user_data));
}

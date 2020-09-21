/*
 * Copyright (C) 2016-2020 Alexandre Dilly <dillya@sparod.com>
 *
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

#include <glib.h>
#include <string.h>

#include <avahi-client/lookup.h>
#include <avahi-client/publish.h>
#include <avahi-gobject/ga-client.h>

#define MELO_LOG_TAG "mdns"
#include "melo/melo_log.h"

#include "melo/melo_mdns.h"

/**
 * SECTION:melo_mdns
 * @title: MeloMdns
 * @short_description: An mDNS client to manage Zeroconf services
 *
 * #MeloMdns is intended to help Zeroconf / mDNS service registration for
 * any sub-module of Melo. It also do discovering in order to list a specific
 * service type on the network.
 */

/* Common mdns client */
G_LOCK_DEFINE_STATIC (melo_mdns_mutex);
GaClient *melo_mdns_client;

struct _MeloMdns {
  /* Parent instance */
  GObject parent_instance;

  GMutex mutex;

  /* Service publisher */
  AvahiEntryGroup *group;
  GList *pservices;

  /* Service browser */
  GHashTable *browsers;
  GList *bservices;
};

G_DEFINE_TYPE (MeloMdns, melo_mdns, G_TYPE_OBJECT)

static void
melo_mdns_finalize (GObject *gobject)
{
  MeloMdns *mdns = MELO_MDNS (gobject);

  /* Free group */
  if (mdns->group)
    avahi_entry_group_free (mdns->group);

  /* Remove and free all browsers */
  if (mdns->browsers)
    g_hash_table_remove_all (mdns->browsers);

  /* Free service list */
  g_list_free_full (mdns->pservices, (GDestroyNotify) melo_mdns_service_free);
  g_list_free_full (mdns->bservices, (GDestroyNotify) melo_mdns_service_free);

  /* Lock mdns client */
  G_LOCK (melo_mdns_mutex);

  /* Free mdns client */
  if (melo_mdns_client)
    g_object_unref (melo_mdns_client);

  /* Unlock mdns client */
  G_UNLOCK (melo_mdns_mutex);

  /* Clear mutex */
  g_mutex_clear (&mdns->mutex);

  /* Chain up to the parent class */
  G_OBJECT_CLASS (melo_mdns_parent_class)->finalize (gobject);
}

static void
melo_mdns_class_init (MeloMdnsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  /* Add custom finalize() function */
  object_class->finalize = melo_mdns_finalize;
}

static void
melo_mdns_init (MeloMdns *self)
{
  /* Init mutex */
  g_mutex_init (&self->mutex);

  /* Lock mdns client */
  G_LOCK (melo_mdns_mutex);

  /* Create new mdns client */
  if (!melo_mdns_client) {
    melo_mdns_client = ga_client_new (GA_CLIENT_FLAG_NO_FLAGS);
    ga_client_start (melo_mdns_client, NULL);
  } else
    g_object_ref (melo_mdns_client);

  /* Unlock mdns client */
  G_UNLOCK (melo_mdns_mutex);
}

/**
 * melo_mdns_new:
 *
 * Instantiate a new #MeloMdns.
 *
 * Returns: (transfer full): the new #MeloMdns instance or %NULL if failed.
 *     After use, call g_object_unref().
 */
MeloMdns *
melo_mdns_new (void)
{
  return g_object_new (MELO_TYPE_MDNS, NULL);
}

static void
entry_group_cb (
    AvahiEntryGroup *group, AvahiEntryGroupState state, void *userdata)
{
  switch (state) {
  case AVAHI_ENTRY_GROUP_ESTABLISHED:
  case AVAHI_ENTRY_GROUP_COLLISION:
  case AVAHI_ENTRY_GROUP_FAILURE:
  case AVAHI_ENTRY_GROUP_UNCOMMITED:
  case AVAHI_ENTRY_GROUP_REGISTERING:
    break;
  }
}

static bool
melo_mdns_service_update_group (AvahiClient *client, MeloMdns *mdns)
{
  MeloMdnsService *s;
  GList *l;
  int ret;

  /* Create group if doesn't exist */
  if (!mdns->group) {
    mdns->group = avahi_entry_group_new (client, entry_group_cb, NULL);
    if (!mdns->group) {
      MELO_LOGE ("failed to create group");
      return false;
    }
  }

  /* Reset group */
  avahi_entry_group_reset (mdns->group);

  /* Parse services list */
  for (l = mdns->pservices; l != NULL; l = l->next) {
    s = (MeloMdnsService *) l->data;

    /* Add service to group */
    ret = avahi_entry_group_add_service_strlst (mdns->group, AVAHI_IF_UNSPEC,
        AVAHI_PROTO_UNSPEC, 0, s->name, s->type, NULL, NULL, s->port, s->txt);

    /* Failed to add service */
    if (ret) {
      MELO_LOGE ("failed to add service %s %s", s->name, s->type);
      return false;
    }
  }

  /* Commit new group */
  if (avahi_entry_group_commit (mdns->group) < 0) {
    MELO_LOGE ("failed to commit service");
    return false;
  }

  return true;
}

static int
melo_mdns_service_cmp (const MeloMdnsService *a, const MeloMdnsService *b)
{
  return g_strcmp0 (a->name, b->name) || g_strcmp0 (a->type, b->type) ||
         a->iface != b->iface;
}

/**
 * melo_mdns_service_get_txt:
 * @s: an mdns service
 * @key: the key to find in the txt record of the service
 *
 * Extract the value associated to @key in the TXT record of the service @s.
 *
 * Returns: (transfer full): a string containing the value associated to @key or
 *     %NULL if @key has not been found in TXT record. After use, call g_free().
 */
char *
melo_mdns_service_get_txt (const MeloMdnsService *s, const char *key)
{
  AvahiStringList *l;
  gsize len;

  /* Find DNS-SD TXT record with its key */
  l = avahi_string_list_find ((AvahiStringList *) s->txt, key);
  if (!l || !l->size)
    return NULL;

  /* Copy string */
  len = strlen (key) + 1;
  return g_strndup ((const char *) l->text + len, l->size - len);
}

/**
 * melo_mdns_service_copy:
 * @s: the mdns service
 *
 * Provide a deep copy of a #MeloMdnsService.
 *
 * Returns: (transfer full): a copy of the #MeloMdnsService in @s. Must be freed
 *     after usage with melo_mdns_service_free().
 */
MeloMdnsService *
melo_mdns_service_copy (const MeloMdnsService *s)
{
  MeloMdnsService *service;

  /* Create new service */
  service = g_slice_new0 (MeloMdnsService);
  if (!service)
    return NULL;

  /* Copy all values */
  service->name = g_strdup (s->name);
  service->type = g_strdup (s->type);
  service->port = s->port;
  service->txt = avahi_string_list_copy (s->txt);
  service->iface = s->iface;
  if (s->is_ipv6)
    memcpy (service->ipv6, s->ipv6, 16);
  else
    memcpy (service->ipv4, s->ipv4, 4);

  return service;
}

/**
 * melo_mdns_service_free:
 * @s: the mdns service
 *
 * Free a #MeloMdnsService instance.
 */
void
melo_mdns_service_free (MeloMdnsService *s)
{
  g_free (s->name);
  g_free (s->type);
  avahi_string_list_free (s->txt);
  g_slice_free (MeloMdnsService, s);
}

/**
 * melo_mdns_add_service:
 * @mdns: the mdns object
 * @name: the service name
 * @type: the type of service (protocol like "_http._tcp")
 * @port: the service port number
 * @...: the string list to add to TXT record as "key=value". Must be terminated
 *     with a %NULL pointer
 *
 * Add a new service to mDNS in order to broadcast the service through
 * Zeroconf / mDNS.
 * The service is handled by the mDNS object and is removed automatically when
 * last reference of the #MeloMdns is unreferenced with g_object_unref().
 * The returned #MeloMdnsService is used to update / to remove the service with
 * respectively melo_mdns_update_service() and melo_mdns_remove_service().
 *
 * Returns: (transfer none): a pointer to the #MeloMdnsService created to
 *     handle the new service.
 */
const MeloMdnsService *
melo_mdns_add_service (
    MeloMdns *mdns, const char *name, const char *type, int port, ...)
{
  MeloMdnsService service = {
      .name = (char *) name,
      .type = (char *) type,
      .iface = 0,
  };
  MeloMdnsService *s;
  va_list va;

  /* Check if service already exists */
  if (g_list_find_custom (
          mdns->pservices, &service, (GCompareFunc) melo_mdns_service_cmp))
    return NULL;

  /* Create new service */
  s = g_slice_new (MeloMdnsService);
  if (!s)
    return NULL;
  s->name = g_strdup (name);
  s->type = g_strdup (type);
  s->port = port;
  s->iface = 0;

  /* Create string list */
  va_start (va, port);
  s->txt = avahi_string_list_new_va (va);
  va_end (va);

  /* Add new service */
  mdns->pservices = g_list_prepend (mdns->pservices, s);

  /* Update group */
  melo_mdns_service_update_group (melo_mdns_client->avahi_client, mdns);

  return s;
}

/**
 * melo_mdns_update_service:
 * @mdns: the mdns object
 * @service: the mdns service
 * @name: the new service name or %NULL
 * @type: the new type of service or %NULL
 * @port: the new service port number or 0
 * @update_txt: set to %true is the TXT record must be updated
 * @...: the new string list to add to TXT record as "key=value". Must be
 *    terminated with a %NULL pointer. The previous TXT record is freed
 *
 * Update a registered service with some new values such as the name, the type,
 * the port or the TXT record.
 *
 * Returns: %true if the service has been updated successfully, %false
 *     otherwise.
 */
bool
melo_mdns_update_service (MeloMdns *mdns, const MeloMdnsService *service,
    const char *name, const char *type, int port, bool update_txt, ...)
{
  MeloMdnsService *s = (MeloMdnsService *) service;
  va_list va;

  /* No service */
  if (!s)
    return false;

  /* Update name */
  if (name && g_strcmp0 (name, s->name)) {
    g_free (s->name);
    s->name = g_strdup (name);
  }

  /* Update type */
  if (type && g_strcmp0 (type, s->type)) {
    g_free (s->type);
    s->type = g_strdup (type);
  }

  /* Update port */
  if (port && port != s->port)
    s->port = port;

  /* Update string list */
  if (update_txt) {
    avahi_string_list_free (s->txt);
    va_start (va, update_txt);
    s->txt = avahi_string_list_new_va (va);
    va_end (va);
  }

  /* Update group */
  return melo_mdns_service_update_group (melo_mdns_client->avahi_client, mdns);
}

/**
 * melo_mdns_remove_service:
 * @mdns: the mdns object
 * @service: the mdns service
 *
 * Unregister and remove a service from Zeroconf / mDNS.
 */
void
melo_mdns_remove_service (MeloMdns *mdns, const MeloMdnsService *service)
{
  if (!service)
    return;

  /* Remove service */
  mdns->pservices = g_list_remove (mdns->pservices, service);
  melo_mdns_service_free ((MeloMdnsService *) service);

  /* Update group */
  melo_mdns_service_update_group (melo_mdns_client->avahi_client, mdns);
}

static void
service_resolver_cb (AvahiServiceResolver *ar, AvahiIfIndex interface,
    AvahiProtocol protocol, AvahiResolverEvent event, const char *name,
    const char *type, const char *domain, const char *host_name,
    const AvahiAddress *address, uint16_t port, AvahiStringList *txt,
    AvahiLookupResultFlags flags, void *userdata)
{
  MeloMdnsService service = {
      .name = (char *) name,
      .type = (char *) type,
      .iface = interface,
  };
  MeloMdns *mdns = (MeloMdns *) userdata;
  MeloMdnsService *s;
  GList *l;

  switch (event) {
  case AVAHI_RESOLVER_FOUND:
    /* Lock services list */
    g_mutex_lock (&mdns->mutex);

    /* Find service record */
    l = g_list_find_custom (
        mdns->bservices, &service, (GCompareFunc) melo_mdns_service_cmp);
    if (!l) {
      /* Create new service */
      s = g_slice_new0 (MeloMdnsService);
      if (!s) {
        g_mutex_unlock (&mdns->mutex);
        goto end;
      }
      /* Fill with name / type */
      s->name = g_strdup (name);
      s->type = g_strdup (type);
      s->iface = interface;
      /* Add new service */
      mdns->bservices = g_list_prepend (mdns->bservices, s);
    } else {
      s = (MeloMdnsService *) l->data;
    }

    /* Update service */
    avahi_string_list_free (s->txt);
    s->txt = txt ? avahi_string_list_copy (txt) : NULL;
    s->port = port;
    if (address->proto == AVAHI_PROTO_INET) {
      s->ipv4[0] = address->data.ipv4.address;
      s->ipv4[1] = address->data.ipv4.address >> 8;
      s->ipv4[2] = address->data.ipv4.address >> 16;
      s->ipv4[3] = address->data.ipv4.address >> 24;
      s->is_ipv6 = false;
    } else if (address->proto == AVAHI_PROTO_INET6) {
      memcpy (s->ipv6, address->data.ipv6.address, 16);
      s->is_ipv6 = true;
    }

    /* Unlock services list */
    g_mutex_unlock (&mdns->mutex);
    break;
  case AVAHI_RESOLVER_FAILURE:
    break;
  }

end:
  /* Free resolver */
  avahi_service_resolver_free (ar);
}

static void
service_browser_cb (AvahiServiceBrowser *ab, AvahiIfIndex interface,
    AvahiProtocol protocol, AvahiBrowserEvent event, const char *name,
    const char *type, const char *domain, AvahiLookupResultFlags flags,
    void *userdata)
{
  MeloMdnsService service = {
      .name = (char *) name,
      .type = (char *) type,
      .iface = interface,
  };
  MeloMdns *mdns = (MeloMdns *) userdata;
  GList *l;

  switch (event) {
  case AVAHI_BROWSER_NEW:
    /* Start resolver which add service */
    avahi_service_resolver_new (melo_mdns_client->avahi_client, interface,
        protocol, name, type, domain, AVAHI_PROTO_UNSPEC, 0,
        service_resolver_cb, userdata);
    break;
  case AVAHI_BROWSER_REMOVE:
    /* Lock services list */
    g_mutex_lock (&mdns->mutex);

    /* Remove service from list */
    l = mdns->bservices;
    while (l != NULL) {
      GList *next = l->next;
      if (!melo_mdns_service_cmp (&service, (MeloMdnsService *) l->data)) {
        melo_mdns_service_free ((MeloMdnsService *) l->data);
        mdns->bservices = g_list_delete_link (mdns->bservices, l);
      }
      l = next;
    }

    /* Unlock services list */
    g_mutex_unlock (&mdns->mutex);

    break;
  case AVAHI_BROWSER_ALL_FOR_NOW:
  case AVAHI_BROWSER_CACHE_EXHAUSTED:
    break;
  case AVAHI_BROWSER_FAILURE:
    break;
  }
}

/**
 * melo_mdns_add_browser:
 * @mdns: the mdns object
 * @type: the service type to monitor
 *
 * Add a new service browser to monitor all services of type @type on the
 * network.
 * After browser has been created, the melo_mdns_list_services() will provide
 * a list with the available services with @type, in addition to other services
 * already monitored by the #MeloMdns object.
 * To remove a browser, call melo_mdns_remove_browser().
 *
 * Returns: %true if the new browser has been created, %false otherwise.
 */
bool
melo_mdns_add_browser (MeloMdns *mdns, const char *type)
{
  AvahiServiceBrowser *ab;

  /* Lock browsers access */
  g_mutex_lock (&mdns->mutex);

  /* Allocate an hash table for all browsers */
  if (!mdns->browsers)
    mdns->browsers = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
        (GDestroyNotify) avahi_service_browser_free);

  /* Check if type is already registered */
  if (g_hash_table_lookup (mdns->browsers, type))
    goto unlock;

  /* Unlock browsers access */
  g_mutex_unlock (&mdns->mutex);

  /* Create new Avahi browser */
  ab = avahi_service_browser_new (melo_mdns_client->avahi_client,
      AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, type, NULL, 0, service_browser_cb,
      mdns);
  if (!ab) {
    MELO_LOGE ("failed to create new service browser");
    return false;
  }

  /* Lock browsers access */
  g_mutex_lock (&mdns->mutex);

  /* Add browser to list */
  g_hash_table_insert (mdns->browsers, g_strdup (type), ab);

unlock:
  /* Unlock browsers access */
  g_mutex_unlock (&mdns->mutex);

  return true;
}

static gpointer
melo_mdns_service_list_copy (gconstpointer src, gpointer data)
{
  return melo_mdns_service_copy ((MeloMdnsService *) src);
}

/**
 * melo_mdns_list_services:
 * @mdns: the mdns object
 *
 * Provide a #GList of #MeloMdnsService corresponding to all services
 * available on the network with a service type for which a browser has been
 * added with melo_mdns_add_browser().
 *
 * Returns: (transfer full): a #GList of #MeloMdnsService available. You must
 *     free list and its data when you are done with it. You can use
 *     g_list_free_full() with melo_mdns_service_free() to do this.
 */
GList *
melo_mdns_list_services (MeloMdns *mdns)
{

  return g_list_copy_deep (mdns->bservices, melo_mdns_service_list_copy, NULL);
}

/**
 * melo_mdns_remove_browser:
 * @mdns: the mdns object
 * @type: the service type to monitor
 *
 * Remove the browser which monitors all services of type @type.
 */
void
melo_mdns_remove_browser (MeloMdns *mdns, const char *type)
{

  /* Lock browsers access */
  g_mutex_lock (&mdns->mutex);

  /* Remove browser from list */
  g_hash_table_remove (mdns->browsers, type);

  /* Unlock browsers access */
  g_mutex_unlock (&mdns->mutex);
}

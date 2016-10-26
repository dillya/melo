/*
 * melo_avahi.c: Avahi client to register services
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
#include <glib.h>

#include <avahi-gobject/ga-client.h>
#include <avahi-client/publish.h>
#include <avahi-client/lookup.h>

#include "melo_avahi.h"

/* Common avahi client */
G_LOCK_DEFINE_STATIC (melo_avahi_mutex);
GaClient *melo_avahi_client;

struct _MeloAvahiPrivate {
  GMutex mutex;
  /* Service publisher */
  AvahiEntryGroup *group;
  GList *pservices;
  /* Service browser */
  GHashTable *browsers;
  GList *bservices;
};

G_DEFINE_TYPE_WITH_PRIVATE (MeloAvahi, melo_avahi, G_TYPE_OBJECT)

static void
melo_avahi_finalize (GObject *gobject)
{
  MeloAvahi *avahi = MELO_AVAHI (gobject);
  MeloAvahiPrivate *priv = melo_avahi_get_instance_private (avahi);

  /* Free group */
  if (priv->group)
    avahi_entry_group_free (priv->group);

  /* Remove and free all browsers */
  if (priv->browsers)
    g_hash_table_remove_all (priv->browsers);

  /* Free service list */
  g_list_free_full (priv->pservices, (GDestroyNotify) melo_avahi_service_free);
  g_list_free_full (priv->bservices, (GDestroyNotify) melo_avahi_service_free);

  /* Lock avahi client */
  G_LOCK (melo_avahi_mutex);

  /* Free avahi client */
  if (melo_avahi_client)
    g_object_unref (melo_avahi_client);

  /* Unlock avahi client */
  G_UNLOCK (melo_avahi_mutex);

  /* Clear mutex */
  g_mutex_clear (&priv->mutex);

  /* Chain up to the parent class */
  G_OBJECT_CLASS (melo_avahi_parent_class)->finalize (gobject);
}

static void
melo_avahi_class_init (MeloAvahiClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  /* Add custom finalize() function */
  object_class->finalize = melo_avahi_finalize;
}

static void
melo_avahi_init (MeloAvahi *self)
{
  MeloAvahiPrivate *priv = melo_avahi_get_instance_private (self);

  self->priv = priv;

  /* Init mutex */
  g_mutex_init (&priv->mutex);

  /* Lock avahi client */
  G_LOCK (melo_avahi_mutex);

  /* Create new avahi client */
  if (!melo_avahi_client) {
    melo_avahi_client = ga_client_new (GA_CLIENT_FLAG_NO_FLAGS);
    ga_client_start (melo_avahi_client, NULL);
  } else
    g_object_ref (melo_avahi_client);

  /* Unlock avahi client */
  G_UNLOCK (melo_avahi_mutex);
}

MeloAvahi *
melo_avahi_new (void)
{
  return g_object_new (MELO_TYPE_AVAHI, NULL);
}

static void
melo_avahi_entry_group_callback (AvahiEntryGroup *group,
                                 AvahiEntryGroupState state, void *userdata)
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

static gboolean
melo_avahi_update_group (AvahiClient *client, MeloAvahiPrivate *priv)
{
  MeloAvahiService *s;
  GList *l;
  int ret;

  /* Create group if doesn't exist */
  if (!priv->group) {
    priv->group = avahi_entry_group_new (client,
                                         melo_avahi_entry_group_callback, NULL);
    if (!priv->group)
      return FALSE;
  }

  /* Reset group */
  avahi_entry_group_reset (priv->group);

  /* Parse services list */
  for (l = priv->pservices; l != NULL; l = l->next) {
    s = (MeloAvahiService *) l->data;

    /* Add service to group */
    ret = avahi_entry_group_add_service_strlst (priv->group,
                                                AVAHI_IF_UNSPEC,
                                                AVAHI_PROTO_UNSPEC,
                                                0,
                                                s->name,
                                                s->type,
                                                NULL,
                                                NULL,
                                                s->port,
                                                s->txt);

    /* Failed to add service */
    if (ret)
      return FALSE;
  }

  /* Commit new group */
  if (avahi_entry_group_commit (priv->group) < 0)
    return FALSE;

  return TRUE;
}

static int
melo_avahi_service_cmp (const MeloAvahiService *a, const MeloAvahiService *b)
{
  return g_strcmp0 (a->name, b->name) || g_strcmp0 (a->type, b->type) ||
         a->iface != b->iface;
}

gchar *
melo_avahi_service_get_txt (const MeloAvahiService *s, const gchar *key)
{
  AvahiStringList *l;
  gsize len;

  /* Find DNS-SD TXT record with its key */
  l = avahi_string_list_find (s->txt, key);
  if (!l || !l->size)
    return NULL;

  /* Copy string */
  len = strlen (key) + 1;
  return g_strndup (l->text + len, l->size - len);
}

MeloAvahiService *
melo_avahi_service_copy (const MeloAvahiService *s)
{
  MeloAvahiService *service;

  /* Create new service */
  service = g_slice_new0 (MeloAvahiService);
  if (!service)
    return NULL;

  /* Copy all values */
  service->name = g_strdup (s->name);
  service->type = g_strdup (s->type);
  service->port = s->port;
  service->txt = avahi_string_list_copy (s->txt);
  service->iface = s->iface;
  memcpy (service->ip, s->ip, 4);

  return service;
}

void
melo_avahi_service_free (MeloAvahiService *s)
{
  g_free (s->name);
  g_free (s->type);
  avahi_string_list_free (s->txt);
  g_slice_free (MeloAvahiService, s);
}

const MeloAvahiService *
melo_avahi_add_service (MeloAvahi *avahi, const gchar *name, const gchar *type,
                        gint port, ...)
{
  MeloAvahiService service = {
    .name = (gchar *) name,
    .type = (gchar *) type,
    .iface = 0,
  };
  MeloAvahiPrivate *priv = avahi->priv;
  MeloAvahiService *s;
  va_list va;

  /* Check if service already exists */
  if (g_list_find_custom (priv->pservices, &service,
                          (GCompareFunc) melo_avahi_service_cmp))
    return NULL;

  /* Create new service */
  s = g_slice_new (MeloAvahiService);
  if (!s)
    return NULL;
  s->name = g_strdup (name);
  s->type = g_strdup (type);
  s->port = port;
  s->iface = 0;

  /* Create string list */
  va_start(va, port);
  s->txt = avahi_string_list_new_va (va);
  va_end(va);

  /* Add new service */
  priv->pservices = g_list_prepend (priv->pservices, s);

  /* Update group */
  melo_avahi_update_group (melo_avahi_client->avahi_client, priv);

  return s;
}

gboolean
melo_avahi_update_service (MeloAvahi *avahi, const MeloAvahiService *service,
                           const gchar *name, const gchar *type, gint port,
                           gboolean update_txt, ...)
{
  MeloAvahiService *s = (MeloAvahiService *) service;
  MeloAvahiPrivate *priv = avahi->priv;
  va_list va;

  /* No service */
  if (!s)
    return FALSE;

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
    va_start(va, update_txt);
    s->txt = avahi_string_list_new_va (va);
    va_end(va);
  }

  /* Update group */
  return melo_avahi_update_group (melo_avahi_client->avahi_client, priv);
}

void
melo_avahi_remove_service (MeloAvahi *avahi, const MeloAvahiService *service)
{
  MeloAvahiPrivate *priv = avahi->priv;

  if (!service)
    return;

  /* Remove service */
  priv->pservices = g_list_remove (priv->pservices, service);
  melo_avahi_service_free ((MeloAvahiService*) service);

  /* Update group */
  melo_avahi_update_group (melo_avahi_client->avahi_client, priv);
}

static void
melo_avahi_resolve_callback (AvahiServiceResolver *ar, AvahiIfIndex interface,
                             AvahiProtocol protocol, AvahiResolverEvent event,
                             const char *name, const char *type,
                             const char *domain, const char *host_name,
                             const AvahiAddress *address, uint16_t port,
                             AvahiStringList *txt, AvahiLookupResultFlags flags,
                             void *userdata)
{
  MeloAvahiService service = {
    .name = (gchar *) name,
    .type = (gchar *) type,
    .iface = interface,
  };
  MeloAvahiPrivate *priv = (MeloAvahiPrivate *) userdata;
  MeloAvahiService *s;
  GList *l;

  switch (event) {
    case AVAHI_RESOLVER_FOUND:
      /* Lock services list */
      g_mutex_lock (&priv->mutex);

      /* Find service record */
      l = g_list_find_custom (priv->bservices, &service,
                              (GCompareFunc) melo_avahi_service_cmp);
      if (!l) {
        /* Create new service */
        s = g_slice_new0 (MeloAvahiService);
        if (!s) {
          g_mutex_unlock (&priv->mutex);
          goto end;
        }
        /* Fill with name / type */
        s->name = g_strdup (name);
        s->type = g_strdup (type);
        s->iface = interface;
        /* Add new service */
        priv->bservices = g_list_prepend (priv->bservices, s);
      } else {
        s = (MeloAvahiService *) l->data;
      }

      /* Update service */
      avahi_string_list_free (s->txt);
      s->txt = txt ? avahi_string_list_copy (txt) : NULL;
      s->port = port;
      s->ip[0] = address->data.ipv4.address;
      s->ip[1] = address->data.ipv4.address >> 8;
      s->ip[2] = address->data.ipv4.address >> 16;
      s->ip[3] = address->data.ipv4.address >> 24;

      /* Unlock services list */
      g_mutex_unlock (&priv->mutex);
      break;
    case AVAHI_RESOLVER_FAILURE:
      break;
  }

end:
  /* Free resolver */
  avahi_service_resolver_free (ar);
}

static void
melo_avahi_browser_callback (AvahiServiceBrowser *ab, AvahiIfIndex interface,
                             AvahiProtocol protocol, AvahiBrowserEvent event,
                             const char *name, const char *type,
                             const char *domain, AvahiLookupResultFlags flags,
                             void *userdata)
{
  MeloAvahiService service = {
    .name = (gchar *) name,
    .type = (gchar *) type,
    .iface = interface,
  };
  MeloAvahiPrivate *priv = (MeloAvahiPrivate *) userdata;
  GList *l;

  switch (event) {
    case AVAHI_BROWSER_NEW:
      /* Start resolver which add service */
      avahi_service_resolver_new (melo_avahi_client->avahi_client, interface,
                                  protocol, name, type, domain,
                                  AVAHI_PROTO_UNSPEC, 0,
                                  melo_avahi_resolve_callback, userdata);
      break;
    case AVAHI_BROWSER_REMOVE:
      /* Lock services list */
      g_mutex_lock (&priv->mutex);

      /* Remove service from list */
      l = priv->bservices;
      while  (l != NULL) {
        GList *next = l->next;
        if (!melo_avahi_service_cmp (&service, (MeloAvahiService *) l->data)) {
          melo_avahi_service_free ((MeloAvahiService *) l->data);
          priv->bservices = g_list_delete_link (priv->bservices, l);
        }
        l = next;
      }

      /* Unlock services list */
      g_mutex_unlock (&priv->mutex);

      break;
    case AVAHI_BROWSER_ALL_FOR_NOW:
    case AVAHI_BROWSER_CACHE_EXHAUSTED:
      break;
    case AVAHI_BROWSER_FAILURE:
      break;
  }
}

gboolean
melo_avahi_add_browser (MeloAvahi *avahi, const gchar *type)
{
  MeloAvahiPrivate *priv = avahi->priv;
  AvahiServiceBrowser *ab;

  /* Lock browsers access */
  g_mutex_lock (&priv->mutex);

  /* Allocate an hash table for all browsers */
  if (!priv->browsers)
    priv->browsers = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                   (GDestroyNotify) avahi_service_browser_free);

  /* Check if type is already registered */
  if (g_hash_table_lookup (priv->browsers, type))
    goto unlock;

  /* Unlock browsers access */
  g_mutex_unlock (&priv->mutex);

  /* Create new Avahi browser */
  ab = avahi_service_browser_new (melo_avahi_client->avahi_client,
                                  AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, type,
                                  NULL, 0, melo_avahi_browser_callback,
                                  priv);
  if (!ab)
    return FALSE;

  /* Lock browsers access */
  g_mutex_lock (&priv->mutex);

  /* Add browser to list */
  g_hash_table_insert (priv->browsers, g_strdup (type), ab);

unlock:
  /* Unlock browsers access */
  g_mutex_unlock (&priv->mutex);

  return TRUE;
}

static gpointer
melo_avahi_service_list_copy (gconstpointer src, gpointer data)
{
  return melo_avahi_service_copy ((MeloAvahiService *) src);
}

GList *
melo_avahi_list_services (MeloAvahi *avahi)
{
  MeloAvahiPrivate *priv = avahi->priv;

  return g_list_copy_deep (priv->bservices, melo_avahi_service_list_copy, NULL);
}

void
melo_avahi_remove_browser (MeloAvahi *avahi, const gchar *type)
{
  MeloAvahiPrivate *priv = avahi->priv;

  /* Lock browsers access */
  g_mutex_lock (&priv->mutex);

  /* Remove browser from list */
  g_hash_table_remove (priv->browsers, type);

  /* Unlock browsers access */
  g_mutex_unlock (&priv->mutex);
}

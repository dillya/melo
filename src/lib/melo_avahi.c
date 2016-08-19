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

#include <glib.h>

#include <avahi-gobject/ga-client.h>
#include <avahi-client/publish.h>

#include "melo_avahi.h"

/* Common avahi client */
G_LOCK_DEFINE_STATIC (melo_avahi_mutex);
GaClient *melo_avahi_client;

struct _MeloAvahiPrivate {
  AvahiEntryGroup *group;
  GList *services;
};

G_DEFINE_TYPE_WITH_PRIVATE (MeloAvahi, melo_avahi, G_TYPE_OBJECT)

static void melo_avahi_service_free (MeloAvahiService *s);

static void
melo_avahi_finalize (GObject *gobject)
{
  MeloAvahi *avahi = MELO_AVAHI (gobject);
  MeloAvahiPrivate *priv = melo_avahi_get_instance_private (avahi);

  /* Free group */
  if (priv->group)
    avahi_entry_group_free (priv->group);

  /* Free service list */
  g_list_free_full (priv->services, (GDestroyNotify) melo_avahi_service_free);

  /* Lock avahi client */
  G_LOCK (melo_avahi_mutex);

  /* Free avahi client */
  if (melo_avahi_client)
    g_object_unref (melo_avahi_client);

  /* Unlock avahi client */
  G_UNLOCK (melo_avahi_mutex);

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
  for (l = priv->services; l != NULL; l = l->next) {
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
  if (avahi_entry_group_commit(priv->group) < 0)
    return FALSE;

  return TRUE;
}

static int
melo_avahi_service_cmp (MeloAvahiService *a, MeloAvahiService *b)
{
  return g_strcmp0 (a->name, b->name) || g_strcmp0 (a->type, b->type);
}

static void
melo_avahi_service_free (MeloAvahiService *s)
{
  g_free (s->name);
  g_free (s->type);
  avahi_string_list_free (s->txt);
  g_slice_free (MeloAvahiService, s);
}

const MeloAvahiService *
melo_avahi_add (MeloAvahi *avahi, const gchar *name, const gchar *type,
                gint port, ...)
{
  MeloAvahiService service = { .name = (gchar *) name, .type = (gchar *) type };
  MeloAvahiPrivate *priv = avahi->priv;
  MeloAvahiService *s;
  va_list va;

  /* Check if service already exists */
  if (g_list_find_custom (priv->services, &service,
                          (GCompareFunc) melo_avahi_service_cmp))
    return NULL;

  /* Create new service */
  s = g_slice_new (MeloAvahiService);
  if (!s)
    return NULL;
  s->name = g_strdup (name);
  s->type = g_strdup (type);
  s->port = port;

  /* Create string list */
  va_start(va, port);
  s->txt = avahi_string_list_new_va (va);
  va_end(va);

  /* Add new service */
  priv->services = g_list_prepend (priv->services, s);

  /* Update group */
  melo_avahi_update_group (melo_avahi_client->avahi_client, priv);

  return s;
}

gboolean
melo_avahi_update (MeloAvahi *avahi, const MeloAvahiService *service,
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
melo_avahi_remove (MeloAvahi *avahi, const MeloAvahiService *service)
{
  MeloAvahiPrivate *priv = avahi->priv;

  if (!service)
    return;

  /* Remove service */
  priv->services = g_list_remove (priv->services, service);
  melo_avahi_service_free ((MeloAvahiService*) service);

  /* Update group */
  melo_avahi_update_group (melo_avahi_client->avahi_client, priv);
}

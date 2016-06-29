/*
 * melo_browser.c: Browser base class
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

#include "melo_browser.h"

/* Internal browser list */
G_LOCK_DEFINE_STATIC (melo_browser_mutex);
static GHashTable *melo_browser_hash = NULL;
static GList *melo_browser_list = NULL;

struct _MeloBrowserPrivate {
  gchar *id;
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (MeloBrowser, melo_browser, G_TYPE_OBJECT)

static void
melo_browser_finalize (GObject *gobject)
{
  MeloBrowser *browser = MELO_BROWSER (gobject);
  MeloBrowserPrivate *priv = melo_browser_get_instance_private (browser);

  /* Lock browser list */
  G_LOCK (melo_browser_mutex);

  /* Remove object from browser list */
  melo_browser_list = g_list_remove (melo_browser_list, browser);
  g_hash_table_remove (melo_browser_hash, priv->id);

  /* Unlock browser list */
  G_UNLOCK (melo_browser_mutex);

  if (priv->id)
    g_free (priv->id);

  /* Chain up to the parent class */
  G_OBJECT_CLASS (melo_browser_parent_class)->finalize (gobject);
}

static void
melo_browser_class_init (MeloBrowserClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  /* Add custom finalize() function */
  object_class->finalize = melo_browser_finalize;
}

static void
melo_browser_init (MeloBrowser *self)
{
  MeloBrowserPrivate *priv = melo_browser_get_instance_private (self);

  self->priv = priv;
  priv->id = NULL;
}

static void
melo_browser_set_id (MeloBrowser *browser, const gchar *id)
{
  MeloBrowserPrivate *priv = browser->priv;

  if (priv->id)
    g_free (priv->id);
  priv->id = g_strdup (id);
}

const gchar *
melo_browser_get_id (MeloBrowser *browser)
{
  return browser->priv->id;
}

const MeloBrowserInfo *
melo_browser_get_info (MeloBrowser *browser)
{
  MeloBrowserClass *bclass = MELO_BROWSER_GET_CLASS (browser);

  if (!bclass->get_info)
    return NULL;

  return bclass->get_info (browser);
}

MeloBrowser *
melo_browser_get_browser_by_id (const gchar *id)
{
  MeloBrowser *bro;

  /* Lock browser list */
  G_LOCK (melo_browser_mutex);

  /* Find browser by id */
  bro = g_hash_table_lookup (melo_browser_hash, id);

  /* Increment reference count */
  if (bro)
    g_object_ref (bro);

  /* Unlock browser list */
  G_UNLOCK (melo_browser_mutex);

  return bro;
}

MeloBrowser *
melo_browser_new (GType type, const gchar *id)
{
  MeloBrowser *bro;

  g_return_val_if_fail (id, NULL);
  g_return_val_if_fail (g_type_is_a (type, MELO_TYPE_BROWSER), NULL);

  /* Lock browser list */
  G_LOCK (melo_browser_mutex);

  /* Create browser list */
  if (!melo_browser_hash)
    melo_browser_hash = g_hash_table_new_full (g_str_hash, g_str_equal,
                                               g_free, NULL);

  /* Check if ID is already used */
  if (g_hash_table_lookup (melo_browser_hash, id))
    goto failed;

  /* Create a new instance of browser */
  bro = g_object_new (type, NULL);
  if (!bro)
    goto failed;

  /* Set ID */
  melo_browser_set_id (bro, id);

  /* Add new browser instance to browser list */
  g_hash_table_insert (melo_browser_hash, g_strdup (id), bro);
  melo_browser_list = g_list_append (melo_browser_list, bro);

  /* Unlock browser list */
  G_UNLOCK (melo_browser_mutex);

  return bro;

failed:
  G_UNLOCK (melo_browser_mutex);
  return NULL;
}

GList *
melo_browser_get_list (MeloBrowser *browser, const gchar *path)
{
  MeloBrowserClass *bclass = MELO_BROWSER_GET_CLASS (browser);

  g_return_val_if_fail (bclass->get_list, NULL);

  return bclass->get_list (browser, path);
}

MeloBrowserItem *
melo_browser_item_new (const gchar *name, const gchar *type)
{
  MeloBrowserItem *item;

  /* Allocate new item */
  item = g_slice_new0 (MeloBrowserItem);
  if (!item)
    return NULL;

  /* Set name and type */
  item->name = g_strdup (name);
  item->type = g_strdup (type);

  return item;
}

gint
melo_browser_item_cmp (const MeloBrowserItem *a, const MeloBrowserItem *b)
{
  return g_strcmp0 (a->name, b->name);
}

void
melo_browser_item_free (MeloBrowserItem *item)
{
  if (item->name)
    g_free (item->name);
  if (item->full_name)
    g_free (item->full_name);
  if (item->type)
    g_free (item->type);
  if (item->remove)
    g_free (item->remove);
  g_slice_free (MeloBrowserItem, item);
}

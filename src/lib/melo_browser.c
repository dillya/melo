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

enum {
  PROP_0,
  PROP_ID,
  PROP_LAST
};

static void melo_browser_set_property (GObject *object, guint property_id,
                                       const GValue *value, GParamSpec *pspec);
static void melo_browser_get_property (GObject *object, guint property_id,
                                       GValue *value, GParamSpec *pspec);

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

  /* Unref attached player */
  if (browser->player)
    g_object_unref (browser->player);

  /* Chain up to the parent class */
  G_OBJECT_CLASS (melo_browser_parent_class)->finalize (gobject);
}

static void
melo_browser_class_init (MeloBrowserClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  /* Add custom finalize() function */
  object_class->finalize = melo_browser_finalize;
  object_class->set_property = melo_browser_set_property;
  object_class->get_property = melo_browser_get_property;

  /* Install ID property */
  g_object_class_install_property (object_class, PROP_ID,
      g_param_spec_string ("id", "ID", "Browser ID", NULL,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                           G_PARAM_STATIC_STRINGS));
}

static void
melo_browser_init (MeloBrowser *self)
{
  MeloBrowserPrivate *priv = melo_browser_get_instance_private (self);

  self->priv = priv;
  priv->id = NULL;
}

const gchar *
melo_browser_get_id (MeloBrowser *browser)
{
  return browser->priv->id;
}

static void
melo_browser_set_property (GObject *object, guint property_id,
                           const GValue *value, GParamSpec *pspec)
{
  MeloBrowser *browser = MELO_BROWSER (object);

  switch (property_id)
    {
    case PROP_ID:
      g_free (browser->priv->id);
      browser->priv->id = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
melo_browser_get_property (GObject *object, guint property_id, GValue *value,
                           GParamSpec *pspec)
{
  MeloBrowser *browser = MELO_BROWSER (object);

  switch (property_id)
    {
    case PROP_ID:
      g_value_set_string (value, melo_browser_get_id (browser));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
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
  bro = g_object_new (type, "id", id, NULL);
  if (!bro)
    goto failed;

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

void
melo_browser_set_player (MeloBrowser *browser, MeloPlayer *player)
{
  if (browser->player)
    g_object_unref (browser->player);

  browser->player = g_object_ref (player);
}

MeloPlayer *
melo_browser_get_player (MeloBrowser *browser)
{
  g_return_val_if_fail (browser->player, NULL);

  return g_object_ref (browser->player);
}

MeloBrowserList *
melo_browser_get_list (MeloBrowser *browser, const gchar *path, gint offset,
                       gint count, const gchar *token,
                       MeloBrowserTagsMode tags_mode,
                       MeloTagsFields tags_fields)
{
  MeloBrowserClass *bclass = MELO_BROWSER_GET_CLASS (browser);

  g_return_val_if_fail (bclass->get_list, NULL);

  return bclass->get_list (browser, path, offset, count, token, tags_mode,
                           tags_fields);
}

MeloBrowserList *
melo_browser_search (MeloBrowser *browser, const gchar *input, gint offset,
                     gint count, const gchar *token,
                     MeloBrowserTagsMode tags_mode, MeloTagsFields tags_fields)
{
  MeloBrowserClass *bclass = MELO_BROWSER_GET_CLASS (browser);

  g_return_val_if_fail (bclass->search, NULL);

  return bclass->search (browser, input, offset, count, token, tags_mode,
                         tags_fields);
}

gchar *
melo_browser_search_hint (MeloBrowser *browser, const gchar *input)
{
  MeloBrowserClass *bclass = MELO_BROWSER_GET_CLASS (browser);

  g_return_val_if_fail (bclass->search_hint, NULL);

  return bclass->search_hint (browser, input);
}

MeloTags *
melo_browser_get_tags (MeloBrowser *browser, const gchar *path,
                       MeloTagsFields fields)
{
  MeloBrowserClass *bclass = MELO_BROWSER_GET_CLASS (browser);

  g_return_val_if_fail (bclass->get_tags, NULL);

  return bclass->get_tags (browser, path, fields);
}

gboolean
melo_browser_add (MeloBrowser *browser, const gchar *path)
{
  MeloBrowserClass *bclass = MELO_BROWSER_GET_CLASS (browser);

  g_return_val_if_fail (bclass->add, FALSE);

  return bclass->add (browser, path);
}

gboolean
melo_browser_play (MeloBrowser *browser, const gchar *path)
{
  MeloBrowserClass *bclass = MELO_BROWSER_GET_CLASS (browser);

  g_return_val_if_fail (bclass->play, FALSE);

  return bclass->play (browser, path);
}

gboolean
melo_browser_remove (MeloBrowser *browser, const gchar *path)
{
  MeloBrowserClass *bclass = MELO_BROWSER_GET_CLASS (browser);

  g_return_val_if_fail (bclass->remove, FALSE);

  return bclass->remove (browser, path);
}

gboolean
melo_browser_get_cover (MeloBrowser *browser, const gchar *path, GBytes **cover,
                        gchar **type)
{
  MeloBrowserClass *bclass = MELO_BROWSER_GET_CLASS (browser);

  g_return_val_if_fail (bclass->get_cover, FALSE);

  return bclass->get_cover (browser, path, cover, type);
}

MeloBrowserList *
melo_browser_list_new (const gchar *path)
{
  MeloBrowserList *list;

  /* Create browser list */
  list = g_slice_new0 (MeloBrowserList);
  if (!list)
    return NULL;

  /* Set path */
  list->path = g_strdup (path);

  return list;
}

void
melo_browser_list_free (MeloBrowserList *list)
{
  g_free (list->path);
  g_free (list->prev_token);
  g_free (list->next_token);
  g_list_free_full (list->items, (GDestroyNotify) melo_browser_item_free);
  g_slice_free (MeloBrowserList, list);
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
  if (item->add)
    g_free (item->add);
  if (item->remove)
    g_free (item->remove);
  if (item->tags)
    melo_tags_unref (item->tags);
  g_slice_free (MeloBrowserItem, item);
}

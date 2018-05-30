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

/**
 * SECTION:melo_browser
 * @title: MeloBrowser
 * @short_description: Base class for Melo Browser
 *
 * #MeloBrowser is the main class to handle media / library / file system
 * browsing interface to find which media to listen with Melo. It provides two
 * ways to get list of medias: melo_browser_get_list() to get a list from a
 * specific path and melo_browser_search() to get a list from a keywords input.
 * Each function uses the same #MeloBrowserList structire to provide the media
 * list to explore.
 *
 * For each media in a list, handled by #MeloBrowserItem, many details are
 * provided as its display name, type, available actions, tags, ...
 * The type and actions are customizable if the standard types and actions
 * offered by Melo are not enough.
 * Moreover, for media tags handled by #MeloTags in every #MeloBrowserItem, some
 * caching methods are possible to increase the speed of the list generation.
 * For more details, please see #MeloBrowserTagsMode.
 *
 * In case of path navigation, the path can follow any logic but it is
 * recommanded to follow the URI schema in order to simplify developer and user
 * life.
 * The path is also used to specify on which item we want to do an action with
 * melo_browser_action().
 *
 * Every instance of #MeloBrowser is automatically stored in a global list in
 * order to get a #MeloBrowser from any place with only the ID provided during
 * instantiation with melo_browser_new().
 */

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

  /**
   * MeloBrowser:id:
   *
   * The ID of the browser. This must be set during the construct and it can
   * be only read after instantiation.
   */
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

/**
 * melo_browser_get_id:
 * @browser: the browser
 *
 * Get the #MeloBrowser ID.
 *
 * Returns: the ID of the #MeloBrowser.
 */
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

  switch (property_id) {
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

  switch (property_id) {
    case PROP_ID:
      g_value_set_string (value, melo_browser_get_id (browser));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

/**
 * melo_browser_get_info:
 * @browser: the browser
 *
 * Get the details of the #MeloBrowser.
 *
 * Returns: a #MeloBrowserInfo or %NULL if get_info callback is not defined.
 */
const MeloBrowserInfo *
melo_browser_get_info (MeloBrowser *browser)
{
  MeloBrowserClass *bclass = MELO_BROWSER_GET_CLASS (browser);

  if (!bclass->get_info)
    return NULL;

  return bclass->get_info (browser);
}

/**
 * melo_browser_get_browser_by_id:
 * @id: the #MeloBrowser ID to retrieve
 *
 * Get an instance of the #MeloBrowser with its ID.
 *
 * Returns: (transfer full): the #MeloBrowser instance or %NULL if not found.
 * Use g_object_unref() after usage.
 */
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

/**
 * melo_browser_new:
 * @type: the type ID of the #MeloBrowser subtype to instantiate
 * @id: the #MeloBrowser ID to use for the new instance
 *
 * Instantiate a new #MeloBrowser subtype with the @id provided. The new browser
 * instance is stored in a global list to be retrieved by its ID with
 * melo_browser_get_browser_by_id().
 *
 * Returns: (transfer full): the new #MeloBrowser instance or %NULL if failed.
 */
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

/**
 * melo_browser_set_player:
 * @browser: the browser
 * @player: the #MeloPlayer instance to associate with
 *
 * Associate the player to the current browser. It allows to use later the
 * melo_browser_get_player() to know the associated #MeloPlayer at any place.
 * This function takes a reference to the player.
 */
void
melo_browser_set_player (MeloBrowser *browser, MeloPlayer *player)
{
  if (browser->player)
    g_object_unref (browser->player);

  browser->player = g_object_ref (player);
}

/**
 * melo_browser_get_player:
 * @browser: the browser
 *
 * Get the current player associated to the browser.
 *
 * Returns: (transfer full): the attached #MeloPlayer instance. Use
 * g_object_unref() after usage.
 */
MeloPlayer *
melo_browser_get_player (MeloBrowser *browser)
{
  g_return_val_if_fail (browser->player, NULL);

  return g_object_ref (browser->player);
}

/**
 * melo_browser_get_list:
 * @browser: the browser
 * @path: the path to get content list
 * @params: parameters to generate list
 *
 * Get the list of items contained by @path. The @params is used to specify some
 * additional filters or preferences to generate the list.
 *
 * Returns: (transfer full): a #MeloBrowserList with all data related to current
 * path and item list. %NULL if an error has occurred.
 * Use melo_browser_list_free() after usage.
 */
MeloBrowserList *
melo_browser_get_list (MeloBrowser *browser, const gchar *path,
                       const MeloBrowserGetListParams *params)
{
  MeloBrowserClass *bclass = MELO_BROWSER_GET_CLASS (browser);

  g_return_val_if_fail (bclass->get_list, NULL);

  return bclass->get_list (browser, path, params);
}

/**
 * melo_browser_search:
 * @browser: the browser
 * @input: the input keywords to use
 * @params: parameters to generate list
 *
 * Get the list of items which satisfy the keywords contained by @input. The
 * @params is used to specify some additional filters or preferences to generate
 * the list.
 *
 * Returns: (transfer full): a #MeloBrowserList with all data related to current
 * research and item list. %NULL if an error has occurred.
 * Use melo_browser_list_free() after usage.
 */
MeloBrowserList *
melo_browser_search (MeloBrowser *browser, const gchar *input,
                     const MeloBrowserSearchParams *params)
{
  MeloBrowserClass *bclass = MELO_BROWSER_GET_CLASS (browser);

  g_return_val_if_fail (bclass->search, NULL);

  return bclass->search (browser, input, params);
}

/**
 * melo_browser_search_hint:
 * @browser: the browser
 * @input: the current keywords input
 *
 * Provide some completion on the current input to help the user for its
 * research.
 *
 * Returns: (transfer full): a string containing some additional values or %NULL
 * if no additional data is available for current input. Use g_free() after
 * usage.
 */
gchar *
melo_browser_search_hint (MeloBrowser *browser, const gchar *input)
{
  MeloBrowserClass *bclass = MELO_BROWSER_GET_CLASS (browser);

  g_return_val_if_fail (bclass->search_hint, NULL);

  return bclass->search_hint (browser, input);
}

/**
 * melo_browser_get_tags:
 * @browser: the browser
 * @path: the item path
 * @fields: the tag fields to get
 *
 * Get the #MeloTags for a specific path. Only the fields selected by @fields
 * are filled in the returned #MeloTags.
 *
 * Return: (transfer full): a #MeloTags containing all tags corresponding to the
 * item pointed by @path. Use melo_tags_unref() after usage.
 */
MeloTags *
melo_browser_get_tags (MeloBrowser *browser, const gchar *path,
                       MeloTagsFields fields)
{
  MeloBrowserClass *bclass = MELO_BROWSER_GET_CLASS (browser);

  g_return_val_if_fail (bclass->get_tags, NULL);

  return bclass->get_tags (browser, path, fields);
}

/**
 * melo_browser_action:
 * @browser: the browser
 * @path: the item path
 * @action: the action to perform
 * @params: the parameters of the action
 *
 * Do the action specified by @action on the item pointed by @path.
 *
 * Returns: %TRUE if the action has been successfully executed, %FALSE
 * otherwise.
 */
gboolean
melo_browser_action (MeloBrowser *browser, const gchar *path,
                     MeloBrowserItemAction action,
                     const MeloBrowserActionParams *params)
{
  MeloBrowserClass *bclass = MELO_BROWSER_GET_CLASS (browser);

  g_return_val_if_fail (bclass->action, FALSE);

  return bclass->action (browser, path, action, params);
}

/**
 * melo_browser_list_new:
 * @path: the path of the current list
 *
 * Create a new #MeloBrowserList which will contain the content detail and list
 * for a specific path.
 *
 * Returns: (transfer full): a new #MeloBrowserList or %NULL if an error
 * occurred. After usage, it should be freed with melo_browser_list_free().
 */
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

/**
 * melo_browser_list_free:
 * @list: the list to free
 *
 * Free the #MeloBrowserList instance.
 */
void
melo_browser_list_free (MeloBrowserList *list)
{
  g_free (list->path);
  g_free (list->prev_token);
  g_free (list->next_token);
  g_list_free_full (list->items, (GDestroyNotify) melo_browser_item_free);
  g_slice_free (MeloBrowserList, list);
}

static const gchar *melo_browser_item_type_map[MELO_BROWSER_ITEM_TYPE_COUNT] = {
  [MELO_BROWSER_ITEM_TYPE_MEDIA] = "media",
  [MELO_BROWSER_ITEM_TYPE_CATEGORY] = "category",
  [MELO_BROWSER_ITEM_TYPE_FILE] = "file",
  [MELO_BROWSER_ITEM_TYPE_FOLDER] = "folder",
  [MELO_BROWSER_ITEM_TYPE_DEVICE] = "device",
  [MELO_BROWSER_ITEM_TYPE_REMOTE] = "remote",
  [MELO_BROWSER_ITEM_TYPE_CUSTOM] = "custom",
};

/**
 * melo_browser_item_type_from_string:
 * @type: the type to convert
 *
 * Convert a string to a #MeloBrowserItemType.
 *
 * Returns: the corresponding #MeloBrowserItemType or
 * MELO_BROWSER_ITEM_TYPE_CUSTOM otherwise.
 */
MeloBrowserItemType
melo_browser_item_type_from_string (const gchar *type)
{
  gint i;

  for (i = 0; i < MELO_BROWSER_ITEM_TYPE_COUNT; i++)
    if (!g_strcmp0 (melo_browser_item_type_map[i], type))
      return i;

  return MELO_BROWSER_ITEM_TYPE_CUSTOM;
}

/**
 * melo_browser_item_type_to_string:
 * @type: the type to convert
 *
 * Convert a #MeloBrowserItemType to a string.
 *
 * Returns: a corresponding string or %NULL if no match has been found.
 */
const gchar *
melo_browser_item_type_to_string (MeloBrowserItemType type)
{
  if (type < MELO_BROWSER_ITEM_TYPE_COUNT)
    return melo_browser_item_type_map[type];
  return NULL;
}

static const gchar *
melo_browser_item_action_map[MELO_BROWSER_ITEM_ACTION_COUNT] = {
  [MELO_BROWSER_ITEM_ACTION_PLAY] = "play",
  [MELO_BROWSER_ITEM_ACTION_ADD] = "add",
  [MELO_BROWSER_ITEM_ACTION_REMOVE] = "remove",
  [MELO_BROWSER_ITEM_ACTION_REMOVE_FILE] = "remove_file",
  [MELO_BROWSER_ITEM_ACTION_EJECT] = "eject",
  [MELO_BROWSER_ITEM_ACTION_CUSTOM] = "custom",
};

/**
 * melo_browser_item_action_from_string:
 * @act: the action to convert
 *
 * Convert a string to a #MeloBrowserItemAction.
 *
 * Returns: the corresponding #MeloBrowserItemAction or
 * MELO_BROWSER_ITEM_ACTION_CUSTOM otherwise.
 */
MeloBrowserItemAction
melo_browser_item_action_from_string (const gchar *act)
{
  gint i;

  for (i = 0; i < MELO_BROWSER_ITEM_ACTION_COUNT; i++)
    if (!g_strcmp0 (melo_browser_item_action_map[i], act))
      return i;

  return MELO_BROWSER_ITEM_ACTION_CUSTOM;
}

/**
 * melo_browser_item_action_to_string:
 * @act: the action to convert
 *
 * Convert a #MeloBrowserItemAction to a string.
 *
 * Returns: a corresponding string or %NULL if no match has been found.
 */
const gchar *
melo_browser_item_action_to_string (MeloBrowserItemAction act)
{
  if (act < MELO_BROWSER_ITEM_ACTION_COUNT)
    return melo_browser_item_action_map[act];
  return NULL;
}

/**
 * melo_browser_item_new:
 * @id: the ID of the item to use for path generation and actions. Can be set
 *    to %NULL and set later with g_strdup().
 * @type: the type of the item
 *
 * Create a new #MeloBrowserItem which will contain detail for one item of a
 * #MeloBrowserList.
 *
 * Returns: (transfer full): a new #MeloBrowserItem or %NULL if an error
 * occurred. After usage, it should be freed with melo_browser_item_free().
 */
MeloBrowserItem *
melo_browser_item_new (const gchar *id, MeloBrowserItemType type)
{
  MeloBrowserItem *item;

  /* Allocate new item */
  item = g_slice_new0 (MeloBrowserItem);
  if (!item)
    return NULL;

  /* Set name and type */
  item->id = g_strdup (id);
  item->type = type;

  return item;
}

/**
 * melo_browser_item_cmp:
 * @a: the first item to compare
 * @b: the second item to compare
 *
 * Compare two #MeloBrowserItem, based on their respective IDs.
 *
 * Returns: 0 if identical.
 */
gint
melo_browser_item_cmp (const MeloBrowserItem *a, const MeloBrowserItem *b)
{
  return g_strcmp0 (a->id, b->id);
}

/**
 * melo_browser_item_free:
 * @item: the item to free
 *
 * Free the #MeloBrowserItem instance.
 */
void
melo_browser_item_free (MeloBrowserItem *item)
{
  if (item->id)
    g_free (item->id);
  if (item->name)
    g_free (item->name);
  if (item->tags)
    melo_tags_unref (item->tags);
  g_slice_free (MeloBrowserItem, item);
}

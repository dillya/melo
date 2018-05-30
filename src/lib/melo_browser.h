/*
 * melo_browser.h: Browser base class
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

#ifndef __MELO_BROWSER_H__
#define __MELO_BROWSER_H__

#include <glib-object.h>

#include "melo_player.h"

G_BEGIN_DECLS

#define MELO_TYPE_BROWSER             (melo_browser_get_type ())
#define MELO_BROWSER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), MELO_TYPE_BROWSER, MeloBrowser))
#define MELO_IS_BROWSER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MELO_TYPE_BROWSER))
#define MELO_BROWSER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), MELO_TYPE_BROWSER, MeloBrowserClass))
#define MELO_IS_BROWSER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), MELO_TYPE_BROWSER))
#define MELO_BROWSER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), MELO_TYPE_BROWSER, MeloBrowserClass))

typedef struct _MeloBrowser MeloBrowser;
typedef struct _MeloBrowserClass MeloBrowserClass;
typedef struct _MeloBrowserPrivate MeloBrowserPrivate;

typedef struct _MeloBrowserInfo MeloBrowserInfo;
typedef struct _MeloBrowserList MeloBrowserList;

typedef enum _MeloBrowserItemType MeloBrowserItemType;
typedef enum _MeloBrowserItemAction MeloBrowserItemAction;
typedef enum _MeloBrowserItemActionFields MeloBrowserItemActionFields;
typedef struct _MeloBrowserItemActionCustom MeloBrowserItemActionCustom;
typedef struct _MeloBrowserItem MeloBrowserItem;

typedef enum _MeloBrowserTagsMode MeloBrowserTagsMode;

typedef struct _MeloBrowserGetListParams MeloBrowserGetListParams;
typedef struct _MeloBrowserGetListParams MeloBrowserSearchParams;
typedef struct _MeloBrowserActionParams MeloBrowserActionParams;

/**
 * MeloBrowser:
 *
 * The opaque #MeloBrowser data structure.
 */
struct _MeloBrowser {
  GObject parent_instance;

  /*< protected */
  MeloPlayer *player;

  /*< private >*/
  MeloBrowserPrivate *priv;
};

/**
 * MeloBrowserClass:
 * @parent_class: Object parent class
 * @get_info: Provide the #MeloBrowserInfo defined by the #MeloBrowser
 * @get_list: Provide the content list for a specific path
 * @search: Search for a specific keywords input
 * @search_hint: Help user by completing its input
 * @get_tags: Provide a #MeloTags containing details on an item
 * @action: Do an action on an item
 *
 * Subclasses must override at least the get_info virtual method. Others can be
 * kept undefined but functionalities will be reduced.
 */
struct _MeloBrowserClass {
  GObjectClass parent_class;

  const MeloBrowserInfo *(*get_info) (MeloBrowser *browser);
  MeloBrowserList *(*get_list) (MeloBrowser *browser, const gchar *path,
                                const MeloBrowserGetListParams *params);
  MeloBrowserList *(*search) (MeloBrowser *browser, const gchar *input,
                              const MeloBrowserSearchParams *params);
  gchar *(*search_hint) (MeloBrowser *browser, const gchar *input);
  MeloTags *(*get_tags) (MeloBrowser *browser, const gchar *path,
                         MeloTagsFields fields);
  gboolean (*action) (MeloBrowser *browser, const gchar *path,
                      MeloBrowserItemAction action,
                      const MeloBrowserActionParams *params);
};

/**
 * MeloBrowserInfo:
 * @name: the display name of the #MeloBrowser
 * @description: the description of the features offered by the #MeloBrowser
 * @search_support: search is supported (must implement melo_browser_search())
 * @search_hint_support: search hint is supported (must implement
 *   melo_browser_search_hint())
 * @search_input_text: text to display into search input, can be %NULL
 * @search_button_text: text to display into search button, can be %NULL
 * @go_support: 'go" is supported: a path can be provided directly instead of
 *   doing a standard navigation with melo_browser_get_list()
 * @go_list_support: the melo_browser_get_list() can be called with 'go'
 * @go_play_support: the melo_browser_action() with MELO_BROWSER_ITEM_ACTION_PLAY
 *   can be called with 'go'
 * @go_add_support: the melo_browser_action() with MELO_BROWSER_ITEM_ACTION_ADD
 *   can be called with 'go'
 * @go_input_text: text to display in 'go' input, can be %NULL
 * @go_button_list_text: text to display in 'go' list button, can be %NULL
 * @go_button_play_text: text to display in 'go' play button, can be %NULL
 * @go_button_add_text: text to display in 'go' add button, can be %NULL
 * @tags_support: tags are supported (must implement melo_browser_get_tags())
 * @tags_cache_support: caching of #MeloTags is supported, which means that
 *   tags already cached will be retrieved fast, especially in case of a
 *   melo_browser_get_list() / melo_browser_search().
 *
 * MeloBrowserInfo provides all details on a #MeloBrowser instance as its name,
 * description, capabilities, ... It is important to define this structure in
 * order to give a correct feedback for user.
 */
struct _MeloBrowserInfo {
  const gchar *name;
  const gchar *description;
  gboolean search_support;
  gboolean search_hint_support;
  const gchar *search_input_text;
  const gchar *search_button_text;
  gboolean go_support;
  gboolean go_list_support;
  gboolean go_play_support;
  gboolean go_add_support;
  const gchar *go_input_text;
  const gchar *go_button_list_text;
  const gchar *go_button_play_text;
  const gchar *go_button_add_text;
  gboolean tags_support;
  gboolean tags_cache_support;
};

/**
 * MeloBrowserList:
 * @path: current path for which list has been generated
 * @count: number of items in the list
 * @prev_token: token used to iterate backward in list for the current path
 * @next_token: token used to iterate forward in list for the current path
 * @items: a #GList of #MeloBrowserItem
 *
 * A list can be restricted to a specific number of item at each call. In this
 * case, the @count will be set lower than the requested count. Then, the user
 * must iterate through the list with the @offset fields from the
 * #MeloBrowserGetListParams.
 * Some browser can need to use a token for navigation, in order to guarantee
 * always the same list, even if an update has been done while getting the list
 * item in multiple iteration. In order to provide the best flexibility, two
 * tokens are provided: @prev_token and @next_token to respectively get the
 * previous items and next items in the list for next call to
 * melo_browser_get_list() / melo_browser_search().
 */
struct _MeloBrowserList {
  gchar *path;
  gint count;
  gchar *prev_token;
  gchar *next_token;
  GList *items;
};

/**
 * MeloBrowserItemType:
 * @MELO_BROWSER_ITEM_TYPE_MEDIA: item is a media (should be playable)
 * @MELO_BROWSER_ITEM_TYPE_CATEGORY: item is a category and contains sub-items
 * @MELO_BROWSER_ITEM_TYPE_FILE: item is a file (should be playable)
 * @MELO_BROWSER_ITEM_TYPE_FOLDER: item is a file system folder
 * @MELO_BROWSER_ITEM_TYPE_DEVICE: item is a device like a USB flash drive
 * @MELO_BROWSER_ITEM_TYPE_REMOTE: item is a remote item like a NAS
 * @MELO_BROWSER_ITEM_TYPE_CUSTOM: item is custom
 * @MELO_BROWSER_ITEM_TYPE_COUNT: Number of item types defined
 *
 * MeloBrowserItemType defines the basic type of item listed by
 * melo_browser_get_list() and melo_browser_search(). If a custom type is needed
 * the type must be set to @MELO_BROWSER_ITEM_TYPE_CUSTOM and field @type_custom
 * of #MeloBrowserItem must be set.
 */
enum _MeloBrowserItemType {
  MELO_BROWSER_ITEM_TYPE_MEDIA = 0,
  MELO_BROWSER_ITEM_TYPE_CATEGORY,
  MELO_BROWSER_ITEM_TYPE_FILE,
  MELO_BROWSER_ITEM_TYPE_FOLDER,
  MELO_BROWSER_ITEM_TYPE_DEVICE,
  MELO_BROWSER_ITEM_TYPE_REMOTE,
  MELO_BROWSER_ITEM_TYPE_CUSTOM,

  MELO_BROWSER_ITEM_TYPE_COUNT
};

/**
 * MeloBrowserItemAction:
 * @MELO_BROWSER_ITEM_ACTION_PLAY: Play item
 * @MELO_BROWSER_ITEM_ACTION_ADD: Add item to playlist
 * @MELO_BROWSER_ITEM_ACTION_REMOVE: Remove item from browser
 * @MELO_BROWSER_ITEM_ACTION_REMOVE_FILE: Remove item from browser and file
 *    system
 * @MELO_BROWSER_ITEM_ACTION_EJECT: Eject device / remote item
 * @MELO_BROWSER_ITEM_ACTION_CUSTOM: Custom action
 * @MELO_BROWSER_ITEM_ACTION_COUNT: Number of item actions defined
 *
 * MeloBrowserItemAction is the action to perform on an item. It is used in
 * melo_browser_action() to specify what to do on the item. If a custom action
 * is needed the action must be set to @MELO_BROWSER_ITEM_ACTION_CUSTOM and
 * list @actions_custom of #MeloBrowserItem must be set.
 */
enum _MeloBrowserItemAction {
  MELO_BROWSER_ITEM_ACTION_PLAY = 0,
  MELO_BROWSER_ITEM_ACTION_ADD,
  MELO_BROWSER_ITEM_ACTION_REMOVE,
  MELO_BROWSER_ITEM_ACTION_REMOVE_FILE,
  MELO_BROWSER_ITEM_ACTION_EJECT,
  MELO_BROWSER_ITEM_ACTION_CUSTOM,

  MELO_BROWSER_ITEM_ACTION_COUNT
};

/**
 * MeloBrowserItemActionFields:
 * @MELO_BROWSER_ITEM_ACTION_FIELDS_NONE: No action
 * @MELO_BROWSER_ITEM_ACTION_FIELDS_PLAY: Playing item
 * @MELO_BROWSER_ITEM_ACTION_FIELDS_ADD: Adding item to playlist
 * @MELO_BROWSER_ITEM_ACTION_FIELDS_REMOVE: Removing item from browser
 * @MELO_BROWSER_ITEM_ACTION_FIELDS_REMOVE_FILE: Removing item from browser and
 *    file system
 * @MELO_BROWSER_ITEM_ACTION_FIELDS_EJECT: Ejecting device / remote item
 * @MELO_BROWSER_ITEM_ACTION_FIELDS_CUSTOM: Custom action
 * @MELO_BROWSER_ITEM_ACTION_FIELDS_FULL: All action
 *
 * MeloBrowserItemActionFields is a bit field to describe supported actions or
 * to list the actions to perform by/on an item.
 */
enum _MeloBrowserItemActionFields {
  MELO_BROWSER_ITEM_ACTION_FIELDS_NONE = 0,
  MELO_BROWSER_ITEM_ACTION_FIELDS_PLAY =
    (1 << MELO_BROWSER_ITEM_ACTION_PLAY),
  MELO_BROWSER_ITEM_ACTION_FIELDS_ADD =
    (1 << MELO_BROWSER_ITEM_ACTION_ADD),
  MELO_BROWSER_ITEM_ACTION_FIELDS_REMOVE =
    (1 << MELO_BROWSER_ITEM_ACTION_REMOVE),
  MELO_BROWSER_ITEM_ACTION_FIELDS_REMOVE_FILE =
    (1 << MELO_BROWSER_ITEM_ACTION_REMOVE_FILE),
  MELO_BROWSER_ITEM_ACTION_FIELDS_EJECT =
    (1 << MELO_BROWSER_ITEM_ACTION_EJECT),
  MELO_BROWSER_ITEM_ACTION_FIELDS_CUSTOM =
    (1 << MELO_BROWSER_ITEM_ACTION_CUSTOM),

  MELO_BROWSER_ITEM_ACTION_FIELDS_FULL = ~0
};

/**
 * MeloBrowserItemActionCustom:
 * @id: the ID of the custom action
 * @name: the display name of the custom action
 *
 * MeloBrowserItemActionCustom describes a custom item action.
 */
struct _MeloBrowserItemActionCustom {
  gchar *id;
  gchar *name;
};

/**
 * MeloBrowserItem:
 * @id: the ID of the current item, used to generate path and do actions
 * @name: the display name for the item, can be %NULL
 * @tags: a #MeloTags which contains requested tags, can be %NULL
 * @type: the item type
 * @type_custom: the name of the custom type if MELO_BROWSER_ITEM_TYPE_CUSTOM is
 *    set in @type
 * @actions: a list of supported action for the current item
 * @actions_custom: a list of custom actions, if the
 *    MELO_BROWSER_ITEM_ACTION_FIELDS_CUSTOM is set in @actions. The last item
 *    of the list must have an ID equal to NULL
 *
 * MeloBrowserItem contains all details about an item in #MeloBrowserList.
 */
struct _MeloBrowserItem {
  gchar *id;
  gchar *name;
  MeloTags *tags;
  MeloBrowserItemType type;
  const gchar *type_custom;
  MeloBrowserItemActionFields actions;
  const MeloBrowserItemActionCustom *actions_custom;
};

/**
 * MeloBrowserTagsMode:
 * @MELO_BROWSER_TAGS_MODE_NONE: no tags requested for the list
 * @MELO_BROWSER_TAGS_MODE_FULL: force getting tags for each item in list, which
 *    can lead in long call
 * @MELO_BROWSER_TAGS_MODE_ONLY_CACHED: get only tags that are cached and can
 *    be retrieved without any delay, which leads in a fast call
 * @MELO_BROWSER_TAGS_MODE_NONE_WITH_CACHING: no tags requested for the list but
 *    start parsing all items in order to fill the tags cache, which leads in a
 *    fast call and fast availability of the tags through next call or when
 *    getting tags separately with melo_browser_get_tags()
 * @MELO_BROWSER_TAGS_MODE_FULL_WITH_CACHING: force getting tags for each item
 *    in list but in using the cache if possible, which can lead in faster call
 *    than @MELO_BROWSER_TAGS_MODE_FULL, but still slower than other options
 * @MELO_BROWSER_TAGS_MODE_COUNT: Number of tags mode available
 *
 * MeloBrowserTagsMode allows to specify the cache strategy to use to get tags
 * during a call to melo_browser_get_list() / melo_browser_search(). The
 * different modes can lead in slower or faster list generation.
 */
enum _MeloBrowserTagsMode {
  MELO_BROWSER_TAGS_MODE_NONE = 0,
  MELO_BROWSER_TAGS_MODE_FULL,
  MELO_BROWSER_TAGS_MODE_ONLY_CACHED,
  MELO_BROWSER_TAGS_MODE_NONE_WITH_CACHING,
  MELO_BROWSER_TAGS_MODE_FULL_WITH_CACHING,

  MELO_BROWSER_TAGS_MODE_COUNT
};

/**
 * MeloBrowserGetListParams:
 * @offset: number of items to skip in the list
 * @count: number of items to get, starting at @offset
 * @sort: the sort method to apply on list (see #MeloSort)
 * @token: token to use to generate the list, can be %NULL
 * @tags_mode: the #MeloTags caching mode to use
 * @tags_fields: the tag fields to fill in item #MeloTags
 *
 * MeloBrowserGetListParams is used to apply some filters and options on list
 * generation done during melo_browser_get_list() or melo_browser_search()
 * calls. The @token field is used only if the first call to one of the
 * functions above has returned a @prev_token / @next_token.
 */
struct _MeloBrowserGetListParams {
  gint offset;
  gint count;
  MeloSort sort;
  const gchar *token;
  MeloBrowserTagsMode tags_mode;
  MeloTagsFields tags_fields;
};

/**
 * MeloBrowserActionParams:
 * @sort: the sort method to apply (useful when action is on an item which
 *    contains sub-items)
 * @token: token to use to perform the action
 *
 * MeloBrowserActionParams is used when doing an action with
 * melo_browser_action(). It allows to pass the sort method to apply if the
 * action is performed on a list, and to specify a token (when returned by a
 * call to melo_browser_get_list() / melo_browser_search()) to keep the same
 * state as the list of item with which action has been triggered.
 */
struct _MeloBrowserActionParams {
  MeloSort sort;
  const gchar *token;
};

GType melo_browser_get_type (void);

MeloBrowser *melo_browser_new (GType type, const gchar *id);
const gchar *melo_browser_get_id (MeloBrowser *browser);
const MeloBrowserInfo *melo_browser_get_info (MeloBrowser *browser);
MeloBrowser *melo_browser_get_browser_by_id (const gchar *id);

void melo_browser_set_player (MeloBrowser *browser, MeloPlayer *player);
MeloPlayer *melo_browser_get_player (MeloBrowser *browser);

MeloBrowserList *melo_browser_get_list (MeloBrowser *browser, const gchar *path,
                                        const MeloBrowserGetListParams *params);
MeloBrowserList *melo_browser_search (MeloBrowser *browser, const gchar *input,
                                      const MeloBrowserSearchParams *params);
gchar *melo_browser_search_hint (MeloBrowser *browser, const gchar *input);
MeloTags *melo_browser_get_tags (MeloBrowser *browser, const gchar *path,
                                 MeloTagsFields fields);
gboolean melo_browser_action (MeloBrowser *browser, const gchar *path,
                              MeloBrowserItemAction action,
                              const MeloBrowserActionParams *params);

MeloBrowserList *melo_browser_list_new (const gchar *path);
void melo_browser_list_free (MeloBrowserList *list);

MeloBrowserItemType melo_browser_item_type_from_string (const gchar *type);
const gchar *melo_browser_item_type_to_string (MeloBrowserItemType type);

MeloBrowserItemAction melo_browser_item_action_from_string (const gchar *act);
const gchar *melo_browser_item_action_to_string (MeloBrowserItemAction act);

MeloBrowserItem *melo_browser_item_new (const gchar *id,
                                        MeloBrowserItemType type);
gint melo_browser_item_cmp (const MeloBrowserItem *a, const MeloBrowserItem *b);
void melo_browser_item_free (MeloBrowserItem *item);

G_END_DECLS

#endif /* __MELO_BROWSER_H__ */

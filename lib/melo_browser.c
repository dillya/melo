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

/**
 * SECTION:melo_browser
 * @title: MeloBrowser
 * @short_description: Base class for Melo browser
 *
 * #MeloBrowser is the main class to handle media / library / file system
 * browsing interface to find which media to listen with Melo.
 */

#define MELO_LOG_TAG "browser"
#include "melo/melo_log.h"

#include "melo_events.h"
#include "melo_requests.h"

#include "melo/melo_browser.h"

/* Global browser list */
G_LOCK_DEFINE_STATIC (melo_browser_mutex);
static GHashTable *melo_browser_list;

/* Browser event listeners */
static MeloEvents melo_browser_events;

/* Melo browser private data */
typedef struct _MeloBrowserPrivate {
  char *id;
  char *name;
  char *description;
  char *icon;
  bool support_search;

  MeloEvents events;
  MeloRequests requests;
} MeloBrowserPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (MeloBrowser, melo_browser, G_TYPE_OBJECT)

/* Melo browser properties */
enum {
  PROP_0,
  PROP_ID,
  PROP_NAME,
  PROP_DESCRIPTION,
  PROP_ICON,
  PROP_SUPPORT_SEARCH
};

static void melo_browser_constructed (GObject *object);
static void melo_browser_finalize (GObject *object);

static void melo_browser_set_property (
    GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void melo_browser_get_property (
    GObject *object, guint property_id, GValue *value, GParamSpec *pspec);

/* Protobuf message */
static MeloMessage *melo_browser_message_add (MeloBrowserPrivate *priv);
static MeloMessage *melo_browser_message_remove (MeloBrowserPrivate *priv);

static void
melo_browser_class_init (MeloBrowserClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  /* Override constructed to capture ID and register the browser */
  object_class->constructed = melo_browser_constructed;

  /* Override finalize to un-register the browser and free private data */
  object_class->finalize = melo_browser_finalize;

  /* Override properties accessors */
  object_class->set_property = melo_browser_set_property;
  object_class->get_property = melo_browser_get_property;

  /**
   * MeloBrowser:id:
   *
   * The unique ID of the browser. This must be set during the construct and it
   * can be only read after instantiation. The value provided at construction
   * will be used to register the browser in the global list which will be
   * exported to user interface.
   */
  g_object_class_install_property (object_class, PROP_ID,
      g_param_spec_string ("id", "ID", "Browser unique ID.", NULL,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  /**
   * MeloBrowser:name:
   *
   * The name of the browser to display. This must be set during the construct
   * and it can be only read after instantiation.
   */
  g_object_class_install_property (object_class, PROP_NAME,
      g_param_spec_string ("name", "Name", "Browser name.", NULL,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  /**
   * MeloBrowser:description:
   *
   * The description of the browser. This must be set during the construct and
   * it can be only read after instantiation.
   */
  g_object_class_install_property (object_class, PROP_DESCRIPTION,
      g_param_spec_string ("description", "Description", "Browser description.",
          NULL,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  /**
   * MeloBrowser:icon:
   *
   * The icon of the browser. This must be set during the construct and it can
   * be only read after instantiation.
   */
  g_object_class_install_property (object_class, PROP_ICON,
      g_param_spec_string ("icon", "Icon", "Browser icon.", NULL,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  /**
   * MeloBrowser:support-search:
   *
   * The browser supports search requests. This must be set during the construct
   * and it can be only read after instantiation.
   */
  g_object_class_install_property (object_class, PROP_SUPPORT_SEARCH,
      g_param_spec_boolean ("support-search", "Supports Search",
          "Browser search feature.", false,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static void
melo_browser_init (MeloBrowser *self)
{
}

static void
melo_browser_constructed (GObject *object)
{
  MeloBrowser *browser = MELO_BROWSER (object);
  MeloBrowserPrivate *priv = melo_browser_get_instance_private (browser);

  /* Register browser */
  if (priv->id) {
    gboolean added = FALSE;

    /* Lock browser list */
    G_LOCK (melo_browser_mutex);

    /* Create browser list */
    if (!melo_browser_list)
      melo_browser_list = g_hash_table_new (g_str_hash, g_str_equal);

    /* Insert browser into list*/
    if (melo_browser_list &&
        g_hash_table_contains (melo_browser_list, priv->id) == FALSE) {
      g_hash_table_insert (melo_browser_list, (gpointer) priv->id, browser);
      added = TRUE;
    }

    /* Unlock browser list */
    G_UNLOCK (melo_browser_mutex);

    /* Browser added */
    if (added == TRUE) {
      /* Broadcast 'add' message */
      melo_events_broadcast (
          &melo_browser_events, melo_browser_message_add (priv));

      MELO_LOGI ("browser '%s' added", priv->id);
    } else
      MELO_LOGW ("failed to add browser '%s' to global list", priv->id);
  }

  /* Chain constructed */
  G_OBJECT_CLASS (melo_browser_parent_class)->constructed (object);
}

static void
melo_browser_finalize (GObject *object)
{
  MeloBrowser *browser = MELO_BROWSER (object);
  MeloBrowserPrivate *priv = melo_browser_get_instance_private (browser);

  /* Un-register browser */
  if (priv->id) {
    gboolean removed = FALSE;

    /* Lock browser list */
    G_LOCK (melo_browser_mutex);

    /* Remove browser from list */
    if (melo_browser_list) {
      g_hash_table_remove (melo_browser_list, priv->id);
      removed = TRUE;
    }

    /* Destroy browser lists */
    if (melo_browser_list && !g_hash_table_size (melo_browser_list)) {
      /* Destroy browser list */
      g_hash_table_destroy (melo_browser_list);
      melo_browser_list = NULL;
    }

    /* Unlock browser list */
    G_UNLOCK (melo_browser_mutex);

    /* Browser removed */
    if (removed == TRUE) {
      /* Broadcast 'remove' message */
      melo_events_broadcast (
          &melo_browser_events, melo_browser_message_remove (priv));

      MELO_LOGI ("browser '%s' removed", priv->id);
    }
  }

  /* Free ID */
  g_free (priv->id);

  /* Chain finalize */
  G_OBJECT_CLASS (melo_browser_parent_class)->finalize (object);
}

static void
melo_browser_set_property (
    GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
  MeloBrowserPrivate *priv =
      melo_browser_get_instance_private (MELO_BROWSER (object));

  switch (property_id) {
  case PROP_ID:
    g_free (priv->id);
    priv->id = g_value_dup_string (value);
    break;
  case PROP_NAME:
    g_free (priv->name);
    priv->name = g_value_dup_string (value);
    break;
  case PROP_DESCRIPTION:
    g_free (priv->description);
    priv->description = g_value_dup_string (value);
    break;
  case PROP_ICON:
    g_free (priv->icon);
    priv->icon = g_value_dup_string (value);
    break;
  case PROP_SUPPORT_SEARCH:
    priv->support_search = g_value_get_boolean (value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
melo_browser_get_property (
    GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
  MeloBrowserPrivate *priv =
      melo_browser_get_instance_private (MELO_BROWSER (object));

  switch (property_id) {
  case PROP_ID:
    g_value_set_string (value, priv->id);
    break;
  case PROP_NAME:
    g_value_set_string (value, priv->name);
    break;
  case PROP_DESCRIPTION:
    g_value_set_string (value, priv->description);
    break;
  case PROP_ICON:
    g_value_set_string (value, priv->icon);
    break;
  case PROP_SUPPORT_SEARCH:
    g_value_set_boolean (value, priv->support_search);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

/*
 * Static functions
 */

static MeloBrowser *
melo_browser_get_by_id (const char *id)
{
  MeloBrowser *browser = NULL;

  /* Lock browser list */
  G_LOCK (melo_browser_mutex);

  if (melo_browser_list && id)
    browser = g_hash_table_lookup (melo_browser_list, id);
  if (browser)
    g_object_ref (browser);

  /* Unlock browser list */
  G_UNLOCK (melo_browser_mutex);

  return browser;
}

static MeloMessage *
melo_browser_message_add (MeloBrowserPrivate *priv)
{
  return NULL;
}

static MeloMessage *
melo_browser_message_remove (MeloBrowserPrivate *priv)
{
  return NULL;
}

static void
melo_browser_list_cb (gpointer key, gpointer value, gpointer user_data)
{
  MeloBrowser *browser = value;
  MeloAsyncData *data = user_data;
  MeloBrowserPrivate *priv = melo_browser_get_instance_private (browser);
  MeloMessage *msg;

  /* Generate 'add' message */
  msg = melo_browser_message_add (priv);
  data->cb (msg, data->user_data);
  melo_message_unref (msg);
}

/**
 * melo_browser_add_event_listener:
 * @id: the id of a #MeloBrowser or %NULL for global browser
 * @cb: the function to call when a new event occurred
 * @user_data: data to pass to @cb
 *
 * This function will add the @cb and @user_data to internal list of event
 * listener and the @cb will be called when a new event occurs.
 *
 * If @id is %NULL, the event listener will be added to the global browser
 * class, otherwise, the event listener will be added to a specific browser
 * instance, pointed by its ID. If the browser doesn't exist, %false will be
 * returned.
 *
 * When a new global listener is added, the current browser list is sent to it.
 *
 * The @cb / @user_data couple is used to identify an event listener, so a
 * second call to this function with the same parameters will fails. You must
 * use the same couple in melo_browser_remove_event_listener() to remove this
 * event listener.
 *
 * Returns: %true if the listener has been added successfully, %false otherwise.
 */
bool
melo_browser_add_event_listener (
    const char *id, MeloAsyncCb cb, void *user_data)
{
  bool ret;

  /* Add event listener to global browser or specific browser */
  if (id) {
    MeloBrowserPrivate *priv;
    MeloBrowser *browser;

    /* Find browser */
    browser = melo_browser_get_by_id (id);
    if (!browser)
      return false;
    priv = melo_browser_get_instance_private (browser);

    /* Add event listener to browser */
    ret = melo_events_add_listener (&priv->events, cb, user_data);
    g_object_unref (browser);
  } else {
    /* Add event listener to global browser */
    ret = melo_events_add_listener (&melo_browser_events, cb, user_data);

    /* Send browser list to new listener */
    if (ret) {
      MeloAsyncData data = {.cb = cb, .user_data = user_data};

      /* Send browser list */
      g_hash_table_foreach (melo_browser_list, melo_browser_list_cb, &data);
    }
  }

  return ret;
}

/**
 * melo_browser_remove_event_listener:
 * @id: the id of a #MeloBrowser or %NULL for global browser
 * @cb: the function to remove
 * @user_data: data passed with @cb during add
 *
 * This function will remove the @cb and @user_data from internal list of event
 * listener and the @cb won't be called anymore after the function returns.
 *
 * Returns: %true if the listener has been removed successfully, %false
 *     otherwise.
 */
bool
melo_browser_remove_event_listener (
    const char *id, MeloAsyncCb cb, void *user_data)
{
  bool ret;

  if (id) {
    MeloBrowserPrivate *priv;
    MeloBrowser *browser;

    /* Find browser */
    browser = melo_browser_get_by_id (id);
    if (!browser)
      return false;
    priv = melo_browser_get_instance_private (browser);

    /* Remove event listener to browser */
    ret = melo_events_remove_listener (&priv->events, cb, user_data);
    g_object_unref (browser);
  } else
    ret = melo_events_remove_listener (&melo_browser_events, cb, user_data);

  return ret;
}

/**
 * melo_browser_handle_request:
 * @id: the id of a #MeloBrowser
 * @msg: the #MeloMessage to handle
 * @cb: the function to call when a response is sent
 * @user_data: data to pass to @cb
 *
 * This function is called when a new browser request is received by the
 * application. This function will handle and forward the request to the
 * destination browser instance. If the browser doesn't exist, the function will
 * return %false.
 *
 * If the request is malformed or an internal error occurs, the function will
 * return %false, otherwise %true will be returned.
 * After returning, many asynchronous tasks related to the request can still be
 * pending, so @cb and @user_data should not be destroyed. If the request need
 * to stopped / cancelled, melo_browser_cancel_request() is intended for.
 *
 * Returns: %true if the message has been handled by the browser, %false
 *     otherwise.
 */
bool
melo_browser_handle_request (
    const char *id, MeloMessage *msg, MeloAsyncCb cb, void *user_data)
{
  MeloBrowserClass *class;
  MeloBrowserPrivate *priv;
  MeloBrowser *browser;
  bool ret = false;

  if (!id || !msg)
    return false;

  /* Find browser */
  browser = melo_browser_get_by_id (id);
  if (!browser)
    return false;
  class = MELO_BROWSER_GET_CLASS (browser);
  priv = melo_browser_get_instance_private (browser);

  /* Forward message to browser */
  if (class->handle_request) {
    MeloAsyncData async = {
        .cb = cb,
        .user_data = user_data,
    };
    MeloRequest *req;

    /* Create new request */
    req =
        melo_requests_new_request (&priv->requests, &async, G_OBJECT (browser));
    if (req) {
      /* Forward message */
      ret = class->handle_request (browser, msg, req);
      if (!ret)
        melo_request_unref (req);
    }
  }

  /* Release browser */
  g_object_unref (browser);

  return ret;
}

/**
 * melo_browser_cancel_request:
 * @id: the id of a #MeloBrowser
 * @cb: the function used during call to melo_browser_handle_request()
 * @user_data: data passed with @cb
 *
 * This function can be called to cancel a running or a pending request. If the
 * request exists, the asynchronous tasks will be cancelled and @cb will be
 * called with a NULL-message to signal end of request. If the request is
 * already finished or a cancellation is already pending, this function will do
 * nothing.
 */
void
melo_browser_cancel_request (const char *id, MeloAsyncCb cb, void *user_data)
{
  MeloAsyncData async = {
      .cb = cb,
      .user_data = user_data,
  };
  MeloBrowserPrivate *priv;
  MeloBrowser *browser;

  /* Find browser */
  browser = melo_browser_get_by_id (id);
  if (!browser)
    return;
  priv = melo_browser_get_instance_private (browser);

  /* Cancel / stop request */
  melo_requests_cancel_request (&priv->requests, &async);
  g_object_unref (browser);
}

/**
 * melo_browser_get_asset:
 * @id: the id of a #MeloBrowser
 * @asset: the id of the asset to get
 *
 * This function returns the URI of an asset identified by @asset from a browser
 * identified by @id. If the asset doesn't exist, %NULL is returned.
 *
 * Returns: the URI of the asset or %NULL.
 */
char *
melo_browser_get_asset (const char *id, const char *asset)
{
  MeloBrowserClass *class;
  MeloBrowser *browser;
  char *ret = NULL;

  if (!id || !asset)
    return NULL;

  /* Find browser */
  browser = melo_browser_get_by_id (id);
  if (!browser)
    return NULL;
  class = MELO_BROWSER_GET_CLASS (browser);

  /* Get asset from browser */
  if (class->get_asset)
    ret = class->get_asset (browser, asset);

  /* Release browser */
  g_object_unref (browser);

  return ret;
}

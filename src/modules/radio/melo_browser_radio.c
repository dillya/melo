/*
 * melo_browser_radio.c: Radio Browser using LibSoup
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

#include <libsoup/soup.h>
#include <json-glib/json-glib.h>

#include "melo_browser_radio.h"

/* Radio browser info */
static MeloBrowserInfo melo_browser_radio_info = {
  .name = "Browse radios",
  .description = "Navigate though more than 30,000 radio and webradio",
  .tags_support = TRUE,
  .tags_cache_support = FALSE,
  /* Search feature */
  .search_support = TRUE,
  .search_input_text = "Type a radio name or a genre...",
  .search_button_text = "Go",
};

static const MeloBrowserInfo *melo_browser_radio_get_info (
                                                          MeloBrowser *browser);
static MeloBrowserList *melo_browser_radio_get_list (MeloBrowser *browser,
                                                  const gchar *path,
                                                  gint offset, gint count,
                                                  const gchar *token,
                                                  MeloBrowserTagsMode tags_mode,
                                                  MeloTagsFields tags_fields);
static MeloBrowserList *melo_browser_radio_search (MeloBrowser *browser,
                                                  const gchar *input,
                                                  gint offset, gint count,
                                                  MeloBrowserTagsMode tags_mode,
                                                  MeloTagsFields tags_fields);
static gboolean melo_browser_radio_play (MeloBrowser *browser,
                                         const gchar *path);

struct _MeloBrowserRadioPrivate {
  GMutex mutex;
  SoupSession *session;
};

G_DEFINE_TYPE_WITH_PRIVATE (MeloBrowserRadio, melo_browser_radio, MELO_TYPE_BROWSER)

static void
melo_browser_radio_finalize (GObject *gobject)
{
  MeloBrowserRadio *browser_radio = MELO_BROWSER_RADIO (gobject);
  MeloBrowserRadioPrivate *priv =
                        melo_browser_radio_get_instance_private (browser_radio);

  /* Free Soup session */
  g_object_unref (priv->session);

  /* Clear mutex */
  g_mutex_clear (&priv->mutex);

  /* Chain up to the parent class */
  G_OBJECT_CLASS (melo_browser_radio_parent_class)->finalize (gobject);
}

static void
melo_browser_radio_class_init (MeloBrowserRadioClass *klass)
{
  MeloBrowserClass *bclass = MELO_BROWSER_CLASS (klass);
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  bclass->get_info = melo_browser_radio_get_info;
  bclass->get_list = melo_browser_radio_get_list;
  bclass->search = melo_browser_radio_search;
  bclass->play = melo_browser_radio_play;

  /* Add custom finalize() function */
  oclass->finalize = melo_browser_radio_finalize;
}

static void
melo_browser_radio_init (MeloBrowserRadio *self)
{
  MeloBrowserRadioPrivate *priv =
                                 melo_browser_radio_get_instance_private (self);

  self->priv = priv;

  /* Init mutex */
  g_mutex_init (&priv->mutex);

  /* Create a new Soup session */
  priv->session = soup_session_new_with_options (
                                SOUP_SESSION_USER_AGENT, "Melo",
                                NULL);
}

static const MeloBrowserInfo *
melo_browser_radio_get_info (MeloBrowser *browser)
{
  return &melo_browser_radio_info;
}

static GList *
melo_browser_radio_parse (MeloBrowserRadio *bradio, const gchar *url)
{
  SoupMessage *msg;
  GInputStream *stream;
  GList *list = NULL;
  JsonParser *parser;
  JsonNode *node;
  JsonArray *array;
  JsonObject *obj;
  gint count, i;

  /* Create request */
  msg = soup_message_new ("GET", url);

  /* Send message and wait answer */
  stream = soup_session_send (bradio->priv->session, msg, NULL, NULL);
  if (!stream)
    goto bad_request;

  /* Bad status */
  if (msg->status_code != 200)
    goto bad_status;

  /* Parse JSON */
  parser = json_parser_new ();
  if (!json_parser_load_from_stream (parser, stream, NULL, NULL))
    goto bad_json;

  /* Get root node and check its type */
  node = json_parser_get_root (parser);
  if (!node || json_node_get_node_type (node) != JSON_NODE_ARRAY)
    goto bad_json;

  /* Get array from node */
  array = json_node_get_array (node);
  count = json_array_get_length (array);
  for (i = 0; i < count; i ++) {
    const gchar *name, *full_name, *type;
    MeloBrowserItem *item;

    /* Get next entry */
    obj = json_array_get_object_element (array, i);
    if (!obj)
      continue;

    /* Get name, full_name and type */
    name = json_object_get_string_member (obj, "id");
    full_name = json_object_get_string_member (obj, "name");
    type = json_object_get_string_member (obj, "type");

    /* Generate new item */
    item = melo_browser_item_new (name, NULL);
    item->full_name = full_name ? g_strdup (full_name) : g_strdup ("Unknown");
    item->type = *type == 'm' ? g_strdup ("category") : g_strdup ("radio");

    /* Add item to list */
    list = g_list_prepend (list, item);
  }

  /* Reverse list */
  list = g_list_reverse (list);

  /* Free objects */
  g_object_unref (parser);
  g_object_unref (stream);
  g_object_unref (msg);

  return list;

bad_json:
  g_object_unref (parser);
bad_status:
  g_object_unref (stream);
bad_request:
  g_object_unref (msg);
  return NULL;
}

static MeloBrowserList *
melo_browser_radio_get_list (MeloBrowser *browser, const gchar *path,
                             gint offset, gint count, const gchar *token,
                             MeloBrowserTagsMode tags_mode,
                             MeloTagsFields tags_fields)
{
  MeloBrowserRadio *bradio = MELO_BROWSER_RADIO (browser);
  static MeloBrowserList *list;
  gchar *url;
  gint page;

  /* Create browser list */
  list = melo_browser_list_new (path);
  if (!list)
    return NULL;

  /* Generate URL */
  page = (offset / count) + 1;
  url = g_strdup_printf ("http://www.sparod.com/radio%s?count=%d&page=%d",
                         path, count, page);

  /* Get list from URL */
  list->items = melo_browser_radio_parse (bradio, url);
  g_free (url);

  return list;
}

static MeloBrowserList *
melo_browser_radio_search (MeloBrowser *browser, const gchar *input,
                           gint offset, gint count,
                           MeloBrowserTagsMode tags_mode,
                           MeloTagsFields tags_fields)
{
  MeloBrowserRadio *bradio = MELO_BROWSER_RADIO (browser);
  static MeloBrowserList *list;
  gchar *url;
  gint page;

  /* Create browser list */
  list = melo_browser_list_new ("/search/0/");
  if (!list)
    return NULL;

  /* Generate URL */
  page = (offset / count) + 1;
  url = g_strdup_printf ("http://www.sparod.com/radio/search/%s?"
                         "count=%d&page=%d", input, count, page);

  /* Get list from URL */
  list->items = melo_browser_radio_parse (bradio, url);
  g_free (url);

  return list;
}

static gboolean
melo_browser_radio_play (MeloBrowser *browser, const gchar *path)
{
  MeloBrowserRadio *bradio = MELO_BROWSER_RADIO (browser);
  SoupMessage *msg;
  GInputStream *stream;
  JsonParser *parser;
  JsonNode *node;
  JsonObject *obj;
  const gchar *name, *surl;
  gchar *url;
  gboolean ret;

  /* Generate URL */
  url = g_strdup_printf ("http://www.sparod.com/radio%s", path);

  /* Create request */
  msg = soup_message_new ("GET", url);
  g_free (url);

  /* Send message and wait answer */
  stream = soup_session_send (bradio->priv->session, msg, NULL, NULL);
  if (!stream)
    goto bad_request;

  /* Bad status */
  if (msg->status_code != 200)
    goto bad_status;

  /* Parse JSON */
  parser = json_parser_new ();
  if (!json_parser_load_from_stream (parser, stream, NULL, NULL))
    goto bad_json;

  /* Get root node and check its type */
  node = json_parser_get_root (parser);
  if (!node || json_node_get_node_type (node) != JSON_NODE_OBJECT)
    goto bad_json;

  /* Get object from node */
  obj = json_node_get_object (node);
  if (!obj)
    goto bad_json;

  /* Get stream URL */
  name = json_object_get_string_member (obj, "name");
  surl = json_object_get_string_member (obj, "url");
  if (!surl)
    return FALSE;

  /* Play radio */
  ret = melo_player_play (browser->player, surl, name, NULL, FALSE);

  /* Free objects */
  g_object_unref (parser);
  g_object_unref (stream);
  g_object_unref (msg);

  return ret;

bad_json:
  g_object_unref (parser);
bad_status:
  g_object_unref (stream);
bad_request:
  g_object_unref (msg);
  return FALSE;
}

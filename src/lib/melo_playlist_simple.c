/*
 * melo_playlist_simple.c: Simple Playlist
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

#include "melo_player.h"
#include "melo_playlist_simple.h"

#define MELO_PLAYLIST_SIMPLE_NAME_EXT_SIZE 10

static MeloPlaylistList *melo_playlist_simple_get_list (MeloPlaylist *playlist,
                                                    MeloTagsFields tags_fields);
static MeloTags *melo_playlist_simple_get_tags (MeloPlaylist *playlist,
                                                const gchar *name,
                                                MeloTagsFields fields);
static gboolean melo_playlist_simple_add (MeloPlaylist *playlist,
                                          const gchar *path, const gchar *name,
                                          MeloTags *tags, gboolean is_current);
static gchar *melo_playlist_simple_get_prev (MeloPlaylist *playlist,
                                             gchar **name, MeloTags **tags,
                                             gboolean set);
static gchar *melo_playlist_simple_get_next (MeloPlaylist *playlist,
                                             gchar **name, MeloTags **tags,
                                             gboolean set);
static gboolean melo_playlist_simple_has_prev (MeloPlaylist *playlist);
static gboolean melo_playlist_simple_has_next (MeloPlaylist *playlist);
static gboolean melo_playlist_simple_play (MeloPlaylist *playlist,
                                           const gchar *name);
static gboolean melo_playlist_simple_remove (MeloPlaylist *playlist,
                                             const gchar *name);
static void melo_playlist_simple_empty (MeloPlaylist *playlist);

static gboolean melo_playlist_simple_get_cover (MeloPlaylist *playlist,
                                                const gchar *path,
                                                GBytes **data, gchar **type);

static void melo_playlist_simple_set_property (GObject *object,
                                               guint property_id,
                                               const GValue *value,
                                                GParamSpec *pspec);
static void melo_playlist_simple_get_property (GObject *object,
                                               guint property_id, GValue *value,
                                               GParamSpec *pspec);

enum {
  PROP_0,
  PROP_PLAYABLE,
  PROP_REMOVABLE,
  PROP_OVERRIDE_COVER_URL,
  PROP_LAST
};

struct _MeloPlaylistSimplePrivate {
  GMutex mutex;
  GList *playlist;
  GHashTable *names;
  GList *current;
  gboolean playable;
  gboolean removable;
  gboolean override_cover_url;
};

G_DEFINE_TYPE_WITH_PRIVATE (MeloPlaylistSimple, melo_playlist_simple, MELO_TYPE_PLAYLIST)

static void
melo_playlist_simple_finalize (GObject *gobject)
{
  MeloPlaylistSimple *playlist_simple = MELO_PLAYLIST_SIMPLE (gobject);
  MeloPlaylistSimplePrivate *priv =
                    melo_playlist_simple_get_instance_private (playlist_simple);

  /* Clear mutex */
  g_mutex_clear (&priv->mutex);

  /* Free hash table */
  g_hash_table_remove_all (priv->names);
  g_hash_table_unref (priv->names);

  /* Free playlist */
  g_list_free_full (priv->playlist, (GDestroyNotify) melo_playlist_item_unref);

  /* Chain up to the parent class */
  G_OBJECT_CLASS (melo_playlist_simple_parent_class)->finalize (gobject);
}

static void
melo_playlist_simple_class_init (MeloPlaylistSimpleClass *klass)
{
  MeloPlaylistClass *plclass = MELO_PLAYLIST_CLASS (klass);
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  plclass->get_list = melo_playlist_simple_get_list;
  plclass->get_tags = melo_playlist_simple_get_tags;
  plclass->add = melo_playlist_simple_add;
  plclass->get_prev = melo_playlist_simple_get_prev;
  plclass->get_next = melo_playlist_simple_get_next;
  plclass->has_prev = melo_playlist_simple_has_prev;
  plclass->has_next = melo_playlist_simple_has_next;
  plclass->play = melo_playlist_simple_play;
  plclass->remove = melo_playlist_simple_remove;
  plclass->empty = melo_playlist_simple_empty;

  plclass->get_cover = melo_playlist_simple_get_cover;

  /* Add custom finalize() function */
  oclass->finalize = melo_playlist_simple_finalize;
  oclass->set_property = melo_playlist_simple_set_property;
  oclass->get_property = melo_playlist_simple_get_property;

  /* Install playable property */
  g_object_class_install_property (oclass, PROP_PLAYABLE,
      g_param_spec_boolean ("playable", "Playable",
                           "Playlist element can be played", FALSE,
                            G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
                            G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  /* Install removable property */
  g_object_class_install_property (oclass, PROP_REMOVABLE,
      g_param_spec_boolean ("removable", "Removable",
                           "Playlist element can be removed", FALSE,
                            G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
                            G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  /* Install override cover URL property */
  g_object_class_install_property (oclass, PROP_OVERRIDE_COVER_URL,
      g_param_spec_boolean ("override-cover-url", "Override cover URL",
                           "Override cover URL in MeloTags at add", FALSE,
                            G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
                            G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
}

static void
melo_playlist_simple_init (MeloPlaylistSimple *self)
{
  MeloPlaylistSimplePrivate *priv =
                               melo_playlist_simple_get_instance_private (self);

  self->priv = priv;
  priv->playlist = NULL;
  priv->current = NULL;

  /* Init mutex */
  g_mutex_init (&priv->mutex);

  /* Init Hash table for names */
  priv->names = g_hash_table_new (g_str_hash, g_str_equal);
}

static void
melo_playlist_simple_set_property (GObject *object, guint property_id,
                                   const GValue *value, GParamSpec *pspec)
{
  MeloPlaylistSimple *plsimple = MELO_PLAYLIST_SIMPLE (object);
  MeloPlaylistSimplePrivate *priv = plsimple->priv;

  switch (property_id) {
    case PROP_PLAYABLE:
      priv->playable = g_value_get_boolean (value);
      break;
    case PROP_REMOVABLE:
      priv->removable = g_value_get_boolean (value);
      break;
    case PROP_OVERRIDE_COVER_URL:
      priv->override_cover_url = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
melo_playlist_simple_get_property (GObject *object, guint property_id,
                                   GValue *value, GParamSpec *pspec)
{
  MeloPlaylistSimple *plsimple = MELO_PLAYLIST_SIMPLE (object);
  MeloPlaylistSimplePrivate *priv = plsimple->priv;

  switch (property_id) {
    case PROP_PLAYABLE:
      g_value_set_boolean (value, priv->playable);
      break;
    case PROP_REMOVABLE:
      g_value_set_boolean (value, priv->removable);
      break;
    case PROP_OVERRIDE_COVER_URL:
      g_value_set_boolean (value, priv->override_cover_url);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static MeloPlaylistList *
melo_playlist_simple_get_list (MeloPlaylist *playlist,
                               MeloTagsFields tags_fields)
{
  MeloPlaylistSimple *plsimple = MELO_PLAYLIST_SIMPLE (playlist);
  MeloPlaylistSimplePrivate *priv = plsimple->priv;
  MeloPlaylistList *list;

  /* Create new list */
  list = melo_playlist_list_new ();
  if (!list)
    return NULL;

  /* Lock playlist */
  g_mutex_lock (&priv->mutex);

  /* Copy playlist */
  list->items = g_list_copy_deep (priv->playlist,
                                  (GCopyFunc) melo_playlist_item_ref, NULL);
  if (priv->current)
    list->current = g_strdup (((MeloPlaylistItem *) priv->current->data)->name);

  /* Unlock playlist */
  g_mutex_unlock (&priv->mutex);

  return list;
}

static MeloTags *
melo_playlist_simple_get_tags (MeloPlaylist *playlist, const gchar *name,
                               MeloTagsFields fields)
{
  MeloPlaylistSimple *plsimple = MELO_PLAYLIST_SIMPLE (playlist);
  MeloPlaylistSimplePrivate *priv = plsimple->priv;
  MeloTags *tags = NULL;
  GList *element;

  /* Lock playlist */
  g_mutex_lock (&priv->mutex);

  /* Find media in hash table */
  element = g_hash_table_lookup (priv->names, name);
  if (element) {
    MeloPlaylistItem *item;

    /* Get tags from item */
    item = (MeloPlaylistItem *) element->data;
    if (item->tags)
      tags = melo_tags_ref (item->tags);
  }

  /* Unlock playlist */
  g_mutex_unlock (&priv->mutex);

  return tags;
}

static inline void
melo_playlist_simple_update_player_status (MeloPlaylistSimple *plsimple)
{
  MeloPlaylist *playlist = MELO_PLAYLIST (plsimple);
  MeloPlaylistSimplePrivate *priv = plsimple->priv;

  if (playlist->player)
    melo_player_set_status_playlist (playlist->player,
                                     priv->current && priv->current->next,
                                     priv->current && priv->current->prev);
}

static gboolean
melo_playlist_simple_add (MeloPlaylist *playlist, const gchar *path,
                          const gchar *name, MeloTags *tags,
                          gboolean is_current)
{
  MeloPlaylistSimple *plsimple = MELO_PLAYLIST_SIMPLE (playlist);
  MeloPlaylistSimplePrivate *priv = plsimple->priv;
  MeloPlaylistItem *item;
  gint len, i;
  gchar *final_name;

  /* Lock playlist */
  g_mutex_lock (&priv->mutex);

  /* Use path when name is not provided */
  if (!name)
    name = path;

  /* Generate a new name if current doesn't exists */
  len = strlen (name);
  final_name = g_strndup (name, len + MELO_PLAYLIST_SIMPLE_NAME_EXT_SIZE);
  for (i = 1; i > 0 && g_hash_table_lookup (priv->names, final_name); i++)
    g_snprintf (final_name + len, MELO_PLAYLIST_SIMPLE_NAME_EXT_SIZE, "_%d", i);
  if (i < 0) {
    g_mutex_unlock (&priv->mutex);
    g_free (final_name);
    return FALSE;
  }

  /* Add a new simple to playlist */
  item = melo_playlist_item_new (NULL, name, path, tags);
  item->name = final_name;
  item->can_play = priv->playable;
  item->can_remove = priv->removable;
  priv->playlist = g_list_prepend (priv->playlist, item);
  g_hash_table_insert (priv->names, final_name, priv->playlist);

  /* Use playlist cover URL if cover data are available */
  if (priv->override_cover_url && tags && melo_tags_has_cover (tags))
    melo_tags_set_cover_url (tags, G_OBJECT (playlist), final_name, NULL);

  /* Set as current */
  if (is_current)
    priv->current = priv->playlist;

  /* Update player status */
  melo_playlist_simple_update_player_status (plsimple);

  /* Unlock playlist */
  g_mutex_unlock (&priv->mutex);

  return TRUE;
}

static gchar *
melo_playlist_simple_get_prev (MeloPlaylist *playlist, gchar **name,
                               MeloTags **tags, gboolean set)
{
  MeloPlaylistSimple *plsimple = MELO_PLAYLIST_SIMPLE (playlist);
  MeloPlaylistSimplePrivate *priv = plsimple->priv;
  MeloPlaylistItem *item;
  gchar *path = NULL;

  /* Lock playlist */
  g_mutex_lock (&priv->mutex);

  /* Get next item after current */
  if (priv->current && priv->current->next) {
    item = (MeloPlaylistItem *) priv->current->next->data;
    path = g_strdup (item->path);
    if (name)
      *name = g_strdup (item->name);
    if (tags && item->tags)
      *tags = melo_tags_ref (item->tags);
    if (set) {
      priv->current = priv->current->next;
      melo_playlist_simple_update_player_status (plsimple);
    }
  }

  /* Unlock playlist */
  g_mutex_unlock (&priv->mutex);

  return path;
}

static gchar *
melo_playlist_simple_get_next (MeloPlaylist *playlist, gchar **name,
                               MeloTags **tags, gboolean set)
{
  MeloPlaylistSimple *plsimple = MELO_PLAYLIST_SIMPLE (playlist);
  MeloPlaylistSimplePrivate *priv = plsimple->priv;
  MeloPlaylistItem *item;
  gchar *path = NULL;

  /* Lock playlist */
  g_mutex_lock (&priv->mutex);

  /* Get previous item before current */
  if (priv->current && priv->current->prev) {
    item = (MeloPlaylistItem *) priv->current->prev->data;
    path = g_strdup (item->path);
    if (name)
      *name = g_strdup (item->name);
    if (tags && item->tags)
      *tags = melo_tags_ref (item->tags);
    if (set) {
      priv->current = priv->current->prev;
      melo_playlist_simple_update_player_status (plsimple);
    }
  }

  /* Unlock playlist */
  g_mutex_unlock (&priv->mutex);

  return path;
}

static gboolean
melo_playlist_simple_has_prev (MeloPlaylist *playlist)
{
  MeloPlaylistSimple *plsimple = MELO_PLAYLIST_SIMPLE (playlist);
  MeloPlaylistSimplePrivate *priv = plsimple->priv;
  gboolean val;

  /* Lock playlist */
  g_mutex_lock (&priv->mutex);

  /* Have anitem after current */
  val = priv->current && priv->current->next;

  /* Unlock playlist */
  g_mutex_unlock (&priv->mutex);

  return val;
}

static gboolean
melo_playlist_simple_has_next (MeloPlaylist *playlist)
{
  MeloPlaylistSimple *plsimple = MELO_PLAYLIST_SIMPLE (playlist);
  MeloPlaylistSimplePrivate *priv = plsimple->priv;
  gboolean val;

  /* Lock playlist */
  g_mutex_lock (&priv->mutex);

  /* Have anitem before current */
  val = priv->current && priv->current->prev;

  /* Unlock playlist */
  g_mutex_unlock (&priv->mutex);

  return val;
}

static gboolean
melo_playlist_simple_play (MeloPlaylist *playlist, const gchar *name)
{
  MeloPlaylistSimple *plsimple = MELO_PLAYLIST_SIMPLE (playlist);
  MeloPlaylistSimplePrivate *priv = plsimple->priv;
  MeloPlaylistItem *item = NULL;
  GList *element;

  /* Cannot be played */
  if (!priv->playable)
    return FALSE;

  /* Lock playlist */
  g_mutex_lock (&priv->mutex);

  /* Find media in hash table */
  element = g_hash_table_lookup (priv->names, name);
  if (element) {
    item = (MeloPlaylistItem *) element->data;
    melo_playlist_item_ref (item);
    priv->current = element;
  }

  /* Update player status */
  melo_playlist_simple_update_player_status (plsimple);

  /* Unlock playlist */
  g_mutex_unlock (&priv->mutex);

  /* No item found */
  if (!item)
    return FALSE;

  /* Play media if player is available */
  if (playlist->player)
    melo_player_play (playlist->player, item->path, item->name, item->tags,
                      FALSE);
  melo_playlist_item_unref (item);

  return TRUE;
}

static gboolean
melo_playlist_simple_remove (MeloPlaylist *playlist, const gchar *name)
{
  MeloPlaylistSimple *plsimple = MELO_PLAYLIST_SIMPLE (playlist);
  MeloPlaylistSimplePrivate *priv = plsimple->priv;
  MeloPlaylistItem *item;
  GList *element;

  /* Cannot be removed */
  if (!priv->removable)
    return FALSE;

  /* Lock playlist */
  g_mutex_lock (&priv->mutex);

  /* Find media in hash table */
  element = g_hash_table_lookup (priv->names, name);
  if (!element) {
    g_mutex_unlock (&priv->mutex);
    return FALSE;
  }
  item = (MeloPlaylistItem *) element->data;

  /* Stop play */
  if (element == priv->current) {
    melo_player_set_state (playlist->player, MELO_PLAYER_STATE_NONE);
    priv->current = NULL;
  }

  /* Remove from list and hash table */
  priv->playlist = g_list_remove (priv->playlist, item);
  g_hash_table_remove (priv->names, name);
  melo_playlist_item_unref (item);

  /* Update player status */
  melo_playlist_simple_update_player_status (plsimple);

  /* Unlock playlist */
  g_mutex_unlock (&priv->mutex);

  return TRUE;
}

static void
melo_playlist_simple_empty (MeloPlaylist *playlist)
{
  MeloPlaylistSimple *plsimple = MELO_PLAYLIST_SIMPLE (playlist);
  MeloPlaylistSimplePrivate *priv = plsimple->priv;

  /* Lock playlist */
  g_mutex_lock (&priv->mutex);

  /* Remove and free all items */
  g_list_free_full (priv->playlist, (GDestroyNotify) melo_playlist_item_unref);
  g_hash_table_remove_all (priv->names);
  priv->playlist = NULL;

  /* Update player status */
  melo_playlist_simple_update_player_status (plsimple);

  /* Unlock playlist */
  g_mutex_unlock (&priv->mutex);
}

static gboolean
melo_playlist_simple_get_cover (MeloPlaylist *playlist, const gchar *path,
                                GBytes **data, gchar **type)
{
  MeloPlaylistSimple *plsimple = MELO_PLAYLIST_SIMPLE (playlist);
  MeloPlaylistSimplePrivate *priv = plsimple->priv;
  MeloPlaylistItem *item;
  GList *element;

  /* Lock playlist */
  g_mutex_lock (&priv->mutex);

  /* Find media in hash table */
  element = g_hash_table_lookup (priv->names, path);
  if (!element) {
    g_mutex_unlock (&priv->mutex);
    return FALSE;
  }

  /* Get tags from item */
  item = (MeloPlaylistItem *) element->data;
  if (item->tags)
    *data = melo_tags_get_cover (item->tags, type);

  /* Unlock playlist */
  g_mutex_unlock (&priv->mutex);

  return TRUE;
}

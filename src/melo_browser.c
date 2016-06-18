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

struct _MeloBrowserPrivate {
  gchar *id;
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (MeloBrowser, melo_browser, G_TYPE_OBJECT)

static void
melo_browser_finalize (GObject *gobject)
{
  MeloBrowserPrivate *priv = melo_browser_get_instance_private (
                                                        MELO_BROWSER (gobject));

  if (priv->id);
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

MeloBrowser *
melo_browser_new (GType type, const gchar *id)
{
  MeloBrowser *bro;

  g_return_val_if_fail (id, NULL);
  g_return_val_if_fail (g_type_is_a (type, MELO_TYPE_BROWSER), NULL);

  /* Create a new instance of browser */
  bro = g_object_new (type, NULL);
  if (!bro)
    return NULL;

  /* Set ID */
  melo_browser_set_id (bro, id);

  return bro;
}

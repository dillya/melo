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

#include "melo_browser_radio.h"

/* Radio browser info */
static MeloBrowserInfo melo_browser_radio_info = {
  .name = "Browse radios",
  .description = "Navigate though more than 30,000 radio and webradio",
};

static const MeloBrowserInfo *melo_browser_radio_get_info (
                                                          MeloBrowser *browser);
static GList *melo_browser_radio_get_list (MeloBrowser *browser,
                                           const gchar *path);
static gboolean melo_browser_radio_play (MeloBrowser *browser,
                                         const gchar *path);

struct _MeloBrowserRadioPrivate {
  GMutex mutex;
};

G_DEFINE_TYPE_WITH_PRIVATE (MeloBrowserRadio, melo_browser_radio, MELO_TYPE_BROWSER)

static void
melo_browser_radio_finalize (GObject *gobject)
{
  MeloBrowserRadio *browser_radio = MELO_BROWSER_RADIO (gobject);
  MeloBrowserRadioPrivate *priv =
                        melo_browser_radio_get_instance_private (browser_radio);

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
}

static const MeloBrowserInfo *
melo_browser_radio_get_info (MeloBrowser *browser)
{
  return &melo_browser_radio_info;
}

static GList *
melo_browser_radio_get_list (MeloBrowser *browser, const gchar *path)
{
  return NULL;
}

static gboolean
melo_browser_radio_play (MeloBrowser *browser, const gchar *path)
{
  return FALSE;
}

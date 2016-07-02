/*
 * melo_file.c: File module for local / remote file playing
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

#include "melo_file.h"
#include "melo_browser_file.h"
#include "melo_player_file.h"

/* Module file info */
static MeloModuleInfo melo_file_info = {
  .name = "Files",
  .description = "Navigate and play any of your music files",
};

static const MeloModuleInfo *melo_file_get_info (MeloModule *module);

struct _MeloFilePrivate {
  MeloBrowser *files;
  MeloPlayer *player;
};

G_DEFINE_TYPE_WITH_PRIVATE (MeloFile, melo_file, MELO_TYPE_MODULE)

static void
melo_file_finalize (GObject *gobject)
{
  MeloFilePrivate *priv = melo_file_get_instance_private (MELO_FILE (gobject));

  if (priv->player) {
    melo_module_unregister_player (MELO_MODULE (gobject), "file_player");
    g_object_unref (priv->player);
  }

  if (priv->files) {
    melo_module_unregister_browser (MELO_MODULE (gobject), "file_files");
    g_object_unref (priv->files);
  }

  /* Chain up to the parent class */
  G_OBJECT_CLASS (melo_file_parent_class)->finalize (gobject);
}


static void
melo_file_class_init (MeloFileClass *klass)
{
  MeloModuleClass *mclass = MELO_MODULE_CLASS (klass);
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  mclass->get_info = melo_file_get_info;

  /* Add custom finalize() function */
  oclass->finalize = melo_file_finalize;
}

static void
melo_file_init (MeloFile *self)
{
  MeloFilePrivate *priv = melo_file_get_instance_private (self);

  self->priv = priv;
  priv->files = melo_browser_new (MELO_TYPE_BROWSER_FILE, "file_files");
  priv->player = melo_player_new (MELO_TYPE_PLAYER_FILE, "file_player");

  if (priv->files)
    melo_module_register_browser (MELO_MODULE (self), priv->files);
  if (priv->player) {
    melo_module_register_player (MELO_MODULE (self), priv->player);
    if (priv->files)
      melo_browser_set_player (MELO_BROWSER (priv->files), priv->player);
  }
}

static const MeloModuleInfo *
melo_file_get_info (MeloModule *module)
{
  return &melo_file_info;
}

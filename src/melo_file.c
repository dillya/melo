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

/* Module file info */
MeloModuleInfo melo_file_info = {
  .name = "Files",
  .description = "Navigate and play any of your music files",
};

static const MeloModuleInfo *melo_file_get_info (MeloModule *module);

G_DEFINE_TYPE (MeloFile, melo_file, MELO_TYPE_MODULE)

static void
melo_file_class_init (MeloFileClass *klass)
{
  MeloModuleClass *mclass = (MeloModuleClass *) klass;

  mclass->get_info = melo_file_get_info;
}

static void
melo_file_init (MeloFile *self)
{
}

static const MeloModuleInfo *
melo_file_get_info (MeloModule *module)
{
  return &melo_file_info;
}

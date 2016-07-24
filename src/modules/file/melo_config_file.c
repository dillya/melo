/*
 * melo_config_file.c: File module configuration
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

#include "melo_config_file.h"

static MeloConfigItem melo_config_global[] = {
  {
    .id = "local_path",
    .name = "Local path",
    .type = MELO_CONFIG_TYPE_STRING,
    .element = MELO_CONFIG_ELEMENT_TEXT,
    .def._string = "",
    .flags = MELO_CONFIG_FLAGS_READ_ONLY,
  },
};

static MeloConfigGroup melo_config_file[] = {
  {
    .id = "global",
    .name = "Global",
    .items = melo_config_global,
    .items_count = G_N_ELEMENTS (melo_config_global),
  }
};

MeloConfig *
melo_config_file_new (void)
{
  return melo_config_new ("file", melo_config_file,
                          G_N_ELEMENTS (melo_config_file));
}

/*
 * melo_config_main.c: Main configuration for Melo
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

#include "melo_config_main.h"

static MeloConfigItem melo_config_http[] = {
  {
    .id = NULL,
    .name = "Main",
  },
  {
    .id = "port",
    .name = "TCP port",
    .type = MELO_CONFIG_TYPE_INTEGER,
    .element = MELO_CONFIG_ELEMENT_NUMBER,
    .def._integer = 8080,
  },
};

static MeloConfigGroup melo_config_main[] = {
  {
    .id = "http",
    .name = "HTTP Server",
    .items = melo_config_http,
    .items_count = G_N_ELEMENTS (melo_config_http),
  }
};

MeloConfig *
melo_config_main_new (void)
{
  return melo_config_new ("main", melo_config_main,
                          G_N_ELEMENTS (melo_config_main));
}

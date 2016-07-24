/*
 * melo_config_airplay.c: Airplay module configuration
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

#include "melo_airplay.h"
#include "melo_config_airplay.h"

static MeloConfigItem melo_config_general[] = {
  {
    .id = "name",
    .name = "Device name",
    .type = MELO_CONFIG_TYPE_STRING,
    .element = MELO_CONFIG_ELEMENT_TEXT,
    .def._string = "Melo",
  },
  {
    .id = "port",
    .name = "RTSP port",
    .type = MELO_CONFIG_TYPE_INTEGER,
    .element = MELO_CONFIG_ELEMENT_NUMBER,
    .def._integer = 5000,
  },
};

static MeloConfigGroup melo_config_airplay[] = {
  {
    .id = "general",
    .name = "General",
    .items = melo_config_general,
    .items_count = G_N_ELEMENTS (melo_config_general),
  }
};

MeloConfig *
melo_config_airplay_new (void)
{
  return melo_config_new ("airplay", melo_config_airplay,
                          G_N_ELEMENTS (melo_config_airplay));
}

void
melo_config_airplay_update (MeloConfigContext *context, gpointer user_data)
{
  MeloAirplay *air = MELO_AIRPLAY (user_data);
  const gchar *old, *new;
  gint64 port, old_port;

  /* Update name */
  if (melo_config_get_updated_string (context, "name", &new, &old) &&
      g_strcmp0 (new, old))
    melo_airplay_set_name (air, new);

  /* Update port */
  if (melo_config_get_updated_integer (context, "port", &port, &old_port) &&
      port != old_port)
    melo_airplay_set_port (air, port);
}

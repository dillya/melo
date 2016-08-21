/*
 * melo_config_upnp.c: UPnP / DLNA renderer module configuration
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

#include "melo_upnp.h"
#include "melo_config_upnp.h"

static MeloConfigItem melo_config_general[] = {
  {
    .id = "name",
    .name = "Device name",
    .type = MELO_CONFIG_TYPE_STRING,
    .element = MELO_CONFIG_ELEMENT_TEXT,
    .def._string = "Melo",
  },
};

static MeloConfigGroup melo_config_upnp[] = {
  {
    .id = "general",
    .name = "General",
    .items = melo_config_general,
    .items_count = G_N_ELEMENTS (melo_config_general),
  },
};

MeloConfig *
melo_config_upnp_new (void)
{
  return melo_config_new ("upnp", melo_config_upnp,
                          G_N_ELEMENTS (melo_config_upnp));
}

void
melo_config_upnp_update (MeloConfigContext *context, gpointer user_data)
{
  MeloUpnp *up = MELO_UPNP (user_data);
  const gchar *old, *new;
  gint64 port, old_port;

  /* Update name */
  if (melo_config_get_updated_string (context, "name", &new, &old) &&
      g_strcmp0 (new, old))
    melo_upnp_set_name (up, new);
}

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

static MeloConfigItem melo_config_global[] = {
  {
    .id = "name",
    .name = "Name",
    .type = MELO_CONFIG_TYPE_STRING,
    .element = MELO_CONFIG_ELEMENT_TEXT,
    .def._string = "Melo",
  },
};

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
  {
    .id = NULL,
    .name = "Authentication",
  },
  {
    .id = "auth_enable",
    .name = "Enable",
    .type = MELO_CONFIG_TYPE_BOOLEAN,
    .element = MELO_CONFIG_ELEMENT_CHECKBOX,
  },
  {
    .id = "auth_username",
    .name = "User name",
    .type = MELO_CONFIG_TYPE_STRING,
    .element = MELO_CONFIG_ELEMENT_TEXT,
  },
  {
    .id = "auth_password_old",
    .name = "Old password",
    .type = MELO_CONFIG_TYPE_STRING,
    .element = MELO_CONFIG_ELEMENT_PASSWORD,
    .flags = MELO_CONFIG_FLAGS_DONT_SAVE | MELO_CONFIG_FLAGS_WRITE_ONLY,
  },
  {
    .id = "auth_password",
    .name = "New password",
    .type = MELO_CONFIG_TYPE_STRING,
    .element = MELO_CONFIG_ELEMENT_PASSWORD,
    .flags = MELO_CONFIG_FLAGS_WRITE_ONLY,
  },
  {
    .id = "auth_password_new",
    .name = "New password (again)",
    .type = MELO_CONFIG_TYPE_STRING,
    .element = MELO_CONFIG_ELEMENT_PASSWORD,
    .flags = MELO_CONFIG_FLAGS_DONT_SAVE | MELO_CONFIG_FLAGS_WRITE_ONLY,
  },
};

static MeloConfigGroup melo_config_main[] = {
  {
    .id = "global",
    .name = "Global",
    .items = melo_config_global,
    .items_count = G_N_ELEMENTS (melo_config_global),
  },
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

/* HTTP server section */
void
melo_config_main_load_http (MeloConfig *config, MeloHTTPD *server)
{
  gchar *user = NULL;
  gchar *pass = NULL;
  gboolean en;

  /* Enable authentication */
  if (melo_config_get_boolean (config, "http", "auth_enable", &en)) {
    if (en)
      melo_httpd_auth_enable (server);
    else
      melo_httpd_auth_disable (server);
  }

  /* Set authentication logins */
  if (melo_config_get_string (config, "http", "auth_username", &user))
    melo_httpd_auth_set_username (server, user);
  if (melo_config_get_string (config, "http", "auth_password", &pass))
    melo_httpd_auth_set_password (server, pass);
  g_free (user);
  g_free (pass);
}

gboolean
melo_config_main_check_http (MeloConfigContext *context, gpointer user_data)
{
  MeloHTTPD *server = user_data;
  const gchar *pass_old = NULL;
  const gchar *pass_new = NULL;
  const gchar *pass = NULL;
  gboolean ret = FALSE;
  gchar *pass_cur;

  /* Get updated passwords */
  ret = melo_config_get_updated_string (context, "auth_password_old",
                                        &pass_old, NULL);
  ret |= melo_config_get_updated_string (context, "auth_password_new",
                                         &pass_new, NULL);
  ret |= melo_config_get_updated_string (context, "auth_password",
                                         &pass, NULL);
  if (ret) {
    /* Check password */
    if (pass_old && *pass_old == '\0')
      pass_old = NULL;
    if (pass_new && *pass_new == '\0')
      pass_new = NULL;
    if (pass && *pass == '\0') {
      melo_config_update_string (context, NULL);
      pass = NULL;
    }

    /* Don't change if all are NULL */
    if (pass_old || pass || pass_new) {
      /* Get current password */
      pass_cur = melo_httpd_auth_get_password (server);
      if ((pass_cur && g_strcmp0 (pass_old, pass_cur)) ||
          g_strcmp0 (pass, pass_new)) {
        g_free (pass_cur);
        return FALSE;
      }
      g_free (pass_cur);
    } else
      melo_config_remove_update (context);
  }

  return TRUE;
}

void
melo_config_main_http_update (MeloConfigContext *context, gpointer user_data)
{
  MeloHTTPD *server = user_data;
  const gchar *new, *old;
  gboolean en;

  /* Enable / Disable authentication */
  if (melo_config_get_updated_boolean (context, "auth_enable", &en, NULL)) {
    if (en)
      melo_httpd_auth_enable (server);
    else
      melo_httpd_auth_disable (server);
  }

  /* Update user name */
  if (melo_config_get_updated_string (context, "auth_username", &new, &old) &&
      g_strcmp0 (new, old))
    melo_httpd_auth_set_username (server, new);

  /* Update password */
  if (melo_config_get_updated_string (context, "auth_password", &new, &old) &&
      g_strcmp0 (new, old))
    melo_httpd_auth_set_password (server, new);
}

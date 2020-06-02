/*
 * Copyright (C) 2020 Alexandre Dilly <dillya@sparod.com>
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA.
 */

#include <melo/melo_settings.h>

#define MELO_LOG_TAG "melo_settings"
#include <melo/melo_log.h>

#include "discover.h"
#include "settings.h"

/* Settings */
static MeloSettings *settings;
static MeloSettingsEntry *entry_name;
static MeloSettingsEntry *entry_discover;
static MeloSettingsEntry *entry_http_port;
static MeloSettingsEntry *entry_https_port;

/* Bind */
static MeloHttpServer *http_server;

static bool
name_cb (MeloSettings *settings, MeloSettingsGroup *group, char **error,
    void *user_data)
{
  /* Update discover name */
  if (settings_is_discover ()) {
    unsigned int http_port, https_port;

    /* Get ports */
    settings_get_http_ports (&http_port, &https_port);

    /* Register device */
    discover_register_device (settings_get_name (), http_port, https_port);
  }

  return true;
}

static bool
discover_cb (MeloSettings *settings, MeloSettingsGroup *group, char **error,
    void *user_data)
{
  bool disco, old_disco;

  /* Update discover status */
  if (melo_settings_group_get_boolean (group, "sparod", &disco, &old_disco) &&
      disco != old_disco) {
    if (disco) {
      unsigned int http_port, https_port;

      /* Get ports */
      settings_get_http_ports (&http_port, &https_port);

      /* Register device */
      discover_register_device (settings_get_name (), http_port, https_port);
    } else
      discover_unregister_device ();
  }

  return true;
}

static bool
auth_cb (MeloSettings *settings, MeloSettingsGroup *group, char **error,
    void *user_data)
{
  *error = g_strdup ("Not yet supported");
  return false;
}

static bool
http_server_cb (MeloSettings *settings, MeloSettingsGroup *group, char **error,
    void *user_data)
{
  unsigned int http_port, https_port;

  /* Nothing to update */
  if (!http_server)
    return true;

  /* Stop HTTP server */
  melo_http_server_stop (http_server);

  /* Get ports */
  settings_get_http_ports (&http_port, &https_port);

  /* Start HTTP server */
  if (!melo_http_server_start (http_server, http_port, https_port))
    MELO_LOGE ("failed to restart HTTP server");

  return true;
}

/**
 * settings_init:
 *
 * Initialize and load global settings.
 */
void
settings_init (void)
{
  MeloSettingsGroup *group;
  MeloSettingsEntry *entry;

  /* Create global settings */
  settings = melo_settings_new ("global");
  if (!settings) {
    MELO_LOGE ("failed to create global settings");
    return;
  }

  /* Create name group */
  group = melo_settings_add_group (
      settings, "name", "Name", "Device name", name_cb, NULL);
  entry_name = melo_settings_group_add_string (group, "name", "Name",
      "Device name", "Melo", NULL, MELO_SETTINGS_FLAG_NONE);

  /* Create discover group */
  group = melo_settings_add_group (settings, "disco", "Discover",
      "Find Melo on https://www.sparod.com/melo", discover_cb, NULL);
  entry_discover = melo_settings_group_add_boolean (group, "sparod",
      "Enable on Sparod", "", true, NULL, MELO_SETTINGS_FLAG_NONE);
  melo_settings_group_add_boolean (group, "local", "Enable on local network",
      "", true, NULL, MELO_SETTINGS_FLAG_READ_ONLY);

  /* Create authentication group */
  group = melo_settings_add_group (settings, "auth", "Authentication",
      "Set a username / password to protect your device", auth_cb, NULL);
  entry = melo_settings_group_add_boolean (
      group, "en", "Enable", "", false, NULL, MELO_SETTINGS_FLAG_READ_ONLY);
  melo_settings_group_add_string (group, "user", "User name", "", "melo", entry,
      MELO_SETTINGS_FLAG_NONE | MELO_SETTINGS_FLAG_READ_ONLY);
  melo_settings_group_add_string (group, "pass", "Current password", "", NULL,
      entry, MELO_SETTINGS_FLAG_PASSWORD | MELO_SETTINGS_FLAG_READ_ONLY);
  melo_settings_group_add_string (group, "new_pass", "New password", "", NULL,
      entry, MELO_SETTINGS_FLAG_PASSWORD | MELO_SETTINGS_FLAG_READ_ONLY);
  melo_settings_group_add_string (group, "conf_pass", "New password (confirm)",
      "", NULL, entry,
      MELO_SETTINGS_FLAG_PASSWORD | MELO_SETTINGS_FLAG_READ_ONLY);

  /* Create HTTP server group */
  group = melo_settings_add_group (settings, "http_server", "HTTP server",
      "Set HTTP server settings such as ports", http_server_cb, NULL);
  entry_http_port = melo_settings_group_add_uint32 (
      group, "http_port", "HTTP port", "", 8080, NULL, MELO_SETTINGS_FLAG_NONE);
  entry_https_port = melo_settings_group_add_uint32 (group, "https_port",
      "HTTPs port", "", 8443, NULL, MELO_SETTINGS_FLAG_NONE);

  /* Load settings */
  melo_settings_load (settings);
}

/**
 * settings_deinit:
 *
 * Clean and release global settings.
 */
void
settings_deinit (void)
{
  if (settings)
    g_object_unref (settings);
  settings = NULL;
}

/**
 * settings_get_name:
 *
 * Get the current device name for discovering.
 *
 * Returns: the current device name.
 */
const char *
settings_get_name (void)
{
  const char *value;

  if (!melo_settings_entry_get_string (entry_name, &value, NULL))
    return false;

  return value;
}

/**
 * settings_is_discover:
 *
 * Returns: %true if the discovering on Sparod is enabled, %false otherwise.
 */
bool
settings_is_discover (void)
{
  bool value;

  if (!melo_settings_entry_get_boolean (entry_discover, &value, NULL))
    return false;

  return value;
}

/**
 * settings_bind_http_server:
 * @server: the #MeloHttpServer to bind
 *
 * Bind the @server instance to settings for updating ports and authentication.
 */
void
settings_bind_http_server (MeloHttpServer *server)
{
  http_server = server;
}

/**
 * settings_get_http_ports:
 * @http_port: the HTTP port
 * @https_port: the HTTPS port
 *
 * Get the current HTTP / HTTPs ports to use.
 */
void
settings_get_http_ports (unsigned int *http_port, unsigned int *https_port)
{
  melo_settings_entry_get_uint32 (entry_http_port, http_port, NULL);
  melo_settings_entry_get_uint32 (entry_https_port, https_port, NULL);
}

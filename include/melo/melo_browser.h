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

#ifndef _MELO_BROWSER_H_
#define _MELO_BROWSER_H_

#include <glib-object.h>

#include <melo/melo_async.h>
#include <melo/melo_request.h>

G_BEGIN_DECLS

/**
 * MeloBrowser:
 */

#define MELO_TYPE_BROWSER melo_browser_get_type ()
G_DECLARE_DERIVABLE_TYPE (MeloBrowser, melo_browser, MELO, BROWSER, GObject)

/**
 * MeloBrowserClass:
 * @parent_class: #GObject parent class
 * @handle_request: This function is called when a new request is received and
 *     should be handled by the #MeloBrowser instance
 * @get_asset: This function is called to get an URI for a specific asset
 *     identified by its ID
 *
 * The class structure for a #MeloBrowser object.
 */
struct _MeloBrowserClass {
  GObjectClass parent_class;

  bool (*handle_request) (
      MeloBrowser *browser, const MeloMessage *msg, MeloRequest *req);
  char *(*get_asset) (MeloBrowser *browser, const char *id);
};

/**
 * MELO_DECLARE_BROWSER:
 * @TN: the type name as MeloCustomNameBrowser
 * @t_n: the type name as melo_custom_name_browser
 * @ON: the object name as CUSTOM_NAME_BROWSER
 *
 * An helper to declare a browser, to use in the header source file as (for a
 * class named "custom"):
 * |[<!-- language="C" -->
 * struct _MeloCustomBrowser {
 *   GObject parent_instance;
 * };
 *
 * MELO_DEFINE_BROWSER (MeloCustomBrowser, melo_custom_browser)
 * ]|
 */
#define MELO_DECLARE_BROWSER(TN, t_n, ON) \
  G_DECLARE_FINAL_TYPE (TN, t_n, MELO, ON, MeloBrowser)

/**
 * MELO_DEFINE_BROWSER:
 * @TN: the type name as MeloCustomNameBrowser
 * @t_n: the type name as melo_custom_name_browser
 *
 * An helper to define a browser, to use in the C source file as (for a class
 * named "custom"):
 * |[<!-- language="C" -->
 * #define MELO_TYPE_CUSTOM_BROWSER melo_custom_browser_get_type ()
 * MELO_DECLARE_BROWSER (MeloCustomBrowser, melo_custom_browser, CUSTOM_BROWSER)
 * ]|
 */
#define MELO_DEFINE_BROWSER(TN, t_n) G_DEFINE_TYPE (TN, t_n, MELO_TYPE_BROWSER)

bool melo_browser_add_event_listener (
    const char *id, MeloAsyncCb cb, void *user_data);
bool melo_browser_remove_event_listener (
    const char *id, MeloAsyncCb cb, void *user_data);

bool melo_browser_handle_request (
    const char *id, MeloMessage *msg, MeloAsyncCb cb, void *user_data);
void melo_browser_cancel_request (
    const char *id, MeloAsyncCb cb, void *user_data);

char *melo_browser_get_asset (const char *id, const char *asset);

G_END_DECLS

#endif /* !_MELO_BROWSER_H_ */

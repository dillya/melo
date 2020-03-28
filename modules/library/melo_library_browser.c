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

#define MELO_LOG_TAG "library_browser"
#include <melo/melo_log.h>

#include "melo_library_browser.h"

struct _MeloLibraryBrowser {
  GObject parent_instance;
};

MELO_DEFINE_BROWSER (MeloLibraryBrowser, melo_library_browser)

static void
melo_library_browser_class_init (MeloLibraryBrowserClass *klass)
{
}

static void
melo_library_browser_init (MeloLibraryBrowser *self)
{
}

MeloLibraryBrowser *
melo_library_browser_new ()
{
  return g_object_new (MELO_TYPE_LIBRARY_BROWSER, "id", MELO_LIBRARY_BROWSER_ID,
      "name", "Library", "description", "Navigate through all your medias",
      "icon", "fa:music", NULL);
}

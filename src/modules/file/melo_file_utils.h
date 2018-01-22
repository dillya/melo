/*
 * melo_file_utils.h: Utils for File module
 *
 * Copyright (C) 2018 Alexandre Dilly <dillya@sparod.com>
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

#ifndef __MELO_FILE_UTILS_H__
#define __MELO_FILE_UTILS_H__

#include <glib.h>
#include <gio/gio.h>

/* Check if a file is accessible and mount the associated volume if necessary */
gboolean melo_file_utils_check_and_mount_file (GFile *file,
                                               GCancellable *cancellable,
                                               GError **error);
gboolean melo_file_utils_check_and_mount_uri (const gchar *uri,
                                              GCancellable *cancellable,
                                              GError **error);

#endif /* __MELO_FILE_UTILS_H__ */

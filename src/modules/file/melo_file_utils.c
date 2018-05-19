/*
 * melo_file_utils.c: Utils for File module
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

//#include <stdlib.h>
//#include <string.h>

#include "melo_file_utils.h"

typedef struct {
  GMutex mutex;
  GError *error;
} MountAsyncData;

static void
mount_async_done (GObject *obj, GAsyncResult *result, gpointer user_data)
{
  MountAsyncData *m = user_data;

  /* Finalize mount operation */
  g_file_mount_enclosing_volume_finish (G_FILE (obj), result, &m->error);

  /* Signal end of mount */
  g_mutex_unlock (&m->mutex);
}

static void
mount_ask_password (GMountOperation *op, const char *message,
                    const char *default_user, const char *default_domain,
                    GAskPasswordFlags flags)
{
  /* We have already tried to connect anonymously */
  if (g_mount_operation_get_anonymous (op)) {
    g_mount_operation_reply (op, G_MOUNT_OPERATION_ABORTED);
    return;
  }

  /* Try to connect anonymously */
  g_mount_operation_set_anonymous (op, TRUE);
  g_mount_operation_reply (op, G_MOUNT_OPERATION_HANDLED);
}

gboolean
melo_file_utils_check_and_mount_file (GFile *file, GCancellable *cancellable,
                                      GError **error)
{
  GError *err = NULL;
  GFileInfo *info;

  /* Mount volume if file info cannot be get */
  info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_TYPE, 0,
                            cancellable, &err);
  if (!info && err->code != G_IO_ERROR_NOT_FOUND) {
    MountAsyncData m = { .error = NULL, };
    GMountOperation *op;

    /* Create mount operation for authentication */
    op = g_mount_operation_new ();
    g_signal_connect (op, "ask_password", G_CALLBACK (mount_ask_password),
                      NULL);

    /* Mount */
    g_mutex_init (&m.mutex);
    g_mutex_lock (&m.mutex);
    g_file_mount_enclosing_volume (file, 0, op, cancellable, mount_async_done,
                                   &m);

    /* Wait end of operation */
    g_mutex_lock (&m.mutex);
    g_clear_error (&err);
    g_mutex_unlock (&m.mutex);
    g_mutex_clear (&m.mutex);

    /* Free mount operation handler */
    g_object_unref (op);

    /* Mpunt failed */
    if (m.error) {
      g_clear_error (&m.error);
      return FALSE;
    }
  } else
    g_object_unref (info);

  return TRUE;
}

gboolean
melo_file_utils_check_and_mount_uri (const gchar *uri,
                                     GCancellable *cancellable, GError **error)
{
  gboolean ret;
  GFile *file;

  /* Create a new file from URI */
  file = g_file_new_for_uri (uri);
  if (!file)
    return FALSE;

  /* Check and mount file */
  ret = melo_file_utils_check_and_mount_file (file, cancellable, error);
  g_object_unref (file);

  return ret;
}

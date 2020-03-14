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

#include <errno.h>

#include <glib/gstdio.h>

#define MELO_LOG_TAG "http_server_file"
#include "melo/melo_log.h"

#include "melo_http_server_file.h"

static const char *
melo_http_server_get_content_type (const char *path)
{
  static const struct {
    const char *ext;
    const char *type;
  } types[] = {
      {"css", "text/css"},
      {"gif", "image/gif"},
      {"htm", "text/html"},
      {"html", "text/html"},
      {"ico", "image/x-icon"},
      {"jpeg", "image/jpeg"},
      {"jpg", "image/jpeg"},
      {"js", "application/javascript"},
      {"json", "application/json"},
      {"png", "image/png"},
      {"svg", "image/svg+xml"},
      {"tif", "image/tiff"},
      {"tiff", "image/tiff"},
      {"ts", "application/typescript"},
      {"ttf", "font/ttf"},
      {"webp", "image/webp"},
      {"woff", "font/woff"},
      {"woff2", "font/woff2"},
      {"xml", "application/xml"},
  };
  const char *suffix;

  /* Find file extension */
  suffix = strrchr (path, '.');
  if (suffix++) {
    unsigned int i;

    /* Find MIME type */
    for (i = 0; i < sizeof (types) / sizeof (*types); i++)
      if (!strcmp (types[i].ext, suffix))
        return types[i].type;
  }

  return NULL;
}

void
melo_http_server_file_handler (SoupServer *server, SoupMessage *msg,
    const char *path, GHashTable *query, SoupClientContext *client,
    gpointer user_data)
{
  const gchar *root_path = user_data;
  gchar *file_path;
  size_t len;

  /* We only support GET and HEAD methods */
  if (msg->method != SOUP_METHOD_GET && msg->method != SOUP_METHOD_HEAD) {
    soup_message_set_status (msg, SOUP_STATUS_NOT_IMPLEMENTED);
    return;
  }

  /* Use default file */
  len = strlen (path);
  if (path[len - 1] == '/')
    path = "index.html";

  /* Generate absolute path in file system */
  file_path = g_build_path (G_DIR_SEPARATOR_S, root_path, path, NULL);
  if (!file_path) {
    soup_message_set_status (msg, SOUP_STATUS_INTERNAL_SERVER_ERROR);
    return;
  }

  /* Serve file */
  melo_http_server_file_serve (msg, client, file_path);

  /* Free absolute path */
  g_free (file_path);
}

void
melo_http_server_file_serve (
    SoupMessage *msg, SoupClientContext *client, const char *path)
{
  guint status = SOUP_STATUS_INTERNAL_SERVER_ERROR;
  GStatBuf st;

  /* Check file status */
  if (g_stat (path, &st) == -1) {
    if (errno == EPERM)
      /* No permission to read */
      status = SOUP_STATUS_FORBIDDEN;
    else if (errno == ENOENT)
      /* No file or directory */
      status = SOUP_STATUS_NOT_FOUND;
    goto end;
  }

  /* Support only regular file */
  if ((st.st_mode & S_IFMT) != S_IFREG) {
    /* No permission to read other than regular file */
    status = SOUP_STATUS_FORBIDDEN;
    goto end;
  }

  /* Set content type and length */
  soup_message_headers_set_content_type (
      msg->response_headers, melo_http_server_get_content_type (path), NULL);
  soup_message_headers_set_content_length (msg->response_headers, st.st_size);

  /* Serve file if GET request */
  if (msg->method == SOUP_METHOD_GET) {
    GMappedFile *mapping;
    SoupBuffer *buffer;

    /* Map file into memory */
    mapping = g_mapped_file_new (path, FALSE, NULL);
    if (!mapping)
      goto end;

    /* Create a buffer to handle the file */
    buffer = soup_buffer_new_with_owner (g_mapped_file_get_contents (mapping),
        g_mapped_file_get_length (mapping), mapping,
        (GDestroyNotify) g_mapped_file_unref);

    /* Append buffer to message */
    soup_message_body_append_buffer (msg->response_body, buffer);
    soup_buffer_free (buffer);
  }

  /* Set status to OK */
  status = SOUP_STATUS_OK;

end:
  /* Set status */
  soup_message_set_status (msg, status);
}

/*
 * melo_httpd.c: HTTP server for Melo remote control
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

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <glib.h>
#include <glib/gstdio.h>

#include "melo_httpd.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

static int
melo_httpd_strcmp (gconstpointer a, gconstpointer b)
{
  const char **sa = (const char **) a;
  const char **sb = (const char **) b;

  return strcmp (*sa, *sb);
}

GString *
melo_httpd_list_directory (const char *f_path, const char *path)
{
  GPtrArray *entries;
  GString *list;
  GDir *dir;
  const gchar *d_name;
  char *escaped;
  int i;

  /* Create a new list for entries */
  entries = g_ptr_array_new ();

  /* Open directory to list */
  dir = g_dir_open (f_path, 0, NULL);
  if (dir) {
    /* List all entries in directory exceopt . and .. */
    while ((d_name = g_dir_read_name (dir))) {
      if ((!strcmp (d_name, "..") && !strcmp (f_path, "./")) ||
          !strcmp (d_name, "."))
        continue;

      /* Convert name for HTML */
      escaped = g_markup_escape_text (d_name, -1);

      /* Add to list */
      g_ptr_array_add (entries, escaped);
    }

    /* close directory */
    g_dir_close (dir);
  }

  /* Sort directory entries */
  g_ptr_array_sort (entries, (GCompareFunc) melo_httpd_strcmp);

  /* Generate HTML page for directory listing */
  list = g_string_new ("<html>\r\n");

  /* Add head and body */
  escaped = g_markup_escape_text (strchr (path, '/'), -1);
  g_string_append_printf (list, "<head><title>Index of %s</title></head>\r\n",
                          escaped);
  g_string_append_printf (list, "<body><h1>Index of %s</h1>\r\n<p>\r\n",
                          escaped);
  g_free (escaped);

  /* Fill with directory entries */
  for (i = 0; i < entries->len; i++) {
    g_string_append_printf (list, "<a href=\"%s%s\">%s</a><br>\r\n",
                            path,
                            (char *) entries->pdata[i],
                            (char *) entries->pdata[i]);
    g_free (entries->pdata[i]);
  }

  /* Finalize the HTML source */
  g_string_append (list, "</body>\r\n</html>\r\n");

  /* Free entries list */
  g_ptr_array_free (entries, TRUE);

  return list;
}

void
melo_httpd_default_handler (SoupServer *server, SoupMessage *msg,
                            const char *path, GHashTable *query,
                            SoupClientContext *client, gpointer user_data)
{
  GStatBuf st;
  char *f_path;

  /* We only support GET and HEAD methods */
  if (msg->method != SOUP_METHOD_GET && msg->method != SOUP_METHOD_HEAD)
    soup_message_set_status (msg, SOUP_STATUS_NOT_IMPLEMENTED);

  /* Generate absolute path in file system */
  f_path = g_strdup_printf (".%s", path);

  /* Check file status */
  if (g_stat (f_path, &st) == -1) {
    if (errno == EPERM)
      /* No permission to read */
      soup_message_set_status (msg, SOUP_STATUS_FORBIDDEN);
    else if (errno == ENOENT)
      /* No file or directory */
      soup_message_set_status (msg, SOUP_STATUS_NOT_FOUND);
    else
      /* Internal error */
      soup_message_set_status (msg, SOUP_STATUS_INTERNAL_SERVER_ERROR);

    /* Nothing else to do */
    g_free (f_path);
    return;
  }

  /* Path is a directory */
  if (g_file_test (f_path, G_FILE_TEST_IS_DIR)) {
    GString *list;
    char *index_path;
    char *slash;

    /* Redirect when slash is missing at end of the path */
    slash = strrchr (path, '/');
    if (!slash || slash[1]) {
      char *redir_uri;
      /* Generate new redirection URI */
      redir_uri = g_strdup_printf ("%s/", soup_message_get_uri (msg)->path);
      soup_message_set_redirect (msg, SOUP_STATUS_MOVED_PERMANENTLY, redir_uri);
      g_free (redir_uri);
      g_free (f_path);
      return;
    }

    /* Check if index.html exists */
    index_path = g_strdup_printf ("%s/index.html", f_path);
    if (g_stat (index_path, &st) == -1) {
      /* Index.html doesn't not exist: list directory */
      g_free (index_path);

      /* No index.html found: list files */
      list = melo_httpd_list_directory (f_path, path);
      soup_message_set_response (msg, "text/html", SOUP_MEMORY_TAKE,
                                 list->str, list->len);
      soup_message_set_status (msg, SOUP_STATUS_OK);
      g_string_free (list, FALSE);
      g_free (f_path);
      return;
    }

    /* Replace absolute path */
    g_free (f_path);
    f_path = index_path;
  }

  /* Check request method */
  if (msg->method == SOUP_METHOD_GET) {
    GMappedFile *mapping;
    SoupBuffer *buffer;

    /* Map file into memory */
    mapping = g_mapped_file_new (f_path, FALSE, NULL);
    if (!mapping) {
      soup_message_set_status (msg, SOUP_STATUS_INTERNAL_SERVER_ERROR);
      g_free (f_path);
      return;
    }

    /* Create a buffer to handle the file */
    buffer = soup_buffer_new_with_owner (g_mapped_file_get_contents (mapping),
                                         g_mapped_file_get_length (mapping),
                                         mapping,
                                         (GDestroyNotify) g_mapped_file_unref);

    /* Append buffer to message */
    soup_message_body_append_buffer (msg->response_body, buffer);
    soup_buffer_free (buffer);
  } else {
    char *length;

    /* Get file length and fill the Content-length header */
    length = g_strdup_printf ("%lu", (gulong) st.st_size);
    soup_message_headers_append (msg->response_headers,
                                 "Content-Length", length);
    g_free (length);
  }

  /* Set status to OK */
  soup_message_set_status (msg, SOUP_STATUS_OK);

  /* Free absolute path */
  g_free (f_path);
}

SoupServer *
melo_httpd_new (guint port)
{
  SoupServer *server;
  GError *err = NULL;
  gboolean res;

  /* Create a new HTTP server */
  server = soup_server_new (0, NULL);

  /* Start listening */
  res = soup_server_listen_all (server, port, 0, &err);
  if (res == FALSE) {
    g_clear_error (&err);
    g_object_unref(&server);
    return NULL;
  }

  /* Add a default handler */
  soup_server_add_handler (server, NULL, melo_httpd_default_handler, NULL,
                           NULL);

  return server;
}

void
melo_httpd_free (SoupServer *server)
{
  /* Disconnect all remaining clients */
  soup_server_disconnect (server);

  /* Free HTTP server */
  g_object_unref (server);
}

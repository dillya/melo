/*
 * melo_httpd_jsonrpc.h: JSON-RPC 2.0 handler for Melo HTTP server
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

#ifndef __MELO_HTTPD_JSONRPC_H__
#define __MELO_HTTPD_JSONRPC_H__

#include <glib.h>
#include <libsoup/soup.h>

void melo_httpd_jsonrpc_thread_handler (gpointer data, gpointer user_data);
void melo_httpd_jsonrpc_handler (SoupServer *server, SoupMessage *msg,
                                 const char *path, GHashTable *query,
                                 SoupClientContext *client, gpointer user_data);

#endif /* __MELO_HTTPD_JSONRPC_H__ */

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

#ifndef _MELO_HTTP_CLIENT_H_
#define _MELO_HTTP_CLIENT_H_

#include <stdbool.h>

#include <json-glib/json-glib.h>

G_BEGIN_DECLS

/**
 * MeloHttpClient:
 *
 * This object can create an asynchronous HTTP(s) client to connect and grab
 * data from HTTP(s) servers.
 */

#define MELO_TYPE_HTTP_CLIENT melo_http_client_get_type ()
G_DECLARE_FINAL_TYPE (
    MeloHttpClient, melo_http_client, MELO, HTTP_CLIENT, GObject)

/**
 * MeloHttpClientCb:
 * @client: the #MeloHttpClient
 * @status: the response status code
 * @data: (transfer none) (array length=size) (nullable): the body data
 * @size: the size of @data
 * @user_data: user data passed to the callback
 *
 * This function is called when a new response is received by the HTTP client.
 */
typedef void (*MeloHttpClientCb) (MeloHttpClient *client, unsigned int code,
    const char *data, size_t size, void *user_data);

/**
 * MeloHttpClientJsonCb:
 * @client: the #MeloHttpClient
 * @node: (transfer none): the json node
 * @user_data: user data passed to the callback
 *
 * This function is called when a new JSON response is received by the HTTP
 * client.
 */
typedef void (*MeloHttpClientJsonCb) (
    MeloHttpClient *client, JsonNode *node, void *user_data);

MeloHttpClient *melo_http_client_new (const char *user_agent);

bool melo_http_client_get (MeloHttpClient *client, const char *url,
    MeloHttpClientCb cb, void *user_data);
bool melo_http_client_get_json (MeloHttpClient *client, const char *url,
    MeloHttpClientJsonCb cb, void *user_data);

G_END_DECLS

#endif /* !_MELO_HTTP_CLIENT_H_ */

/*
 * Copyright (C) 2020 Alexandre Dilly <dillya@sparod.com>
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA.
 */

#ifndef _MEDIA_H_
#define _MEDIA_H_

#include <melo/melo_http_server.h>

void media_header_cb (MeloHttpServer *server,
    MeloHttpServerConnection *connection, const char *path, void *user_data);

void media_body_cb (MeloHttpServer *server,
    MeloHttpServerConnection *connection, const char *path, void *user_data);

void media_close_cb (MeloHttpServer *server,
    MeloHttpServerConnection *connection, void *user_data);

#endif /* !_MEDIA_H_ */

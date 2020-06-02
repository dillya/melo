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

#ifndef _SETTINGS_H_
#define _SETTINGS_H_

#include <melo/melo_http_server.h>

void settings_init (void);
void settings_deinit (void);

const char *settings_get_name (void);

bool settings_is_discover (void);

void settings_bind_http_server (MeloHttpServer *server);
void settings_get_http_ports (
    unsigned int *http_port, unsigned int *https_port);

#endif /* !_SETTINGS_H_ */

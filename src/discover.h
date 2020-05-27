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

#ifndef _DISCOVER_H_
#define _DISCOVER_H_

#include <stdbool.h>

void discover_init (void);
void discover_exit (void);

bool discover_register_device (
    const char *name, unsigned int http_port, unsigned https_port);
bool discover_unregister_device (void);

#endif /* !_DISCOVER_H_ */

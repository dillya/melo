/*
 * Copyright (C) 2022 Alexandre Dilly <dillya@sparod.com>
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

#ifndef _SYSTEM_H_
#define _SYSTEM_H_

#include <stddef.h>

#include <melo/melo_async.h>

bool system_handle_request (MeloMessage *msg, MeloAsyncCb cb, void *user_data);

#endif /* !_SYSTEM_H_ */

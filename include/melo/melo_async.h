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

#ifndef _MELO_ASYNC_H_
#define _MELO_ASYNC_H_

#include <stdbool.h>

#include <melo/melo_message.h>

typedef struct _MeloAsyncData MeloAsyncData;

/**
 * MeloAsyncCb:
 * @msg: the generated message
 * @user_data: user data passed to the callback
 *
 * This function is called when an asynchronous call has finished or when an
 * event occurs.
 *
 * If the function wants to keep a reference to the message after returning, it
 * must call melo_message_ref().
 *
 * For requests, @msg will be set to %NULL to signal the application that
 * request is finished. Then, the application can close / release layer used to
 * handle the request.
 *
 * Returns: %true if the message has been handled, %false otherwise.
 */
typedef bool (*MeloAsyncCb) (MeloMessage *msg, void *user_data);

/**
 * MeloAsyncData:
 * @cb: the #MeloAsyncCb callback
 * @user_data: the data to pass with @cb
 *
 * This structure can be used to handle an asynchronous callback and its user
 * data pointer in once.
 */
struct _MeloAsyncData {
  MeloAsyncCb cb;
  void *user_data;
};

#endif /* !_MELO_ASYNC_H_ */

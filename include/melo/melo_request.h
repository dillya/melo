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

#ifndef _MELO_REQUEST_H_
#define _MELO_REQUEST_H_

#include <stdbool.h>

#include <glib-object.h>

#include <melo/melo_async.h>
#include <melo/melo_message.h>

G_BEGIN_DECLS

/**
 * MeloRequest:
 *
 * Request handler.
 */

#define MELO_TYPE_REQUEST melo_request_get_type ()
G_DECLARE_FINAL_TYPE (MeloRequest, melo_request, MELO, REQUEST, GObject)

/**
 * MeloRequestCb:
 * @req: the #MeloRequest
 * @user_data: user data passed to the callback
 *
 * This function is called when the cancellation of the request is requested
 * from application layer or when the request is going to be destroyed.
 */
typedef void (*MeloRequestCb) (MeloRequest *req, void *user_data);

MeloRequest *melo_request_new (MeloAsyncData *async, GObject *obj);

/**
 * melo_request_ref:
 * @req: a #MeloRequest
 *
 * Increase the reference count on @req.
 *
 * Returns: the #MeloRequest
 */
static inline MeloRequest *
melo_request_ref (MeloRequest *req)
{
  return g_object_ref (req);
}

/**
 * melo_request_unref:
 * @req: (nullable): a #MeloRequest
 *
 * Releases a reference on @req. This may result in the request being freed.
 */
static inline void
melo_request_unref (MeloRequest *req)
{
  g_object_unref (req);
}

GObject *melo_request_get_object (MeloRequest *req);

void melo_request_set_user_data (MeloRequest *req, void *user_data);
void *melo_request_get_user_data (MeloRequest *req);

bool melo_request_send_response (MeloRequest *req, MeloMessage *msg);

void melo_request_cancel (MeloRequest *req);
void melo_request_complete (MeloRequest *req);

G_END_DECLS

#endif /* !_MELO_REQUEST_H_ */
